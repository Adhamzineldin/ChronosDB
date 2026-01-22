#include "storage/table/table_heap.h"
#include "buffer/page_guard.h"
#include "common/exception.h"

namespace francodb {
    // Constructor 1: Open existing
    TableHeap::TableHeap(BufferPoolManager *bpm, page_id_t first_page_id)
        : buffer_pool_manager_(bpm), first_page_id_(first_page_id) {
    }

    // Constructor 2: Create New
    // Note: Can't use PageGuard here since page doesn't exist yet
    // NewPage returns an already-pinned page that we must manually handle
    TableHeap::TableHeap(BufferPoolManager *bpm, Transaction *txn) : buffer_pool_manager_(bpm) {
        (void) txn;
        page_id_t new_page_id;
        Page *page = bpm->NewPage(&new_page_id);
        if (page == nullptr) throw Exception(ExceptionType::OUT_OF_RANGE, "Out of memory");

        // Lock, initialize, and unlock
        page->WLock();
        auto *table_page = reinterpret_cast<TablePage *>(page->GetData());
        table_page->Init(new_page_id, INVALID_PAGE_ID, INVALID_PAGE_ID, txn);
        first_page_id_ = new_page_id;
        page->WUnlock();

        bpm->UnpinPage(new_page_id, true);
    }

    // --- INSERT (Thread-Safe with RAII PageGuard) ---
    // Issue #1 Fix: Uses PageGuard for automatic pin/unpin on all paths
    // Issue #3 Fix: Releases latch before disk I/O (NewPage) to prevent latch convoy
    bool TableHeap::InsertTuple(const Tuple &tuple, RID *rid, Transaction *txn) {
        if (first_page_id_ == INVALID_PAGE_ID) return false;

        page_id_t curr_page_id = first_page_id_;
        
        while (true) {
            PageGuard guard(buffer_pool_manager_, curr_page_id, true);  // Write lock
            if (!guard.IsValid()) return false;
            
            auto *table_page = guard.As<TablePage>();

            // Try to insert into current page
            if (table_page->InsertTuple(tuple, rid, txn)) {
                guard.SetDirty();
                return true;  // PageGuard auto-releases
            }

            page_id_t next_page_id = table_page->GetNextPageId();

            if (next_page_id == INVALID_PAGE_ID) {
                // Need to create a new page
                // Issue #3 Fix: Release current latch BEFORE I/O
                page_id_t current_page = curr_page_id;
                guard.Release();  // Explicitly release to avoid holding latch during I/O
                
                // Allocate new page without holding any latch
                page_id_t new_page_id;
                Page *new_page_raw = buffer_pool_manager_->NewPage(&new_page_id);
                if (new_page_raw == nullptr) return false;
                
                // Re-acquire latches in order: current, then new
                PageGuard curr_guard(buffer_pool_manager_, current_page, true);
                if (!curr_guard.IsValid()) {
                    buffer_pool_manager_->UnpinPage(new_page_id, false);
                    return false;
                }
                
                // Check if another thread already added a page
                auto *curr_page_data = curr_guard.As<TablePage>();
                if (curr_page_data->GetNextPageId() != INVALID_PAGE_ID) {
                    // Another thread beat us - release new page and retry
                    buffer_pool_manager_->UnpinPage(new_page_id, false);
                    curr_page_id = curr_page_data->GetNextPageId();
                    continue;
                }
                
                // Lock and initialize the new page
                new_page_raw->WLock();
                auto *new_page = reinterpret_cast<TablePage *>(new_page_raw->GetData());
                new_page->Init(new_page_id, current_page, INVALID_PAGE_ID, txn);
                
                // Link pages and insert
                curr_page_data->SetNextPageId(new_page_id);
                new_page->InsertTuple(tuple, rid, txn);
                
                new_page_raw->WUnlock();
                buffer_pool_manager_->UnpinPage(new_page_id, true);
                curr_guard.SetDirty();
                return true;
            }

            // Move to next page (latch crabbing - current guard releases at end of iteration)
            curr_page_id = next_page_id;
        }
    }

    bool TableHeap::GetTuple(const RID &rid, Tuple *tuple, Transaction *txn) {
        // Issue #1 Fix: Use PageGuard for RAII-based pin/unpin
        // Prevents pin leaks if GetTuple throws an exception
        PageGuard guard(buffer_pool_manager_, rid.GetPageId(), false);  // Read lock
        if (!guard.IsValid()) return false;
        
        auto *table_page = guard.As<TablePage>();
        return table_page->GetTuple(rid, tuple, txn);
        // PageGuard destructor automatically unpins and unlocks
    }

    bool TableHeap::MarkDelete(const RID &rid, Transaction *txn) {
        // Issue #1 Fix: Use PageGuard for RAII-based pin/unpin
        PageGuard guard(buffer_pool_manager_, rid.GetPageId(), true);  // Write lock
        if (!guard.IsValid()) return false;
        
        auto *table_page = guard.As<TablePage>();
        bool result = table_page->MarkDelete(rid, txn);
        if (result) guard.SetDirty();
        return result;
        // PageGuard destructor automatically unpins and unlocks
    }

    bool TableHeap::UnmarkDelete(const RID &rid, Transaction *txn) {
        // Issue #1 Fix: Use PageGuard for RAII-based pin/unpin
        PageGuard guard(buffer_pool_manager_, rid.GetPageId(), true);  // Write lock
        if (!guard.IsValid()) return false;
        
        auto *table_page = guard.As<TablePage>();
        bool result = table_page->UnmarkDelete(rid, txn);
        if (result) guard.SetDirty();
        return result;
        // PageGuard destructor automatically unpins and unlocks
    }

    bool TableHeap::UpdateTuple(const Tuple &tuple, const RID &rid, Transaction *txn) {
        // Issue #1 Fix: Use PageGuard for RAII-based pin/unpin
        {
            PageGuard guard(buffer_pool_manager_, rid.GetPageId(), true);  // Write lock
            if (!guard.IsValid()) return false;
            
            auto *table_page = guard.As<TablePage>();
            bool is_deleted = table_page->MarkDelete(rid, txn);
            guard.SetDirty();
            
            if (!is_deleted) return false;
            // PageGuard destructor automatically unpins and unlocks
        }

        RID new_rid;
        return InsertTuple(tuple, &new_rid, txn);
    }

    page_id_t TableHeap::GetFirstPageId() const { return first_page_id_; }


    // Iterator implementation
    TableHeap::Iterator::Iterator(BufferPoolManager *bpm, page_id_t page_id,
                                  uint32_t slot_id, Transaction *txn, bool is_end)
        : bpm_(bpm), current_page_id_(page_id), current_slot_(slot_id),
          txn_(txn), is_end_(is_end), has_cached_tuple_(false) {
        if (!is_end_) {
            AdvanceToNextValidTuple();
        }
    }

    Tuple TableHeap::Iterator::operator*() {
        // If we have a cached tuple, return a copy
        if (has_cached_tuple_) {
            return cached_tuple_;
        }
        
        // Otherwise fetch it (shouldn't normally happen if using correctly)
        CacheTuple();
        return cached_tuple_;
    }

    void TableHeap::Iterator::CacheTuple() {
        if (is_end_ || current_page_id_ == INVALID_PAGE_ID) {
            has_cached_tuple_ = false;
            return;
        }
        
        // Use PageGuard for automatic pin/unpin (RAII - Issue #1 fix)
        PageGuard guard(bpm_, current_page_id_, false);  // Read lock
        if (!guard.IsValid()) {
            has_cached_tuple_ = false;
            return;
        }
        
        auto *table_page = guard.As<TablePage>();
        RID rid(current_page_id_, current_slot_);
        
        has_cached_tuple_ = table_page->GetTuple(rid, &cached_tuple_, txn_);
        // PageGuard destructor automatically unpins
    }

    TableHeap::Iterator &TableHeap::Iterator::operator++() {
        current_slot_++;
        has_cached_tuple_ = false;  // Invalidate cache
        AdvanceToNextValidTuple();
        return *this;
    }

    bool TableHeap::Iterator::operator!=(const Iterator &other) const {
        return is_end_ != other.is_end_ ||
               current_page_id_ != other.current_page_id_ ||
               current_slot_ != other.current_slot_;
    }

    void TableHeap::Iterator::AdvanceToNextValidTuple() {
        while (current_page_id_ != INVALID_PAGE_ID) {
            // Use PageGuard for automatic pin/unpin (RAII - Issue #1 fix)
            PageGuard guard(bpm_, current_page_id_, false);  // Read lock
            if (!guard.IsValid()) {
                is_end_ = true;
                has_cached_tuple_ = false;
                return;
            }

            auto *table_page = guard.As<TablePage>();
            uint32_t tuple_count = table_page->GetTupleCount();

            // Find next valid tuple in current page
            while (current_slot_ < tuple_count) {
                RID rid(current_page_id_, current_slot_);
                if (table_page->GetTuple(rid, &cached_tuple_, txn_)) {
                    has_cached_tuple_ = true;
                    return; // Found valid tuple and cached it - guard auto-unpins
                }
                current_slot_++;
            }

            // Move to next page
            page_id_t next_page = table_page->GetNextPageId();
            current_page_id_ = next_page;
            current_slot_ = 0;
            // guard auto-unpins here when it goes out of scope
        }

        is_end_ = true;
        has_cached_tuple_ = false;
    }

    TableHeap::Iterator TableHeap::Begin(Transaction *txn) {
        return Iterator(buffer_pool_manager_, first_page_id_, 0, txn, false);
    }

    TableHeap::Iterator TableHeap::End() {
        return Iterator(buffer_pool_manager_, INVALID_PAGE_ID, 0, nullptr, true);
    }
    
    // ========================================================================
    // INTERFACE COMPLIANCE (Issue #13 - Dependency Inversion Principle)
    // ========================================================================
    
    /**
     * TableHeapIteratorAdapter - Bridges TableHeap::Iterator to ITableStorage::Iterator
     * This allows polymorphic usage through the ITableStorage interface.
     */
    class TableHeapIteratorAdapter : public ITableStorage::Iterator {
    public:
        TableHeapIteratorAdapter(TableHeap::Iterator begin, TableHeap::Iterator end)
            : current_(std::move(begin)), end_(std::move(end)) {}
        
        bool IsEnd() const override { 
            return !(current_ != end_); 
        }
        
        void Next() override { 
            ++current_; 
        }
        
        Tuple GetTuple() const override { 
            return current_.GetCurrentTuple(); 
        }
        
        RID GetRID() const override { 
            return current_.GetRID(); 
        }
        
    private:
        TableHeap::Iterator current_;
        TableHeap::Iterator end_;
    };
    
    std::unique_ptr<ITableStorage::Iterator> TableHeap::CreateIterator(Transaction* txn) {
        return std::make_unique<TableHeapIteratorAdapter>(Begin(txn), End());
    }
} // namespace francodb

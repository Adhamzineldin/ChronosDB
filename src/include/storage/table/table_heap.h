#pragma once

#include "storage/table/table_page.h"
#include "storage/table/tuple.h"
#include "concurrency/transaction.h"
#include "common/exception.h"
#include "storage/storage_interface.h"

namespace chronosdb {

    // Forward declaration
    class BufferPoolManager;

    /**
     * TableHeap represents a physical table on disk.
     * It is a doubly-linked list of TablePages.
     * 
     * SOLID COMPLIANCE (Issue #13 Fix):
     * Implements ITableStorage interface for Dependency Inversion Principle.
     * Executors can depend on ITableStorage abstraction rather than concrete TableHeap.
     */
    class TableHeap : public ITableStorage {
    public:
        // Accept IBufferManager interface for polymorphic buffer pool usage
        TableHeap(IBufferManager *bpm, page_id_t first_page_id);
        
        TableHeap(IBufferManager *bpm, Transaction *txn = nullptr);
        
        // Virtual destructor for interface
        ~TableHeap() override = default;

        // Insert a tuple -> Returns true if success
        bool InsertTuple(const Tuple &tuple, RID *rid, Transaction *txn) override;

        // Mark a tuple as deleted
        bool MarkDelete(const RID &rid, Transaction *txn) override;
        
        // Unmark a tuple as deleted (for rollback)
        bool UnmarkDelete(const RID &rid, Transaction *txn) override;

        // Update a tuple (Delete Old + Insert New)
        bool UpdateTuple(const Tuple &tuple, const RID &rid, Transaction *txn) override;

        // Read a tuple
        bool GetTuple(const RID &rid, Tuple *tuple, Transaction *txn) override;

        // Get the ID of the first page (useful for scanning)
        page_id_t GetFirstPageId() const override;
        
        
        
        class Iterator {
        public:
            Iterator(IBufferManager *bpm, page_id_t page_id, uint32_t slot_id, 
                     Transaction *txn, bool is_end = false);
            
            /**
             * Dereference operator - returns copy of current tuple
             * Consider using GetCurrentTuple() for reference access
             */
            Tuple operator*();
            
            /**
             * Get reference to current tuple (avoids copy)
             * Valid until Next() is called
             */
            const Tuple& GetCurrentTuple() const { return cached_tuple_; }
            
            /**
             * Extract tuple with move semantics (most efficient)
             * Iterator must be advanced after calling this
             */
            Tuple ExtractTuple() { return std::move(cached_tuple_); }
            
            Iterator& operator++();
            bool operator!=(const Iterator &other) const;
            RID GetRID() const { return RID(current_page_id_, current_slot_); }
            
            /**
             * Check if iterator has a valid cached tuple
             */
            bool HasCachedTuple() const { return has_cached_tuple_; }

        private:
            void AdvanceToNextValidTuple();
            void CacheTuple();  // Load current tuple into cache
            
            IBufferManager *bpm_;
            page_id_t current_page_id_;
            uint32_t current_slot_;
            Transaction *txn_;
            bool is_end_;
            
            // Cached tuple to avoid repeated reads
            Tuple cached_tuple_;
            bool has_cached_tuple_ = false;
        };

        Iterator Begin(Transaction *txn = nullptr);
        Iterator End();
        
        // Interface compliance - CreateIterator returns a polymorphic iterator
        // Note: For now, use Begin/End directly for best performance
        std::unique_ptr<ITableStorage::Iterator> CreateIterator(Transaction* txn) override;

    private:
        IBufferManager *buffer_pool_manager_;
        page_id_t first_page_id_;
    };

} // namespace chronosdb
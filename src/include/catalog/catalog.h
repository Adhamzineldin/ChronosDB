#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <mutex> // <--- NEW
#include <atomic> // <--- NEW

#include "catalog/table_metadata.h"
#include "buffer/buffer_pool_manager.h"
#include "storage/page/free_page_manager.h"

namespace francodb {

    class Catalog {
    public:
        // Initialize OID counter to 0
        Catalog(BufferPoolManager *bpm) : bpm_(bpm), next_table_oid_(0) {}

        /**
         * Create a new table.
         * Returns pointers to the metadata, or nullptr if it exists.
         */
        TableMetadata *CreateTable(const std::string &table_name, const Schema &schema) {
            std::lock_guard<std::mutex> lock(latch_); // <--- THREAD SAFETY

            if (names_to_oid_.count(table_name) > 0) {
                return nullptr; // Table already exists
            }

            // 1. Assign a unique ID
            uint32_t table_oid = next_table_oid_++;

            // 2. Create physical storage
            auto table_heap = std::make_unique<TableHeap>(bpm_);
            page_id_t first_page_id = table_heap->GetFirstPageId();

            // 3. Create metadata with the new OID
            auto metadata = std::make_unique<TableMetadata>(
                schema, table_name, std::move(table_heap), first_page_id, table_oid);

            // 4. Update Maps
            TableMetadata *ptr = metadata.get();
            tables_[table_oid] = std::move(metadata);
            names_to_oid_[table_name] = table_oid;

            return ptr;
        }

        /**
         * Get table by Name (Used by Parser/Binder)
         */
        TableMetadata *GetTable(const std::string &table_name) {
            std::lock_guard<std::mutex> lock(latch_);
            if (names_to_oid_.count(table_name) == 0) {
                return nullptr;
            }
            uint32_t oid = names_to_oid_[table_name];
            return tables_[oid].get();
        }

        /**
         * Get table by OID (Used by internal Executors/Indexes)
         */
        TableMetadata *GetTable(uint32_t table_oid) {
            std::lock_guard<std::mutex> lock(latch_);
            if (tables_.count(table_oid) == 0) {
                return nullptr;
            }
            return tables_[table_oid].get();
        }
        
        /**
         * Drop Table (S-Grade: Reclaims disk space)
         */
        bool DropTable(const std::string &table_name) {
            std::lock_guard<std::mutex> lock(latch_);
            
            if (names_to_oid_.count(table_name) == 0) return false;

            uint32_t oid = names_to_oid_[table_name];
            TableMetadata *info = tables_[oid].get();

            // 1. Fetch the Bitmap Page
            Page *bitmap_page = bpm_->FetchPage(FreePageManager::BITMAP_PAGE_ID);
            char *bitmap_data = bitmap_page->GetData();

            // 2. Traverse TableHeap and free pages
            page_id_t curr_id = info->first_page_id_;
            while (curr_id != INVALID_PAGE_ID) {
                Page *p = bpm_->FetchPage(curr_id);
                if (p == nullptr) break; 
                
                auto *table_p = reinterpret_cast<TablePage *>(p->GetData());
                page_id_t next_id = table_p->GetNextPageId();
        
                // RECLAIM DISK SPACE
                FreePageManager::DeallocatePage(bitmap_data, curr_id);
        
                bpm_->UnpinPage(curr_id, false);
                curr_id = next_id;
            }

            bpm_->UnpinPage(FreePageManager::BITMAP_PAGE_ID, true); // Dirty = true

            // 3. Erase from maps
            names_to_oid_.erase(table_name);
            tables_.erase(oid);
            
            return true;
        }

    private:
        BufferPoolManager *bpm_;
        std::mutex latch_; // Protects the maps
        std::atomic<uint32_t> next_table_oid_;

        // Map: Table OID -> Metadata (Owner of the pointer)
        std::unordered_map<uint32_t, std::unique_ptr<TableMetadata>> tables_;
        
        // Map: Table Name -> Table OID (Lookup helper)
        std::unordered_map<std::string, uint32_t> names_to_oid_;
    };

} // namespace francodb
#pragma once

#include <unordered_map>
#include <string>
#include <memory>

#include "catalog/table_metadata.h"
#include "buffer/buffer_pool_manager.h"
#include "storage/page/free_page_manager.h"

namespace francodb {

    class Catalog {
    public:
        Catalog(BufferPoolManager *bpm) : bpm_(bpm) {}

        // CREATE TABLE ESM (id RAKAM, ...)
        bool CreateTable(const std::string &table_name, const Schema &schema) {
            if (names_to_metadata_.find(table_name) != names_to_metadata_.end()) {
                return false; // Table already exists
            }

            // 1. Create the physical storage (TableHeap)
            auto table_heap = std::make_unique<TableHeap>(bpm_);
            page_id_t first_page_id = table_heap->GetFirstPageId();

            // 2. Create the metadata
            auto metadata = std::make_unique<TableMetadata>(schema, table_name, std::move(table_heap), first_page_id);

            // 3. Register in our maps
            names_to_metadata_[table_name] = std::move(metadata);
            return true;
        }

        // Lookup a table by name
        TableMetadata* GetTable(const std::string &table_name) {
            if (names_to_metadata_.find(table_name) == names_to_metadata_.end()) {
                return nullptr;
            }
            return names_to_metadata_[table_name].get();
        }
        
        
        // DROP TABLE ESM
        bool DropTable(const std::string &table_name) {
            auto it = names_to_metadata_.find(table_name);
            if (it == names_to_metadata_.end()) return false;

            // 1. Fetch the Bitmap Page from Buffer Pool
            Page *bitmap_page = bpm_->FetchPage(FreePageManager::BITMAP_PAGE_ID);
            char *bitmap_data = bitmap_page->GetData();

            // 2. Traverse the TableHeap and free every page
            page_id_t curr_id = it->second->first_page_id_;
            while (curr_id != INVALID_PAGE_ID) {
                Page *p = bpm_->FetchPage(curr_id);
                auto *table_p = reinterpret_cast<TablePage *>(p->GetData());
                page_id_t next_id = table_p->GetNextPageId();
        
                // RECLAIM DISK SPACE
                FreePageManager::DeallocatePage(bitmap_data, curr_id);
        
                bpm_->UnpinPage(curr_id, false);
                // In a real system, you'd mark the page as "deleted" in BPM too
                curr_id = next_id;
            }

            // 3. Finalize
            bpm_->UnpinPage(FreePageManager::BITMAP_PAGE_ID, true); // Dirty = true
            names_to_metadata_.erase(table_name);
            return true;
        }

    private:
        BufferPoolManager *bpm_;
        std::unordered_map<std::string, std::unique_ptr<TableMetadata>> names_to_metadata_;
    };

} // namespace francodb
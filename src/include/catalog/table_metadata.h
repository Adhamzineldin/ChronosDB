#pragma once

#include <string>
#include <memory>
#include "storage/table/schema.h"
#include "storage/table/table_heap.h"

namespace francodb {

    struct TableMetadata {
        TableMetadata(Schema schema, std::string name, std::unique_ptr<TableHeap> &&table_heap, page_id_t first_page_id, uint32_t oid)
            : schema_(std::move(schema)), 
              name_(std::move(name)), 
              table_heap_(std::move(table_heap)), 
              first_page_id_(first_page_id),
              oid_(oid) {} // <--- NEW

        Schema schema_;
        std::string name_;
        std::unique_ptr<TableHeap> table_heap_;
        page_id_t first_page_id_;
        uint32_t oid_; // <--- NEW: Unique ID for this table (0, 1, 2...)
    };

} // namespace francodb
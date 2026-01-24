#pragma once

#include <string>
#include <memory>
#include <vector>
#include "storage/table/schema.h"
#include "storage/table/table_heap.h"
#include "parser/statement.h"
#include "recovery/log_record.h"

namespace francodb {

    struct TableMetadata {
        TableMetadata(Schema schema, std::string name, std::unique_ptr<TableHeap> &&table_heap, page_id_t first_page_id, uint32_t oid)
            : schema_(std::move(schema)), 
              name_(std::move(name)), 
              table_heap_(std::move(table_heap)), 
              first_page_id_(first_page_id),
              oid_(oid),
              last_checkpoint_lsn_(LogRecord::INVALID_LSN),
              checkpoint_page_id_(INVALID_PAGE_ID) {}

        Schema schema_;
        std::string name_;
        std::unique_ptr<TableHeap> table_heap_;
        page_id_t first_page_id_;
        uint32_t oid_;
        std::vector<CreateStatement::ForeignKey> foreign_keys_;
        
        // ========================================================================
        // CHECKPOINT-BASED TIME TRAVEL (Bug #6 Optimization)
        // ========================================================================
        // Instead of replaying from LSN 0, we store:
        // 1. The LSN of the last checkpoint for this table
        // 2. A snapshot page that stores the table state at that checkpoint
        // When doing time travel, we only replay the delta from the nearest checkpoint
        
        LogRecord::lsn_t last_checkpoint_lsn_;   // LSN of last checkpoint for this table
        page_id_t checkpoint_page_id_;           // Page storing checkpoint snapshot (or INVALID)
        
        void SetCheckpointLSN(LogRecord::lsn_t lsn) { last_checkpoint_lsn_ = lsn; }
        LogRecord::lsn_t GetCheckpointLSN() const { return last_checkpoint_lsn_; }
    };

} // namespace francodb
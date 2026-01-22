#include "execution/executors/delete_executor.h"
#include "execution/predicate_evaluator.h"
#include "common/exception.h"
#include "catalog/index_info.h"
#include "storage/index/index_key.h"
#include "storage/table/table_page.h"
#include "buffer/buffer_pool_manager.h"
#include "storage/page/page.h"
#include <iostream>
#include <cmath>

namespace francodb {

void DeleteExecutor::Init() {
    table_info_ = exec_ctx_->GetCatalog()->GetTable(plan_->table_name_);
    if (table_info_ == nullptr) {
        throw Exception(ExceptionType::EXECUTION, "Table not found: " + plan_->table_name_);
    }
}

bool DeleteExecutor::Next(Tuple *tuple) {
    (void)tuple; // Unused
    if (is_finished_) return false;

    // 1. Gather all RIDs and tuples to delete
    std::vector<std::pair<RID, Tuple>> tuples_to_delete;
    
    // --- SCAN LOGIC (Similar to SeqScan) ---
    page_id_t curr_page_id = table_info_->first_page_id_;
    auto bpm = exec_ctx_->GetBufferPoolManager();

    while (curr_page_id != INVALID_PAGE_ID) {
        Page *page = bpm->FetchPage(curr_page_id);
        if (page == nullptr) {
            break; // Skip if page fetch fails
        }
        auto *table_page = reinterpret_cast<TablePage *>(page->GetData());
        
        // Loop over all slots in page
        for (uint32_t i = 0; i < table_page->GetTupleCount(); ++i) {
            RID rid(curr_page_id, i);
            Tuple t;
            
            // Read tuple (if it exists)
            if (table_page->GetTuple(rid, &t, nullptr)) {
                // Check WHERE clause
                if (EvaluatePredicate(t)) {
                    tuples_to_delete.push_back({rid, t});
                }
            }
        }
        page_id_t next = table_page->GetNextPageId();
        bpm->UnpinPage(curr_page_id, false);
        curr_page_id = next;
    }

    // 2. Perform Deletes (with index updates)
    int count = 0;
    for (const auto &pair : tuples_to_delete) {
        const RID &rid = pair.first;
        const Tuple &tuple = pair.second;
        
        // Verify the tuple still exists (might have been deleted by another thread)
        Tuple verify_tuple;
        if (!table_info_->table_heap_->GetTuple(rid, &verify_tuple, txn_)) {
            // Tuple was already deleted, skip
            continue;
        }
        
        // Verify it still matches the predicate
        if (!EvaluatePredicate(verify_tuple)) {
            // Tuple no longer matches predicate, skip
            continue;
        }
        
        // Track modification for rollback
        if (txn_) {
            txn_->AddModifiedTuple(rid, verify_tuple, true, plan_->table_name_); // true = is_deleted
        }
        
        // A. Remove from indexes BEFORE deleting from table
        auto indexes = exec_ctx_->GetCatalog()->GetTableIndexes(plan_->table_name_);
        for (auto *index : indexes) {
            int col_idx = table_info_->schema_.GetColIdx(index->col_name_);
            Value key_val = tuple.GetValue(table_info_->schema_, col_idx);
            
            GenericKey<8> key;
            key.SetFromValue(key_val);
            
            index->b_plus_tree_->Remove(key, txn_);
        }
        
        // B. Delete from table (only if not already deleted)
        bool deleted = table_info_->table_heap_->MarkDelete(rid, txn_);
        if (deleted && txn_) {
            txn_->AddModifiedTuple(rid, tuple, true, plan_->table_name_);
            
            if (exec_ctx_->GetLogManager()) {
                // Serialize ALL values as a pipe-separated string for complete tuple recovery
                std::string tuple_str;
                for (uint32_t i = 0; i < table_info_->schema_.GetColumnCount(); i++) {
                    if (i > 0) tuple_str += "|";
                    tuple_str += tuple.GetValue(table_info_->schema_, i).ToString();
                }
                Value old_val(TypeId::VARCHAR, tuple_str);
            
                LogRecord log_rec(txn_->GetTransactionId(), txn_->GetPrevLSN(), 
                                  LogRecordType::APPLY_DELETE, plan_->table_name_, old_val);
                auto lsn = exec_ctx_->GetLogManager()->AppendLogRecord(log_rec);
                txn_->SetPrevLSN(lsn);
            }
        }
    }

    // Logging removed to avoid interleaved output during concurrent operations
    is_finished_ = true;
    return false;
}

const Schema *DeleteExecutor::GetOutputSchema() {
    return &table_info_->schema_;
}

bool DeleteExecutor::EvaluatePredicate(const Tuple &tuple) {
    // Issue #12 Fix: Use shared PredicateEvaluator to eliminate code duplication
    return PredicateEvaluator::Evaluate(tuple, table_info_->schema_, plan_->where_clause_);
}

} // namespace francodb


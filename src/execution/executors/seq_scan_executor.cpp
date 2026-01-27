#include "execution/executors/seq_scan_executor.h"
#include "execution/executor_context.h"
#include "execution/predicate_evaluator.h"
#include "common/type.h"

namespace chronosdb {

    void SeqScanExecutor::Init() {
        // 1. Determine which Table Heap to scan (Live vs Time Travel)
        if (table_heap_override_ != nullptr) {
            // TIME TRAVEL MODE
            active_heap_ = table_heap_override_;
            
            // Get schema from catalog, but data from snapshot
            table_info_ = exec_ctx_->GetCatalog()->GetTable(plan_->table_name_);
        } else {
            // LIVE MODE
            table_info_ = exec_ctx_->GetCatalog()->GetTable(plan_->table_name_);
            if (!table_info_) {
                throw Exception(ExceptionType::CATALOG, "Table not found: " + plan_->table_name_);
            }
            active_heap_ = table_info_->table_heap_.get();
        }

        // 2. Initialize the Iterator
        iter_ = active_heap_->Begin(txn_);
    }

    bool SeqScanExecutor::Next(Tuple *tuple) {
        // Use the Iterator to traverse the table
        // This abstracts away page fetching, pinning, and slot logic
        while (iter_ != active_heap_->End()) {
            
            // Get reference to cached tuple (avoids copy)
            const Tuple& candidate_tuple = iter_.GetCurrentTuple();
            
            // Apply WHERE clause
            if (EvaluatePredicate(candidate_tuple)) {
                // Use move semantics when extracting the tuple
                *tuple = iter_.ExtractTuple();
                ++iter_;
                return true;
            }
            
            // Move iterator forward (handles jumping across pages)
            ++iter_;
        }
        
        return false; // End of Scan
    }

    const Schema *SeqScanExecutor::GetOutputSchema() {
        if (!table_info_) {
             table_info_ = exec_ctx_->GetCatalog()->GetTable(plan_->table_name_);
        }
        return &table_info_->schema_;
    }

    bool SeqScanExecutor::EvaluatePredicate(const Tuple &tuple) {
        // Issue #12 Fix: Use shared PredicateEvaluator to eliminate code duplication
        // The same logic is needed in SeqScanExecutor, DeleteExecutor, UpdateExecutor
        return PredicateEvaluator::Evaluate(tuple, table_info_->schema_, plan_->where_clause_);
    }

} // namespace chronosdb
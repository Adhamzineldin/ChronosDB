#pragma once

#include <memory>
#include <vector>
#include "execution/executors/abstract_executor.h"
#include "parser/statement.h"
#include "storage/table/tuple.h"
#include "common/exception.h"
#include "catalog/index_info.h"

namespace francodb {

    class InsertExecutor : public AbstractExecutor {
    public:
        InsertExecutor(ExecutorContext *exec_ctx, InsertStatement *plan)
            : AbstractExecutor(exec_ctx), 
              plan_(plan), 
              table_info_(nullptr) {} // <--- FIX 2: Initialize pointer

        void Init() override {
            // 1. Look up the table in the Catalog
            table_info_ = exec_ctx_->GetCatalog()->GetTable(plan_->table_name_);
            if (table_info_ == nullptr) {
                throw Exception(ExceptionType::EXECUTION, "Table not found: " + plan_->table_name_);
            }
        }

        // Insert executes everything in one go, then returns false.
        bool Next(Tuple *tuple) override {
            (void)tuple;
            if (is_finished_) return false;

            Tuple to_insert(plan_->values_, table_info_->schema_);
            RID rid;
            bool success = table_info_->table_heap_->InsertTuple(to_insert, &rid, nullptr);
            if (!success) throw Exception(ExceptionType::EXECUTION, "Failed to insert tuple");

            // --- UPDATE INDEXES ---
            auto indexes = exec_ctx_->GetCatalog()->GetTableIndexes(plan_->table_name_);
            for (auto *index : indexes) {
                int col_idx = table_info_->schema_.GetColIdx(index->col_name_);
                Value key_val = to_insert.GetValue(table_info_->schema_, col_idx);
             
                GenericKey<8> key;
                key.SetFromValue(key_val); 
             
                index->b_plus_tree_->Insert(key, rid, nullptr);
            }
            // ----------------------

            is_finished_ = true;
            return false;
        }

        const Schema *GetOutputSchema() override { return &table_info_->schema_; }

    private:
        InsertStatement *plan_;
        TableMetadata *table_info_;
        bool is_finished_ = false;
    };

} // namespace francodb
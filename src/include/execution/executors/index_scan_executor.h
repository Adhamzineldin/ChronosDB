#pragma once

#include <vector>
#include "execution/executors/abstract_executor.h"
#include "parser/statement.h"
#include "storage/table/tuple.h"
#include "catalog/index_info.h"
#include "storage/index/index_key.h"

namespace francodb {

class IndexScanExecutor : public AbstractExecutor {
public:
    IndexScanExecutor(ExecutorContext *exec_ctx, SelectStatement *plan, IndexInfo *index_info, Value lookup_value)
        : AbstractExecutor(exec_ctx), 
          plan_(plan), 
          index_info_(index_info), 
          lookup_value_(lookup_value),
          table_info_(nullptr) {}

    void Init() override {
        // 1. Get Table Metadata (so we can fetch the actual data later)
        table_info_ = exec_ctx_->GetCatalog()->GetTable(plan_->table_name_);
        
        // 2. Convert the Lookup Value (from WHERE clause) to a GenericKey
        GenericKey<8> key;
        key.SetFromValue(lookup_value_);

        // 3. Ask the B+Tree for the RIDs
        // The tree returns a list of RIDs matching this key.
        result_rids_.clear();
        index_info_->b_plus_tree_->GetValue(key, &result_rids_);

        // 4. Reset iterator
        cursor_ = 0;
    }

    bool Next(Tuple *tuple) override {
        // Loop through the results found by the B+Tree
        if (cursor_ < result_rids_.size()) {
            RID rid = result_rids_[cursor_];
            cursor_++;

            // 5. FETCH THE ACTUAL TUPLE
            // We have the address (RID), now go get the data from the Heap.
            bool success = table_info_->table_heap_->GetTuple(rid, tuple, nullptr);
            if (!success) {
                // This shouldn't happen unless the index is out of sync
                return false; 
            }
            return true;
        }
        
        return false; // No more matches
    }

    const Schema *GetOutputSchema() override { return &table_info_->schema_; }

private:
    SelectStatement *plan_;
    IndexInfo *index_info_;
    Value lookup_value_;     // The value we are searching for (e.g., 100)
    
    TableMetadata *table_info_;
    std::vector<RID> result_rids_; // The "Hit List" returned by the Index
    size_t cursor_ = 0;
};

} // namespace francodb
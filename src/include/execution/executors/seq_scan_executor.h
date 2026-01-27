#pragma once

#include <vector>
#include "execution/executors/abstract_executor.h"
#include "parser/statement.h"
#include "storage/table/tuple.h"
#include "storage/table/table_heap.h" // Required for Iterator
#include "common/rid.h"
#include "common/exception.h"

namespace chronosdb {

    class SeqScanExecutor : public AbstractExecutor {
    public:
        // [MODIFIED] Added 'table_heap_override'
        SeqScanExecutor(ExecutorContext *exec_ctx, SelectStatement *plan, 
                        Transaction *txn = nullptr, TableHeap *table_heap_override = nullptr)
            : AbstractExecutor(exec_ctx), 
              plan_(plan), 
              table_info_(nullptr),
              txn_(txn),
              table_heap_override_(table_heap_override), // Store the shadow heap
              iter_(nullptr, INVALID_PAGE_ID, 0, nullptr, true) // Init empty iterator
              {}

        void Init() override;
        bool Next(Tuple *tuple) override;
        const Schema *GetOutputSchema() override;

    private:
        bool EvaluatePredicate(const Tuple &tuple);

        SelectStatement *plan_;
        TableMetadata *table_info_;
        Transaction *txn_;
    
        // [NEW] Supports Time Travel
        TableHeap *table_heap_override_; 
        TableHeap *active_heap_; // Points to either override or catalog heap
    
        // Use the Iterator we built earlier
        TableHeap::Iterator iter_; 
    };

} // namespace chronosdb
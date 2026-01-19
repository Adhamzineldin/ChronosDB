#pragma once

#include <vector>
#include <map>
#include <memory>
#include "execution/executors/abstract_executor.h"
#include "parser/advanced_statements.h"
#include "storage/table/tuple.h"

namespace francodb {

    /**
     * AggregationExecutor: Handles GROUP BY and aggregate functions
     * Follows Single Responsibility Principle - only handles aggregation
     * Supports COUNT, SUM, AVG, MIN, MAX aggregates
     */
    class AggregationExecutor : public AbstractExecutor {
    public:
        AggregationExecutor(ExecutorContext *exec_ctx, SelectStatementWithJoins *plan,
                           AbstractExecutor *child_executor, Transaction *txn = nullptr)
            : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(child_executor), 
              txn_(txn), finished_(false) {}

        ~AggregationExecutor() override {
            if (child_executor_) delete child_executor_;
        }

        void Init() override;
        bool Next(Tuple *tuple) override;
        const Schema *GetOutputSchema() override;

    private:
        SelectStatementWithJoins *plan_;
        AbstractExecutor *child_executor_;
        Transaction *txn_;
        bool finished_;

        // Aggregate state
        struct AggregateGroup {
            std::vector<Value> group_keys;
            std::map<std::string, Value> aggregate_values;  // func_name -> result
        };

        std::vector<AggregateGroup> groups_;
        size_t current_group_index_;
        std::shared_ptr<Schema> output_schema_;

        // Helpers
        std::vector<Value> ExtractGroupKeys(const Tuple &tuple) const;
        void UpdateAggregate(AggregateGroup &group, const Tuple &tuple);
        Tuple BuildOutputTuple(const AggregateGroup &group) const;
    };

    /**
     * SortExecutor: Implements ORDER BY functionality
     * Uses merge sort internally for stable sorting
     * Follows Strategy Pattern
     */
    class SortExecutor : public AbstractExecutor {
    public:
        SortExecutor(ExecutorContext *exec_ctx, SelectStatementWithJoins *plan,
                    AbstractExecutor *child_executor, Transaction *txn = nullptr)
            : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(child_executor),
              txn_(txn), current_index_(0) {}

        ~SortExecutor() override {
            if (child_executor_) delete child_executor_;
        }

        void Init() override;
        bool Next(Tuple *tuple) override;
        const Schema *GetOutputSchema() override;

    private:
        SelectStatementWithJoins *plan_;
        AbstractExecutor *child_executor_;
        Transaction *txn_;
        
        std::vector<Tuple> sorted_tuples_;
        size_t current_index_;
        std::shared_ptr<Schema> output_schema_;

        // Helpers
        bool CompareTuples(const Tuple &a, const Tuple &b) const;
        int CompareValues(const Value &a, const Value &b) const;
    };

    /**
     * LimitExecutor: Implements LIMIT and OFFSET
     * Simple executor that filters out rows beyond limit
     * Follows Decorator Pattern
     */
    class LimitExecutor : public AbstractExecutor {
    public:
        LimitExecutor(ExecutorContext *exec_ctx, SelectStatementWithJoins *plan,
                     AbstractExecutor *child_executor, Transaction *txn = nullptr)
            : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(child_executor),
              txn_(txn), row_count_(0), offset_(plan->offset_), limit_(plan->limit_) {}

        ~LimitExecutor() override {
            if (child_executor_) delete child_executor_;
        }

        void Init() override;
        bool Next(Tuple *tuple) override;
        const Schema *GetOutputSchema() override;

    private:
        SelectStatementWithJoins *plan_;
        AbstractExecutor *child_executor_;
        Transaction *txn_;
        uint32_t row_count_;
        uint32_t offset_;
        uint32_t limit_;

        bool ShouldSkipRow() const { return row_count_ < offset_; }
        bool HasReachedLimit() const { return limit_ > 0 && (row_count_ - offset_) >= limit_; }
    };

    /**
     * DistinctExecutor: Removes duplicate rows (SELECT DISTINCT)
     * Uses a set to track seen tuples
     */
    class DistinctExecutor : public AbstractExecutor {
    public:
        DistinctExecutor(ExecutorContext *exec_ctx, SelectStatement *plan,
                        AbstractExecutor *child_executor, Transaction *txn = nullptr)
            : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(child_executor), txn_(txn) {}

        ~DistinctExecutor() override {
            if (child_executor_) delete child_executor_;
        }

        void Init() override;
        bool Next(Tuple *tuple) override;
        const Schema *GetOutputSchema() override;

    private:
        SelectStatement *plan_;
        AbstractExecutor *child_executor_;
        Transaction *txn_;
        
        std::set<std::string> seen_tuples_;  // Hash of seen tuples
        std::shared_ptr<Schema> output_schema_;

        std::string TupleToHash(const Tuple &tuple) const;
    };

} // namespace francodb


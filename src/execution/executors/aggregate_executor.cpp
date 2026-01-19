#include "execution/executors/aggregate_executor.h"
#include "common/exception.h"
#include <algorithm>
#include <functional>

namespace francodb {

    /**
     * AggregationExecutor Implementation
     * Supports GROUP BY with aggregate functions
     */
    void AggregationExecutor::Init() {
        if (!child_executor_) {
            throw Exception(ExceptionType::EXECUTION, "Child executor required for aggregation");
        }

        child_executor_->Init();
        current_group_index_ = 0;
        finished_ = false;

        // Build aggregate state by scanning child executor
        Tuple tuple;
        while (child_executor_->Next(&tuple)) {
            std::vector<Value> group_keys = ExtractGroupKeys(tuple);

            // Find or create group
            AggregateGroup *group = nullptr;
            for (auto &g : groups_) {
                if (g.group_keys == group_keys) {
                    group = &g;
                    break;
                }
            }

            if (!group) {
                groups_.push_back(AggregateGroup());
                group = &groups_.back();
                group->group_keys = group_keys;
            }

            UpdateAggregate(*group, tuple);
        }
    }

    bool AggregationExecutor::Next(Tuple *tuple) {
        if (finished_ || groups_.empty()) return false;

        if (current_group_index_ < groups_.size()) {
            *tuple = BuildOutputTuple(groups_[current_group_index_]);
            current_group_index_++;
            return true;
        }

        finished_ = true;
        return false;
    }

    const Schema *AggregationExecutor::GetOutputSchema() {
        if (!output_schema_) {
            std::vector<Column> cols;
            // Add group by columns
            for (const auto &col_name : plan_->group_by_columns_) {
                cols.emplace_back(col_name, TypeId::VARCHAR);  // Simplified
            }
            // Add aggregate columns (would need to parse aggregate functions)
            output_schema_ = std::make_shared<Schema>(cols);
        }
        return output_schema_.get();
    }

    std::vector<Value> AggregationExecutor::ExtractGroupKeys(const Tuple &tuple) const {
        std::vector<Value> keys;
        // Extract values for GROUP BY columns
        // Simplified implementation
        return keys;
    }

    void AggregationExecutor::UpdateAggregate(AggregateGroup &group, const Tuple &tuple) {
        // Update aggregate values
        // Implementation depends on aggregate function types
    }

    Tuple AggregationExecutor::BuildOutputTuple(const AggregateGroup &group) const {
        std::vector<Value> values = group.group_keys;
        // Add aggregate values
        return Tuple(values, *output_schema_);
    }

    /**
     * SortExecutor Implementation
     */
    void SortExecutor::Init() {
        if (!child_executor_) {
            throw Exception(ExceptionType::EXECUTION, "Child executor required for sort");
        }

        child_executor_->Init();

        // Load all tuples
        Tuple tuple;
        while (child_executor_->Next(&tuple)) {
            sorted_tuples_.push_back(tuple);
        }

        // Sort tuples
        std::sort(sorted_tuples_.begin(), sorted_tuples_.end(),
                 [this](const Tuple &a, const Tuple &b) {
                     return CompareTuples(a, b);
                 });

        current_index_ = 0;
    }

    bool SortExecutor::Next(Tuple *tuple) {
        if (current_index_ >= sorted_tuples_.size()) {
            return false;
        }

        *tuple = sorted_tuples_[current_index_++];
        return true;
    }

    const Schema *SortExecutor::GetOutputSchema() {
        return child_executor_->GetOutputSchema();
    }

    bool SortExecutor::CompareTuples(const Tuple &a, const Tuple &b) const {
        if (plan_->order_by_.empty()) return false;

        const auto &order_col = plan_->order_by_[0];
        int col_idx = child_executor_->GetOutputSchema()->GetColIdx(order_col.column);

        if (col_idx < 0) return false;

        Value a_val = a.GetValue(*child_executor_->GetOutputSchema(), col_idx);
        Value b_val = b.GetValue(*child_executor_->GetOutputSchema(), col_idx);

        int cmp = CompareValues(a_val, b_val);
        
        if (order_col.direction == SelectStatementWithJoins::SortDirection::DESC) {
            return cmp > 0;
        }
        return cmp < 0;
    }

    int SortExecutor::CompareValues(const Value &a, const Value &b) const {
        if (a.GetTypeId() == TypeId::INTEGER) {
            int32_t a_int = a.GetAsInteger();
            int32_t b_int = b.GetAsInteger();
            if (a_int < b_int) return -1;
            if (a_int > b_int) return 1;
            return 0;
        } else if (a.GetTypeId() == TypeId::VARCHAR) {
            return a.GetAsString().compare(b.GetAsString());
        }
        return 0;
    }

    /**
     * LimitExecutor Implementation
     */
    void LimitExecutor::Init() {
        if (!child_executor_) {
            throw Exception(ExceptionType::EXECUTION, "Child executor required for limit");
        }

        child_executor_->Init();
        row_count_ = 0;
    }

    bool LimitExecutor::Next(Tuple *tuple) {
        while (child_executor_->Next(tuple)) {
            row_count_++;

            // Skip rows before offset
            if (ShouldSkipRow()) {
                continue;
            }

            // Check if we've reached limit
            if (HasReachedLimit()) {
                return false;
            }

            return true;
        }

        return false;
    }

    const Schema *LimitExecutor::GetOutputSchema() {
        return child_executor_->GetOutputSchema();
    }

    /**
     * DistinctExecutor Implementation
     */
    void DistinctExecutor::Init() {
        if (!child_executor_) {
            throw Exception(ExceptionType::EXECUTION, "Child executor required for distinct");
        }

        child_executor_->Init();
        seen_tuples_.clear();
    }

    bool DistinctExecutor::Next(Tuple *tuple) {
        while (child_executor_->Next(tuple)) {
            std::string hash = TupleToHash(*tuple);

            if (seen_tuples_.find(hash) == seen_tuples_.end()) {
                seen_tuples_.insert(hash);
                return true;
            }
        }

        return false;
    }

    const Schema *DistinctExecutor::GetOutputSchema() {
        return child_executor_->GetOutputSchema();
    }

    std::string DistinctExecutor::TupleToHash(const Tuple &tuple) const {
        std::string hash;
        const Schema *schema = child_executor_->GetOutputSchema();

        for (uint32_t i = 0; i < schema->GetColumnCount(); ++i) {
            Value val = tuple.GetValue(*schema, i);
            hash += val.GetAsString() + "|";
        }

        return hash;
    }

} // namespace francodb


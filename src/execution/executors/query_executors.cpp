#include "execution/executors/query_executors.h"
#include "common/exception.h"
#include <algorithm>
#include <set>

namespace chronosdb {

// ============================================================================
// GroupByExecutor Implementation
// ============================================================================

void GroupByExecutor::Init() {
    child_executor_->Init();
    
    // Collect all tuples and group them
    Tuple tuple;
    while (child_executor_->Next(&tuple)) {
        std::string group_key = ComputeGroupKey(tuple);
        groups_[group_key].push_back(tuple);
    }

    groups_iter_ = groups_.begin();

    // Build output schema
    std::vector<Column> output_cols;
    const Schema *child_schema = child_executor_->GetOutputSchema();
    
    for (const auto &col_name : group_by_columns_) {
        int col_idx = child_schema->GetColIdx(col_name);
        if (col_idx >= 0) {
            output_cols.push_back(child_schema->GetColumn(col_idx));
        }
    }

    // Add aggregate columns
    for (const auto &agg_expr : aggregate_expressions_) {
        // Parse aggregate expressions like "COUNT(*)", "SUM(price)", etc.
        // For now, simplified handling
        Column agg_col("aggregated", TypeId::INTEGER);
        output_cols.push_back(agg_col);
    }

    output_schema_ = std::make_unique<Schema>(output_cols);
}

bool GroupByExecutor::Next(Tuple *tuple) {
    if (groups_iter_ == groups_.end()) {
        return false;
    }

    // For each group, compute aggregates
    const auto &group = groups_iter_->second;
    std::vector<Value> result_values;

    // Add group key columns
    if (!group.empty()) {
        const Schema *child_schema = child_executor_->GetOutputSchema();
        const Tuple &first = group[0];

        for (const auto &col_name : group_by_columns_) {
            int col_idx = child_schema->GetColIdx(col_name);
            if (col_idx >= 0) {
                result_values.push_back(first.GetValue(*child_schema, col_idx));
            }
        }

        // Compute aggregates
        for (const auto &agg_expr : aggregate_expressions_) {
            if (agg_expr.find("COUNT") != std::string::npos) {
                result_values.push_back(Value(TypeId::INTEGER, static_cast<int32_t>(group.size())));
            } else if (agg_expr.find("SUM") != std::string::npos) {
                // Simplified: would need to parse column name
                int32_t sum = 0;
                result_values.push_back(Value(TypeId::INTEGER, sum));
            }
        }
    }

    *tuple = Tuple(result_values, *output_schema_);
    ++groups_iter_;
    return true;
}

const Schema *GroupByExecutor::GetOutputSchema() {
    return output_schema_.get();
}

std::string GroupByExecutor::ComputeGroupKey(const Tuple &tuple) {
    std::string key;
    const Schema *schema = child_executor_->GetOutputSchema();

    for (const auto &col_name : group_by_columns_) {
        int col_idx = schema->GetColIdx(col_name);
        if (col_idx >= 0) {
            key += tuple.GetValue(*schema, col_idx).GetAsString() + "|";
        }
    }

    return key;
}

// ============================================================================
// OrderByExecutor Implementation
// ============================================================================

void OrderByExecutor::Init() {
    child_executor_->Init();

    // Load all tuples into memory for sorting
    Tuple tuple;
    while (child_executor_->Next(&tuple)) {
        sorted_tuples_.push_back(tuple);
    }

    // Sort tuples
    std::sort(sorted_tuples_.begin(), sorted_tuples_.end(),
              [this](const Tuple &a, const Tuple &b) {
                  return CompareTuples(a, b);
              });

    output_schema_ = std::make_unique<Schema>(
        *const_cast<Schema *>(child_executor_->GetOutputSchema())
    );
    current_index_ = 0;
}

bool OrderByExecutor::Next(Tuple *tuple) {
    if (current_index_ >= sorted_tuples_.size()) {
        return false;
    }

    *tuple = sorted_tuples_[current_index_];
    ++current_index_;
    return true;
}

const Schema *OrderByExecutor::GetOutputSchema() {
    return output_schema_.get();
}

bool OrderByExecutor::CompareTuples(const Tuple &a, const Tuple &b) const {
    const Schema *schema = child_executor_->GetOutputSchema();

    for (const auto &sort_col : sort_columns_) {
        int col_idx = schema->GetColIdx(sort_col.column_name);
        if (col_idx < 0) continue;

        Value val_a = a.GetValue(*schema, col_idx);
        Value val_b = b.GetValue(*schema, col_idx);

        int cmp = 0;
        if (val_a.GetTypeId() == TypeId::INTEGER) {
            cmp = (val_a.GetAsInteger() < val_b.GetAsInteger()) ? -1 : 
                  (val_a.GetAsInteger() > val_b.GetAsInteger()) ? 1 : 0;
        } else {
            cmp = (val_a.GetAsString() < val_b.GetAsString()) ? -1 :
                  (val_a.GetAsString() > val_b.GetAsString()) ? 1 : 0;
        }

        if (cmp != 0) {
            return sort_col.ascending ? (cmp < 0) : (cmp > 0);
        }
    }

    return false;
}

// ============================================================================
// LimitExecutor Implementation
// ============================================================================

void LimitExecutor::Init() {
    child_executor_->Init();
    current_count_ = 0;
    
    // Skip offset rows
    Tuple dummy;
    for (uint32_t i = 0; i < offset_; ++i) {
        child_executor_->Next(&dummy);
    }

    output_schema_ = std::make_unique<Schema>(
        *const_cast<Schema *>(child_executor_->GetOutputSchema())
    );
}

bool LimitExecutor::Next(Tuple *tuple) {
    if (limit_ > 0 && current_count_ >= limit_) {
        return false;
    }

    if (child_executor_->Next(tuple)) {
        ++current_count_;
        return true;
    }

    return false;
}

const Schema *LimitExecutor::GetOutputSchema() {
    return output_schema_.get();
}

// ============================================================================
// DistinctExecutor Implementation
// ============================================================================

void DistinctExecutor::Init() {
    child_executor_->Init();

    // Load all tuples and filter duplicates
    Tuple tuple;
    while (child_executor_->Next(&tuple)) {
        std::string tuple_str = TupleToString(tuple);
        
        if (seen_tuples_.find(tuple_str) == seen_tuples_.end()) {
            distinct_tuples_.push_back(tuple);
            seen_tuples_.insert(tuple_str);
        }
    }

    output_schema_ = std::make_unique<Schema>(
        *const_cast<Schema *>(child_executor_->GetOutputSchema())
    );
    current_index_ = 0;
}

bool DistinctExecutor::Next(Tuple *tuple) {
    if (current_index_ >= distinct_tuples_.size()) {
        return false;
    }

    *tuple = distinct_tuples_[current_index_];
    ++current_index_;
    return true;
}

const Schema *DistinctExecutor::GetOutputSchema() {
    return output_schema_.get();
}

std::string DistinctExecutor::TupleToString(const Tuple &tuple) const {
    std::string result;
    const Schema *schema = child_executor_->GetOutputSchema();

    for (uint32_t i = 0; i < schema->GetColumnCount(); ++i) {
        result += tuple.GetValue(*schema, i).GetAsString() + "|";
    }

    return result;
}

} // namespace chronosdb


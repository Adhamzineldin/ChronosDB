#include "execution/executors/join_executor.h"
#include "common/exception.h"
#include <algorithm>
#include <iostream>

namespace chronosdb {

JoinExecutor::JoinExecutor(ExecutorContext *exec_ctx,
                           std::unique_ptr<AbstractExecutor> left_executor,
                           std::unique_ptr<AbstractExecutor> right_executor,
                           JoinType join_type,
                           const std::vector<JoinCondition> &conditions,
                           Transaction *txn)
    : AbstractExecutor(exec_ctx),
      left_executor_(std::move(left_executor)),
      right_executor_(std::move(right_executor)),
      join_type_(join_type),
      conditions_(conditions),
      txn_(txn) {
    if (!left_executor_ || !right_executor_) {
        throw Exception(ExceptionType::EXECUTION, "Invalid executors provided to JoinExecutor");
    }
}

void JoinExecutor::Init() {
    // Initialize both executors
    left_executor_->Init();
    right_executor_->Init();

    // Build output schema by combining left and right schemas
    const Schema *left_schema = left_executor_->GetOutputSchema();
    const Schema *right_schema = right_executor_->GetOutputSchema();

    std::vector<Column> output_columns;
    
    // Add all columns from left table
    for (uint32_t i = 0; i < left_schema->GetColumnCount(); ++i) {
        const Column &col = left_schema->GetColumn(i);
        Column new_col = col;
        new_col.SetOffset(output_columns.size() * 8);  // Simplified offset calculation
        output_columns.push_back(new_col);
    }

    // Add all columns from right table
    for (uint32_t i = 0; i < right_schema->GetColumnCount(); ++i) {
        const Column &col = right_schema->GetColumn(i);
        Column new_col = col;
        new_col.SetOffset(output_columns.size() * 8);
        output_columns.push_back(new_col);
    }

    output_schema_ = std::make_unique<Schema>(output_columns);

    // For outer joins, cache all tuples for matching logic
    if (join_type_ == JoinType::LEFT || join_type_ == JoinType::RIGHT || 
        join_type_ == JoinType::FULL) {
        
        // Cache left table
        while (left_executor_->Next(&left_tuple_)) {
            left_cache_.push_back(left_tuple_);
            left_matched_.push_back(false);
        }

        // Cache right table
        while (right_executor_->Next(&right_tuple_)) {
            right_cache_.push_back(right_tuple_);
            right_matched_.push_back(false);
        }

        left_exhausted_ = true;
        right_exhausted_ = true;
    }
}

bool JoinExecutor::Next(Tuple *tuple) {
    switch (join_type_) {
        case JoinType::INNER:
            return ExecuteInnerJoin(tuple);
        case JoinType::LEFT:
            return ExecuteLeftJoin(tuple);
        case JoinType::RIGHT:
            return ExecuteRightJoin(tuple);
        case JoinType::FULL:
            return ExecuteFullJoin(tuple);
        case JoinType::CROSS:
            return ExecuteCrossJoin(tuple);
        default:
            return false;
    }
}

bool JoinExecutor::ExecuteInnerJoin(Tuple *result_tuple) {
    // Nested loop join: for each left tuple, scan right table
    if (left_exhausted_) return false;

    // If right executor is exhausted, fetch next left tuple
    if (right_exhausted_) {
        if (!left_executor_->Next(&left_tuple_)) {
            return false;
        }
        right_executor_->Init();  // Reset right executor
        right_exhausted_ = false;
    }

    // Inner loop: find matching right tuples
    while (right_executor_->Next(&right_tuple_)) {
        if (EvaluateJoinCondition(left_tuple_, right_tuple_)) {
            *result_tuple = CombineTuples(left_tuple_, right_tuple_);
            return true;
        }
    }

    // Right table exhausted, get next left tuple
    right_exhausted_ = true;
    return Next(result_tuple);
}

bool JoinExecutor::ExecuteLeftJoin(Tuple *result_tuple) {
    // LEFT JOIN: Include all left table rows, matching right if found
    while (left_index_ < left_cache_.size()) {
        left_tuple_ = left_cache_[left_index_];
        bool found_match = false;

        // Find all matching right tuples
        for (size_t r_idx = 0; r_idx < right_cache_.size(); ++r_idx) {
            right_tuple_ = right_cache_[r_idx];
            if (EvaluateJoinCondition(left_tuple_, right_tuple_)) {
                *result_tuple = CombineTuples(left_tuple_, right_tuple_);
                left_matched_[left_index_] = true;
                right_matched_[r_idx] = true;
                found_match = true;
                
                // Return first match
                if (right_index_ == 0) {
                    right_index_ = r_idx + 1;
                    return true;
                }
            }
        }

        // If no matches found, return left tuple with NULL right tuple
        if (!found_match && right_index_ == 0) {
            *result_tuple = CombineTuples(left_tuple_, Tuple());
            left_index_++;
            return true;
        }

        left_index_++;
        right_index_ = 0;
    }

    return false;
}

bool JoinExecutor::ExecuteRightJoin(Tuple *result_tuple) {
    // RIGHT JOIN: Include all right table rows, matching left if found
    while (right_index_ < right_cache_.size()) {
        right_tuple_ = right_cache_[right_index_];
        bool found_match = false;

        for (size_t l_idx = 0; l_idx < left_cache_.size(); ++l_idx) {
            left_tuple_ = left_cache_[l_idx];
            if (EvaluateJoinCondition(left_tuple_, right_tuple_)) {
                *result_tuple = CombineTuples(left_tuple_, right_tuple_);
                left_matched_[l_idx] = true;
                right_matched_[right_index_] = true;
                found_match = true;
                return true;
            }
        }

        if (!found_match) {
            *result_tuple = CombineTuples(Tuple(), right_tuple_);
            right_index_++;
            return true;
        }

        right_index_++;
    }

    return false;
}

bool JoinExecutor::ExecuteFullJoin(Tuple *result_tuple) {
    // FULL OUTER JOIN: Include all rows from both tables
    // First, do inner join matches
    if (!left_exhausted_) {
        while (left_index_ < left_cache_.size()) {
            left_tuple_ = left_cache_[left_index_];
            for (size_t r_idx = 0; r_idx < right_cache_.size(); ++r_idx) {
                right_tuple_ = right_cache_[r_idx];
                if (EvaluateJoinCondition(left_tuple_, right_tuple_)) {
                    *result_tuple = CombineTuples(left_tuple_, right_tuple_);
                    left_matched_[left_index_] = true;
                    right_matched_[r_idx] = true;
                    return true;
                }
            }
            left_index_++;
        }
        left_exhausted_ = true;
    }

    // Add unmatched left rows
    if (!right_exhausted_) {
        for (size_t i = 0; i < left_cache_.size(); ++i) {
            if (!left_matched_[i]) {
                *result_tuple = CombineTuples(left_cache_[i], Tuple());
                left_matched_[i] = true;
                return true;
            }
        }

        // Add unmatched right rows
        for (size_t i = 0; i < right_cache_.size(); ++i) {
            if (!right_matched_[i]) {
                *result_tuple = CombineTuples(Tuple(), right_cache_[i]);
                right_matched_[i] = true;
                return true;
            }
        }

        right_exhausted_ = true;
    }

    return false;
}

bool JoinExecutor::ExecuteCrossJoin(Tuple *result_tuple) {
    // CROSS JOIN: Cartesian product (all combinations)
    while (right_executor_->Next(&right_tuple_)) {
        *result_tuple = CombineTuples(left_tuple_, right_tuple_);
        return true;
    }

    // Right exhausted, get next left
    if (!left_executor_->Next(&left_tuple_)) {
        return false;
    }
    right_executor_->Init();
    return Next(result_tuple);
}

bool JoinExecutor::EvaluateJoinCondition(const Tuple &left_tuple, const Tuple &right_tuple) {
    if (conditions_.empty()) {
        return true;  // No join condition = all rows match
    }

    const Schema *left_schema = left_executor_->GetOutputSchema();
    const Schema *right_schema = right_executor_->GetOutputSchema();

    for (const auto &cond : conditions_) {
        int left_col_idx = left_schema->GetColIdx(cond.left_column);
        int right_col_idx = right_schema->GetColIdx(cond.right_column);

        if (left_col_idx < 0 || right_col_idx < 0) {
            continue;  // Skip if column not found
        }

        Value left_val = left_tuple.GetValue(*left_schema, left_col_idx);
        Value right_val = right_tuple.GetValue(*right_schema, right_col_idx);

        bool match = false;
        if (cond.op == "=") {
            match = (left_val.GetAsString() == right_val.GetAsString());
        } else if (cond.op == "<") {
            if (left_val.GetTypeId() == TypeId::INTEGER) {
                match = (left_val.GetAsInteger() < right_val.GetAsInteger());
            } else {
                match = (left_val.GetAsString() < right_val.GetAsString());
            }
        } else if (cond.op == ">") {
            if (left_val.GetTypeId() == TypeId::INTEGER) {
                match = (left_val.GetAsInteger() > right_val.GetAsInteger());
            } else {
                match = (left_val.GetAsString() > right_val.GetAsString());
            }
        }

        if (!match) return false;  // AND logic
    }

    return true;
}

Tuple JoinExecutor::CombineTuples(const Tuple &left, const Tuple &right) {
    // Combine two tuples into one result tuple
    const Schema *left_schema = left_executor_->GetOutputSchema();
    const Schema *right_schema = right_executor_->GetOutputSchema();

    std::vector<Value> combined_values;

    // Add values from left tuple
    for (uint32_t i = 0; i < left_schema->GetColumnCount(); ++i) {
        combined_values.push_back(left.GetValue(*left_schema, i));
    }

    // Add values from right tuple
    for (uint32_t i = 0; i < right_schema->GetColumnCount(); ++i) {
        combined_values.push_back(right.GetValue(*right_schema, i));
    }

    return Tuple(combined_values, *output_schema_);
}

const Schema *JoinExecutor::GetOutputSchema() {
    return output_schema_.get();
}

} // namespace chronosdb

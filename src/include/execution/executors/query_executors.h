#pragma once

#include "execution/executors/abstract_executor.h"
#include "parser/advanced_statements.h"
#include <vector>
#include <map>
#include <memory>

namespace francodb {

/**
 * GroupByExecutor: Groups rows and applies aggregate functions
 * 
 * Supports: GROUP BY, HAVING, Aggregate functions (COUNT, SUM, AVG, MIN, MAX)
 * Follows Strategy Pattern for different aggregation strategies
 */
class GroupByExecutor : public AbstractExecutor {
public:
    GroupByExecutor(ExecutorContext *exec_ctx,
                   std::unique_ptr<AbstractExecutor> child_executor,
                   const std::vector<std::string> &group_by_columns,
                   const std::vector<std::string> &aggregate_expressions,
                   Transaction *txn = nullptr)
        : AbstractExecutor(exec_ctx),
          child_executor_(std::move(child_executor)),
          group_by_columns_(group_by_columns),
          aggregate_expressions_(aggregate_expressions),
          txn_(txn) {}

    void Init() override;
    bool Next(Tuple *tuple) override;
    const Schema *GetOutputSchema() override;

private:
    std::unique_ptr<AbstractExecutor> child_executor_;
    std::vector<std::string> group_by_columns_;
    std::vector<std::string> aggregate_expressions_;
    
    // Group aggregation data: maps group key to list of tuples
    std::map<std::string, std::vector<Tuple>> groups_;
    std::map<std::string, std::vector<Tuple>>::iterator groups_iter_;
    
    std::unique_ptr<Schema> output_schema_;
    Transaction *txn_;

    std::string ComputeGroupKey(const Tuple &tuple);
};

/**
 * OrderByExecutor: Sorts result set
 * 
 * Supports: ASC/DESC, multiple column sorting
 */
class OrderByExecutor : public AbstractExecutor {
public:
    struct SortColumn {
        std::string column_name;
        bool ascending;
    };

    OrderByExecutor(ExecutorContext *exec_ctx,
                   std::unique_ptr<AbstractExecutor> child_executor,
                   const std::vector<SortColumn> &sort_columns,
                   Transaction *txn = nullptr)
        : AbstractExecutor(exec_ctx),
          child_executor_(std::move(child_executor)),
          sort_columns_(sort_columns),
          txn_(txn) {}

    void Init() override;
    bool Next(Tuple *tuple) override;
    const Schema *GetOutputSchema() override;

private:
    std::unique_ptr<AbstractExecutor> child_executor_;
    std::vector<SortColumn> sort_columns_;
    std::vector<Tuple> sorted_tuples_;
    size_t current_index_ = 0;
    
    std::unique_ptr<Schema> output_schema_;
    Transaction *txn_;

    bool CompareTuples(const Tuple &a, const Tuple &b) const;
};

/**
 * LimitExecutor: Limits and offsets result rows
 * 
 * Supports: LIMIT n OFFSET m
 */
class LimitExecutor : public AbstractExecutor {
public:
    LimitExecutor(ExecutorContext *exec_ctx,
                 std::unique_ptr<AbstractExecutor> child_executor,
                 uint32_t limit,
                 uint32_t offset = 0,
                 Transaction *txn = nullptr)
        : AbstractExecutor(exec_ctx),
          child_executor_(std::move(child_executor)),
          limit_(limit),
          offset_(offset),
          txn_(txn),
          current_count_(0) {}

    void Init() override;
    bool Next(Tuple *tuple) override;
    const Schema *GetOutputSchema() override;

private:
    std::unique_ptr<AbstractExecutor> child_executor_;
    uint32_t limit_;
    uint32_t offset_;
    uint32_t current_count_;
    
    std::unique_ptr<Schema> output_schema_;
    Transaction *txn_;
};

/**
 * DistinctExecutor: Removes duplicate rows
 * 
 * Supports: SELECT DISTINCT
 */
class DistinctExecutor : public AbstractExecutor {
public:
    explicit DistinctExecutor(ExecutorContext *exec_ctx,
                             std::unique_ptr<AbstractExecutor> child_executor,
                             Transaction *txn = nullptr)
        : AbstractExecutor(exec_ctx),
          child_executor_(std::move(child_executor)),
          txn_(txn) {}

    void Init() override;
    bool Next(Tuple *tuple) override;
    const Schema *GetOutputSchema() override;

private:
    std::unique_ptr<AbstractExecutor> child_executor_;
    std::vector<Tuple> distinct_tuples_;
    size_t current_index_ = 0;
    
    std::set<std::string> seen_tuples_;  // For uniqueness tracking
    std::unique_ptr<Schema> output_schema_;
    Transaction *txn_;

    std::string TupleToString(const Tuple &tuple) const;
};

} // namespace francodb


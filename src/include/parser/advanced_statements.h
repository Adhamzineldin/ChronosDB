#pragma once

#include <string>
#include <vector>
#include "parser/statement.h"

namespace chronosdb {

/**
 * JoinType: Types of SQL JOIN operations
 */
enum class JoinType {
    INNER,      // INNER JOIN
    LEFT,       // LEFT OUTER JOIN
    RIGHT,      // RIGHT OUTER JOIN
    FULL,       // FULL OUTER JOIN
    CROSS       // CROSS JOIN (Cartesian product)
};

/**
 * JoinCondition: Represents a JOIN condition (e.g., table1.col = table2.col)
 */
struct JoinCondition {
    std::string left_table;
    std::string left_column;
    std::string right_table;
    std::string right_column;
    std::string op;  // "=", "<", ">", etc.
    
    JoinCondition() = default;
    JoinCondition(const std::string& lt, const std::string& lc,
                  const std::string& rt, const std::string& rc,
                  const std::string& operation = "=")
        : left_table(lt), left_column(lc), right_table(rt), right_column(rc), op(operation) {}
};

/**
 * AggregateType: Types of aggregate functions
 */
enum class AggregateType {
    COUNT,
    SUM,
    AVG,
    MIN,
    MAX
};

/**
 * AggregateExpression: Represents an aggregate function call
 */
struct AggregateExpression {
    AggregateType type;
    std::string column_name;
    std::string alias;
    
    AggregateExpression() : type(AggregateType::COUNT) {}
    AggregateExpression(AggregateType t, const std::string& col, const std::string& a = "")
        : type(t), column_name(col), alias(a) {}
};

/**
 * SortDirection: Direction for ORDER BY clause
 */
enum class SortDirection {
    ASC,    // Ascending
    DESC    // Descending
};

/**
 * OrderByItem: Single item in ORDER BY clause
 */
struct OrderByItem {
    std::string column_name;
    SortDirection direction = SortDirection::ASC;
    
    OrderByItem() = default;
    OrderByItem(const std::string& col, SortDirection dir = SortDirection::ASC)
        : column_name(col), direction(dir) {}
};

/**
 * ReferentialAction: Actions for FOREIGN KEY constraints
 */
enum class ReferentialAction {
    NO_ACTION,
    CASCADE,
    RESTRICT,
    SET_NULL,
    SET_DEFAULT
};

/**
 * ForeignKeyConstraint: Represents a FOREIGN KEY constraint
 */
struct ForeignKeyConstraint {
    std::string name;
    std::vector<std::string> columns;
    std::string ref_table;
    std::vector<std::string> ref_columns;
    ReferentialAction on_delete = ReferentialAction::NO_ACTION;
    ReferentialAction on_update = ReferentialAction::NO_ACTION;
    
    ForeignKeyConstraint() = default;
};

/**
 * CheckConstraint: Represents a CHECK constraint
 */
struct CheckConstraint {
    std::string name;
    std::string expression;
    
    CheckConstraint() = default;
    CheckConstraint(const std::string& n, const std::string& expr)
        : name(n), expression(expr) {}
};

} // namespace chronosdb


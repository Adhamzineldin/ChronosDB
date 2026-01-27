#pragma once

#include <string>
#include <vector>
#include "parser/statement.h"
#include "storage/table/column.h"
#include "catalog/foreign_key.h"

namespace chronosdb {

/**
 * ALTER TABLE Statement: Modify table structure
 * 
 * Supports:
 * - ADD COLUMN
 * - DROP COLUMN
 * - ADD FOREIGN KEY
 * - DROP FOREIGN KEY
 * - RENAME COLUMN
 * - RENAME TABLE
 */
class AlterTableStatement : public Statement {
public:
    enum class AlterType {
        ADD_COLUMN,
        DROP_COLUMN,
        ADD_FOREIGN_KEY,
        DROP_FOREIGN_KEY,
        ADD_CONSTRAINT,
        DROP_CONSTRAINT,
        RENAME_TABLE,
        RENAME_COLUMN
    };

    StatementType GetType() const override { return StatementType::CREATE; }

    std::string table_name_;
    std::string new_table_name_;           // For RENAME TABLE
    
    AlterType operation_;
    
    Column new_column_;                    // For ADD COLUMN
    std::string column_to_drop_;           // For DROP COLUMN
    std::string old_column_name_;          // For RENAME COLUMN
    std::string new_column_name_;
    
    ForeignKeyConstraint fk_constraint_;   // For ADD/DROP FK
};

/**
 * TRUNCATE Statement: Delete all rows from table
 * 
 * Supports: TRUNCATE TABLE name
 * Faster than DELETE for clearing entire table
 */
class TruncateStatement : public Statement {
public:
    StatementType GetType() const override { return StatementType::DELETE_CMD; }

    std::string table_name_;

    explicit TruncateStatement(const std::string &table) 
        : table_name_(table) {}
};

/**
 * CREATE INDEX Statement: Enhanced with partial indexes
 * 
 * Supports: CREATE [UNIQUE] INDEX name ON table (columns) [WHERE condition]
 */
class CreateIndexStatementEnhanced : public Statement {
public:
    StatementType GetType() const override { return StatementType::CREATE_INDEX; }

    std::string index_name_;
    std::string table_name_;
    std::vector<std::string> columns_;     // Support multi-column indexes
    
    bool is_unique_ = false;               // UNIQUE INDEX
    std::vector<WhereCondition> where_clause_;  // Partial index
    
    enum class IndexType { BTREE, HASH, FULLTEXT } type_ = IndexType::BTREE;
};

/**
 * VIEW Statement: Create/Drop views
 * 
 * Supports: CREATE VIEW name AS SELECT ...
 */
class CreateViewStatement : public Statement {
public:
    StatementType GetType() const override { return StatementType::CREATE; }

    std::string view_name_;
    std::string select_query_;             // The underlying SELECT
    std::vector<std::string> column_names_;
};

/**
 * ANALYZE Statement: Gather table statistics
 * 
 * Supports: ANALYZE TABLE name [UPDATE HISTOGRAM]
 */
class AnalyzeStatement : public Statement {
public:
    StatementType GetType() const override { return StatementType::SHOW_STATUS; }

    std::string table_name_;
    bool update_histogram_ = false;
};

/**
 * EXPLAIN Statement: Query execution plan analysis
 * 
 * Supports: EXPLAIN SELECT ...
 */
class ExplainStatement : public Statement {
public:
    StatementType GetType() const override { return StatementType::SHOW_STATUS; }

    std::unique_ptr<Statement> query_statement_;
    bool detailed_ = false;  // EXPLAIN DETAIL
};

/**
 * PRAGMA Statement: Database configuration
 * 
 * Supports: PRAGMA key = value
 */
class PragmaStatement : public Statement {
public:
    StatementType GetType() const override { return StatementType::SHOW_STATUS; }

    std::string pragma_key_;
    std::string pragma_value_;
};

/**
 * VACUUM Statement: Optimize database
 * 
 * Supports: VACUUM [table_name]
 */
class VacuumStatement : public Statement {
public:
    StatementType GetType() const override { return StatementType::CREATE; }

    std::string table_name_;  // Empty = vacuum all
};

} // namespace chronosdb


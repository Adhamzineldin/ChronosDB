#pragma once

#include <string>
#include <vector>
#include <memory>
#include "storage/table/schema.h"
#include "common/value.h"

namespace chronosdb {
    enum class StatementType {
        CREATE, INSERT, SELECT, DELETE_CMD, UPDATE_CMD, DROP, CREATE_INDEX, DROP_INDEX, BEGIN, ROLLBACK, COMMIT, CREATE_DB, USE_DB,
        LOGIN, CREATE_USER, ALTER_USER_ROLE, DELETE_USER, SHOW_USERS, SHOW_DATABASES, SHOW_TABLES, SHOW_STATUS, WHOAMI,
        DROP_DB, CREATE_TABLE,
        DESCRIBE_TABLE, ALTER_TABLE, SHOW_CREATE_TABLE,
        CHECKPOINT, RECOVER, STOP_SERVER,
        SHOW_AI_STATUS, SHOW_ANOMALIES, SHOW_EXECUTION_STATS,
        CREATE_VIEW, DROP_VIEW, EXPLAIN,
        // New features
        EXPORT_TABLE, IMPORT_TABLE,
        BACKUP_DB, RESTORE_DB,
        CREATE_PROCEDURE, CALL_PROCEDURE, DROP_PROCEDURE,
        CREATE_TRIGGER, DROP_TRIGGER,
        SHOW_HISTORY,
        CREATE_SCHEDULE, DROP_SCHEDULE, SHOW_SCHEDULES,
        SET_REPLICATION, SHOW_REPLICATION_STATUS,
        SHOW_INDEX_SUGGESTIONS,
        SHOW_BLOCKED_QUERIES, APPROVE_QUERY
    };

    enum class LogicType { NONE, AND, OR };

    class Statement {
    public:
        virtual ~Statement() = default;

        virtual StatementType GetType() const = 0;

        // Original SQL text (set by parser for query history tracking)
        std::string sql_text_;
    };

    // Forward declaration for subquery support in WhereCondition
    class SelectStatement;

    struct WhereCondition {
        std::string column;
        std::string op; // "=", ">", "<", ">=", "<=", or "IN"
        Value value; // For =, >, <, >=, <= operators
        std::vector<Value> in_values; // For "IN" operator
        LogicType next_logic; // Does "WE" or "AW" come after this?
        std::unique_ptr<SelectStatement> subquery; // For IN (SELECT ...)
    };

    // --- TABLE LEVEL OPS ---

    /** 2E3MEL GADWAL <name> (...) */
    class CreateStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::CREATE; }
        std::string table_name_;
        std::vector<Column> columns_;

        // ============ ENTERPRISE FEATURES ============
        // FOREIGN KEY constraints
        struct ForeignKey {
            std::vector<std::string> columns; // Local columns
            std::string ref_table; // Referenced table
            std::vector<std::string> ref_columns; // Referenced columns
            std::string on_delete; // CASCADE, RESTRICT, SET NULL, etc.
            std::string on_update; // CASCADE, RESTRICT, SET NULL, etc.
        };

        std::vector<ForeignKey> foreign_keys_;

        // CHECK constraints (table-level)
        struct CheckConstraint {
            std::string name;
            std::string expression;
        };

        std::vector<CheckConstraint> check_constraints_;

        // ============ TABLE PARTITIONING ============
        struct PartitionDef {
            std::string name;
            int upper_bound = INT32_MAX; // INT32_MAX for MAXVALUE
        };
        bool is_partitioned_ = false;
        std::string partition_column_;
        std::string partition_type_; // "RANGE" or "HASH"
        std::vector<PartitionDef> partitions_;
        int hash_partition_count_ = 0;
    };

    /** EREMY GADWAL <name> (DROP TABLE) */
    class DropStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::DROP; }
        std::string table_name_;
        bool if_exists_ = false;  // DROP TABLE IF EXISTS
    };

    /** DROP INDEX <name> */
    class DropIndexStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::DROP_INDEX; }
        std::string index_name_;
        bool if_exists_ = false;
    };

    // --- ROW LEVEL OPS ---

    /** EMLA GOWA <name> ELKEYAM (...) - Supports multi-row insert for efficiency */
    class InsertStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::INSERT; }
        std::string table_name_;
        std::vector<std::string> column_names_;
        std::vector<Value> values_;  // For single row insert (backward compatibility)
        std::vector<std::vector<Value>> value_rows_;  // For multi-row insert

        // Helper to check if this is a multi-row insert
        bool IsMultiRowInsert() const { return !value_rows_.empty(); }
    };

    class SelectStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SELECT; }

        bool select_all_ = false;
        std::vector<std::string> columns_;
        std::string table_name_;

        // THE UPGRADE: A list of conditions instead of single variables
        std::vector<WhereCondition> where_clause_;

        // ============ ENTERPRISE FEATURES ============
        // DISTINCT support
        bool is_distinct_ = false;

        // Aggregate functions (COUNT, SUM, AVG, MIN, MAX)
        std::vector<std::pair<std::string, std::string> > aggregates_; // {function, column}

        // JOIN support
        struct JoinClause {
            std::string join_type; // "INNER", "LEFT", "RIGHT", "CROSS"
            std::string table_name; // Table to join with
            std::string condition; // Join condition (simplified for now)
        };

        std::vector<JoinClause> joins_;

        // GROUP BY support
        std::vector<std::string> group_by_columns_;

        // HAVING clause (like WHERE but for grouped results)
        std::vector<WhereCondition> having_clause_;

        // ORDER BY support
        struct OrderByClause {
            std::string column;
            std::string direction; // "ASC" or "DESC"
        };

        std::vector<OrderByClause> order_by_;

        // LIMIT and OFFSET support
        int limit_ = -1; // -1 means no limit
        int offset_ = 0; // Start from beginning by default
        uint64_t as_of_timestamp_ = 0;

        // ============ CTE SUPPORT ============
        struct CTEDefinition {
            std::string name;
            std::unique_ptr<SelectStatement> query;
        };
        std::vector<CTEDefinition> cte_definitions_;

        // ============ WINDOW FUNCTIONS ============
        struct WindowFunction {
            std::string function_name; // ROW_NUMBER, RANK, DENSE_RANK, LAG, LEAD
            std::string argument;      // column arg for LAG/LEAD
            int offset = 1;            // offset for LAG/LEAD
            std::vector<std::string> partition_by;
            std::vector<OrderByClause> order_by;
            std::string alias;
        };
        std::vector<WindowFunction> window_functions_;
    };

    class UpdateStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::UPDATE_CMD; }
        std::string table_name_;
        std::string target_column_;
        Value new_value_;

        std::vector<WhereCondition> where_clause_; // Upgrade
    };

    class DeleteStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::DELETE_CMD; }
        std::string table_name_;

        std::vector<WhereCondition> where_clause_; // Upgrade
    };

    struct CreateIndexStatement : public Statement {
        StatementType GetType() const override { return StatementType::CREATE_INDEX; }

        std::string index_name_;
        std::string table_name_;
        std::string column_name_; // Single column index for simplicity
        std::string index_type_str_; // "BTREE" or "HASH" - defaults to "BTREE"
    };

    /** BED2 (BEGIN TRANSACTION) */
    class BeginStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::BEGIN; }
    };

    /** ERGA3 (ROLLBACK TRANSACTION) */
    class RollbackStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::ROLLBACK; }
    };

    /** KAMEL (COMMIT TRANSACTION) */
    class CommitStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::COMMIT; }
    };

    // --- DATABASE & AUTH ---

    class CreateDatabaseStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::CREATE_DB; }
        std::string db_name_;
    };

    class UseDatabaseStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::USE_DB; }
        std::string db_name_;
    };

    class LoginStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::LOGIN; }
        std::string username_;
        std::string password_;
    };

    class CreateUserStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::CREATE_USER; }
        std::string username_;
        std::string password_;
        std::string role_; // ADMIN/USER/READONLY
    };

    class AlterUserRoleStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::ALTER_USER_ROLE; }
        std::string username_;
        std::string role_;
        std::string db_name_; // Added for ALTER USER ... ROLE ... IN ...
    };

    class DeleteUserStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::DELETE_USER; }
        std::string username_;
    };

    class ShowUsersStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SHOW_USERS; }
    };

    class ShowDatabasesStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SHOW_DATABASES; }
    };

    class ShowTablesStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SHOW_TABLES; }
    };

    class WhoAmIStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::WHOAMI; }
    };

    class ShowStatusStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SHOW_STATUS; }
    };

    class DropDatabaseStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::DROP_DB; }
        std::string db_name_;
    };

    // DESCRIBE TABLE / DESC
    class DescribeTableStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::DESCRIBE_TABLE; }
        std::string table_name_;
    };

    // SHOW CREATE TABLE
    class ShowCreateTableStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SHOW_CREATE_TABLE; }
        std::string table_name_;
    };

    // ALTER TABLE
    class AlterTableStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::ALTER_TABLE; }
        std::string table_name_;

        enum class AlterType {
            ADD_COLUMN,
            DROP_COLUMN,
            MODIFY_COLUMN,
            RENAME_COLUMN,
            ADD_PRIMARY_KEY,
            DROP_PRIMARY_KEY
        };

        AlterType alter_type_;
        std::string column_name_;
        Column new_column_def_; // For ADD_COLUMN or MODIFY_COLUMN
        std::string new_column_name_; // For RENAME_COLUMN
    };
    
    class CheckpointStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::CHECKPOINT; }
    };

    class RecoverStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::RECOVER; }
        uint64_t timestamp_;
        
        explicit RecoverStatement(uint64_t timestamp) : timestamp_(timestamp) {}
    };
    
    class StopServerStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::STOP_SERVER; }
    };

    // --- AI LAYER COMMANDS ---

    class ShowAIStatusStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SHOW_AI_STATUS; }
    };

    class ShowAnomaliesStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SHOW_ANOMALIES; }
    };

    class ShowExecutionStatsStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SHOW_EXECUTION_STATS; }
    };

    class DropViewStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::DROP_VIEW; }
        std::string view_name_;
    };

    // ============ EXPORT / IMPORT ============

    class ExportStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::EXPORT_TABLE; }
        std::string table_name_;
        std::string file_path_;
        std::string format_ = "CSV";
    };

    class ImportStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::IMPORT_TABLE; }
        std::string file_path_;
        std::string table_name_;
        std::string format_ = "CSV";
    };

    // ============ BACKUP / RESTORE ============

    class BackupStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::BACKUP_DB; }
        std::string file_path_;
    };

    class RestoreStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::RESTORE_DB; }
        std::string file_path_;
    };

    // ============ STORED PROCEDURES ============

    struct ProcedureParam {
        std::string name;
        TypeId type;
    };

    class CreateProcedureStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::CREATE_PROCEDURE; }
        std::string name_;
        std::vector<ProcedureParam> params_;
        std::string body_; // Raw SQL body between BEGIN...END
    };

    class CallProcedureStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::CALL_PROCEDURE; }
        std::string name_;
        std::vector<Value> arguments_;
    };

    class DropProcedureStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::DROP_PROCEDURE; }
        std::string name_;
    };

    // ============ TRIGGERS ============

    class CreateTriggerStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::CREATE_TRIGGER; }
        std::string name_;
        std::string timing_;     // "BEFORE" or "AFTER"
        std::string event_;      // "INSERT", "UPDATE", "DELETE"
        std::string table_name_;
        std::string body_;       // Raw SQL between BEGIN...END
    };

    class DropTriggerStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::DROP_TRIGGER; }
        std::string name_;
    };

    // ============ QUERY HISTORY ============

    class ShowHistoryStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SHOW_HISTORY; }
        int limit_ = 100;
    };

    // ============ SCHEDULED JOBS ============

    class CreateScheduleStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::CREATE_SCHEDULE; }
        std::string name_;
        int interval_seconds_ = 0;
        std::string sql_body_;
    };

    class DropScheduleStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::DROP_SCHEDULE; }
        std::string name_;
    };

    class ShowSchedulesStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SHOW_SCHEDULES; }
    };

    // ============ REPLICATION ============

    class SetReplicationStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SET_REPLICATION; }
        std::string role_;        // "PRIMARY", "REPLICA", "STANDALONE"
        std::string primary_host_;
        int primary_port_ = 2501;
    };

    class ShowReplicationStatusStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SHOW_REPLICATION_STATUS; }
    };

    // ============ INDEX ADVISOR ============

    class ShowIndexSuggestionsStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SHOW_INDEX_SUGGESTIONS; }
    };

    // ============ QUERY FIREWALL ============

    class ShowBlockedQueriesStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::SHOW_BLOCKED_QUERIES; }
    };

    class ApproveQueryStatement : public Statement {
    public:
        StatementType GetType() const override { return StatementType::APPROVE_QUERY; }
        int query_id_ = 0;
    };

} // namespace chronosdb

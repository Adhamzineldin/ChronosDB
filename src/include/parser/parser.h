#pragma once

#include <memory>
#include <iomanip>
#include <sstream>
#include <chrono>

#include "parser/lexer.h"
#include "parser/statement.h"
#include "parser/extended_statements.h"

namespace chronosdb {

    class Parser {
    public:
        explicit Parser(Lexer lexer);

        static uint64_t ParseHumanDateToMicros(const std::string & text);

        std::unique_ptr<Statement> ParseQuery();

    private:
        void Advance();
    
        // Returns true and advances if current_token matches type, else returns false
        bool Match(TokenType type); 
    
        // Helper to turn a NUMBER or STRING_LIT token into a Value object
        Value ParseValue();

        // --- Specific Command Parsers ---
    
        std::unique_ptr<CreateStatement> ParseCreateTable();
        std::unique_ptr<InsertStatement> ParseInsert();
        std::unique_ptr<SelectStatement> ParseSelect();
        std::unique_ptr<UpdateStatement> ParseUpdate();
        std::unique_ptr<Statement> ParseDelete(); // Handles DROP and DELETE
        std::vector<WhereCondition> ParseWhereClause();

        std::unique_ptr<CreateIndexStatement> ParseCreateIndex();
        std::unique_ptr<CreateDatabaseStatement> ParseCreateDatabase();
        std::unique_ptr<UseDatabaseStatement> ParseUseDatabase();
        std::unique_ptr<LoginStatement> ParseLogin();
        std::unique_ptr<CreateUserStatement> ParseCreateUser();
        std::unique_ptr<AlterUserRoleStatement> ParseAlterUserRole();
        std::unique_ptr<DeleteUserStatement> ParseDeleteUser();
        std::unique_ptr<ShowUsersStatement> ParseShowUsers();
        std::unique_ptr<ShowDatabasesStatement> ParseShowDatabases();
        std::unique_ptr<ShowTablesStatement> ParseShowTables();
        std::unique_ptr<WhoAmIStatement> ParseWhoAmI();
        std::unique_ptr<ShowStatusStatement> ParseShowStatus();

        // ============ PHASE 2B: ENTERPRISE FEATURE HELPERS ============
        
        // Aggregate Functions
        bool IsAggregateFunction();
        std::pair<std::string, std::string> ParseAggregateFunction();
        
        // JOIN support
        bool IsJoinKeyword();
        SelectStatement::JoinClause ParseJoinClause();
        std::string ParseJoinCondition();
        
        // GROUP BY
        std::vector<std::string> ParseGroupByColumns();
        
        // ORDER BY
        std::vector<SelectStatement::OrderByClause> ParseOrderByClause();
        
        // Numbers (for LIMIT/OFFSET)
        int ParseNumber();
        
        // Enhanced CREATE TABLE
        std::string ParseReferentialAction();
        std::string ParseCheckExpression();

        std::unique_ptr<DropDatabaseStatement> ParseDropDatabase();
        std::unique_ptr<DropStatement> ParseDropTable();
        std::unique_ptr<DropIndexStatement> ParseDropIndex();

        // Hash Index / Views / Explain
        std::unique_ptr<CreateIndexStatement> ParseCreateHashIndex();
        std::unique_ptr<Statement> ParseCreateView();
        std::unique_ptr<Statement> ParseDropView();
        std::unique_ptr<Statement> ParseExplain();

        // Subquery (no trailing semicolon)
        std::unique_ptr<SelectStatement> ParseSelectSubquery();

        // ============ NEW FEATURES ============
        // CTEs
        std::unique_ptr<Statement> ParseCTE();

        // Window Functions
        bool IsWindowFunction();
        SelectStatement::WindowFunction ParseWindowFunction();

        // Table Partitioning
        void ParsePartitionClause(CreateStatement* stmt);

        // Export/Import
        std::unique_ptr<Statement> ParseExport();
        std::unique_ptr<Statement> ParseImport();

        // Backup/Restore
        std::unique_ptr<Statement> ParseBackup();
        std::unique_ptr<Statement> ParseRestore();

        // Stored Procedures
        std::unique_ptr<Statement> ParseCreateProcedure();
        std::unique_ptr<Statement> ParseCall();
        std::string CaptureBodyUntilEnd();

        // Triggers
        std::unique_ptr<Statement> ParseCreateTrigger();

        // Scheduled Jobs
        std::unique_ptr<Statement> ParseCreateSchedule();

        // Replication
        std::unique_ptr<Statement> ParseSetReplication();

        Lexer lexer_;
        Token current_token_;
    };

} // namespace chronosdb
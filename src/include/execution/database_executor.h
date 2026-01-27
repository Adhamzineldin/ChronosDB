#pragma once

#include "execution/execution_result.h"
#include "common/auth_manager.h"
#include "recovery/log_manager.h"
#include "network/database_registry.h"
#include "storage/storage_interface.h"  // For IBufferManager

namespace chronosdb {

// Forward declarations
class CreateDatabaseStatement;
class UseDatabaseStatement;
class DropDatabaseStatement;
class SessionContext;
class Catalog;
class ExecutorContext;

/**
 * DatabaseExecutor - Database management operations
 * 
 * SOLID PRINCIPLE: Single Responsibility
 * This class handles all database-level operations:
 * - CREATE DATABASE
 * - USE DATABASE
 * - DROP DATABASE
 * 
 * @author ChronosDB Team
 */
class DatabaseExecutor {
public:
    DatabaseExecutor(AuthManager* auth_manager, 
                     DatabaseRegistry* db_registry,
                     LogManager* log_manager)
        : auth_manager_(auth_manager), 
          db_registry_(db_registry),
          log_manager_(log_manager) {}
    
    // ========================================================================
    // DATABASE MANAGEMENT
    // ========================================================================
    
    /**
     * Create a new database.
     * Creates directory structure and initializes WAL.
     */
    ExecutionResult CreateDatabase(CreateDatabaseStatement* stmt, SessionContext* session);
    
    /**
     * Switch to a different database.
     * Updates session context and switches WAL file.
     * 
     * @param bpm_out Output: new IBufferManager pointer (polymorphic)
     * @param catalog_out Output: new Catalog pointer
     */
    ExecutionResult UseDatabase(UseDatabaseStatement* stmt, 
                                SessionContext* session,
                                IBufferManager** bpm_out,
                                Catalog** catalog_out);
    
    /**
     * Drop a database and all its contents.
     */
    ExecutionResult DropDatabase(DropDatabaseStatement* stmt, SessionContext* session);

private:
    AuthManager* auth_manager_;
    DatabaseRegistry* db_registry_;
    LogManager* log_manager_;
};

} // namespace chronosdb


#pragma once

#include <vector>
#include <memory>
#include <string>
#include <atomic>
#include "execution/execution_result.h"
#include "execution/executor_context.h"
#include "parser/statement.h"
#include "concurrency/transaction.h"
#include "catalog/catalog.h"
#include "common/auth_manager.h"
#include "network/database_registry.h" 
#include "network/session_context.h"
#include "recovery/log_manager.h"

namespace francodb {

// Forward declarations for specialized executors (SOLID - SRP)
class DDLExecutor;
class DMLExecutor;
class SystemExecutor;
class UserExecutor;
class DatabaseExecutor;
class TransactionExecutor;

/**
 * ExecutionEngine - Query Execution Coordinator
 * 
 * SOLID COMPLIANCE:
 * =================
 * This class follows the Single Responsibility Principle by delegating
 * all specialized operations to focused executor classes:
 * 
 * - DDLExecutor: CREATE/DROP/ALTER TABLE, CREATE INDEX
 * - DMLExecutor: INSERT/SELECT/UPDATE/DELETE
 * - SystemExecutor: SHOW DATABASES/TABLES/STATUS, WHOAMI
 * - UserExecutor: CREATE/ALTER/DELETE USER
 * - DatabaseExecutor: CREATE/USE/DROP DATABASE
 * - TransactionExecutor: BEGIN/COMMIT/ROLLBACK
 * 
 * The ExecutionEngine now only handles:
 * 1. Concurrency gatekeeper (global lock management)
 * 2. Delegation to appropriate executor
 * 3. Recovery operations (CHECKPOINT, RECOVER)
 * 
 * @author FrancoDB Team
 */
class ExecutionEngine {
public:
    static std::shared_mutex global_lock_;
    
    ExecutionEngine(BufferPoolManager* bpm, Catalog* catalog, AuthManager* auth_manager, 
                    DatabaseRegistry* db_registry, LogManager* log_manager);

    ~ExecutionEngine();

    // ========================================================================
    // PUBLIC API
    // ========================================================================
    
    /**
     * Execute a statement. Delegates to specialized executors.
     */
    ExecutionResult Execute(Statement* stmt, SessionContext* session);
    
    /**
     * Get the current catalog.
     */
    Catalog* GetCatalog() { return catalog_; }
    
    /**
     * Get the current transaction.
     */
    Transaction* GetCurrentTransaction();
    
    /**
     * Get or create transaction for write operations.
     */
    Transaction* GetCurrentTransactionForWrite();

private:
    // ========================================================================
    // HELPER METHODS
    // ========================================================================
    void InitializeExecutorFactory();
    std::string ValueToString(const Value& v);

    // ========================================================================
    // RECOVERY OPERATIONS (Kept inline - require special handling)
    // ========================================================================
    ExecutionResult ExecuteCheckpoint();
    ExecutionResult ExecuteRecover(RecoverStatement* stmt);

    // ========================================================================
    // CORE DEPENDENCIES (order matches constructor initialization)
    // ========================================================================
    BufferPoolManager* bpm_;
    Catalog* catalog_;
    AuthManager* auth_manager_;
    DatabaseRegistry* db_registry_;
    LogManager* log_manager_;
    ExecutorContext* exec_ctx_;
    
    // ========================================================================
    // SPECIALIZED EXECUTORS (SOLID - SRP Compliance)
    // ========================================================================
    std::unique_ptr<DDLExecutor> ddl_executor_;
    std::unique_ptr<DMLExecutor> dml_executor_;
    std::unique_ptr<SystemExecutor> system_executor_;
    std::unique_ptr<UserExecutor> user_executor_;
    std::unique_ptr<DatabaseExecutor> database_executor_;
    std::unique_ptr<TransactionExecutor> transaction_executor_;
    
    // ========================================================================
    // THREAD SAFETY (Issue #4 & #5 Fix)
    // ========================================================================
    // Cache-line aligned to prevent false sharing
    alignas(64) std::atomic<int> next_txn_id_{1};
};

} // namespace francodb
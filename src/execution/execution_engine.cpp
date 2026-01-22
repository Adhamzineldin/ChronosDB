/**
 * execution_engine.cpp
 * 
 * Query Execution Coordinator - SOLID Compliant
 * 
 * This file has been completely refactored to follow SOLID principles:
 * - Single Responsibility: Each executor handles one category of operations
 * - Open/Closed: New statement types can be added without modifying this file
 * - Dependency Inversion: Depends on executor abstractions
 * 
 * The ExecutionEngine now only handles:
 * 1. Concurrency gatekeeper (global lock management)
 * 2. Delegation to appropriate specialized executor
 * 3. Recovery operations (CHECKPOINT, RECOVER)
 * 
 * @author FrancoDB Team
 */

#include "execution/execution_engine.h"

// Specialized Executors (SOLID - SRP)
#include "execution/ddl_executor.h"
#include "execution/dml_executor.h"
#include "execution/system_executor.h"
#include "execution/user_executor.h"
#include "execution/database_executor.h"
#include "execution/transaction_executor.h"

// Recovery
#include "recovery/checkpoint_manager.h"
#include "recovery/recovery_manager.h"

// Common
#include "common/exception.h"

#include <sstream>
#include <iostream>
#include <chrono>

namespace francodb {

std::shared_mutex ExecutionEngine::global_lock_;

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

ExecutionEngine::ExecutionEngine(BufferPoolManager* bpm, Catalog* catalog,
                                 AuthManager* auth_manager, DatabaseRegistry* db_registry,
                                 LogManager* log_manager)
    : bpm_(bpm), 
      catalog_(catalog), 
      auth_manager_(auth_manager),
      db_registry_(db_registry), 
      log_manager_(log_manager), 
      exec_ctx_(nullptr),
      next_txn_id_(1) {
    
    // Create executor context
    exec_ctx_ = new ExecutorContext(bpm_, catalog_, nullptr, log_manager_);
    
    // Initialize all specialized executors (SOLID - SRP)
    ddl_executor_ = std::make_unique<DDLExecutor>(catalog_, log_manager_);
    dml_executor_ = std::make_unique<DMLExecutor>(bpm_, catalog_, log_manager_);
    system_executor_ = std::make_unique<SystemExecutor>(catalog_, auth_manager_, db_registry_);
    user_executor_ = std::make_unique<UserExecutor>(auth_manager_);
    database_executor_ = std::make_unique<DatabaseExecutor>(auth_manager_, db_registry_, log_manager_);
    transaction_executor_ = std::make_unique<TransactionExecutor>(log_manager_, catalog_);
    
    // Share the atomic counter with transaction executor
    transaction_executor_->SetNextTxnId(&next_txn_id_);
}

ExecutionEngine::~ExecutionEngine() {
    delete exec_ctx_;
}

// ============================================================================
// TRANSACTION ACCESS (Delegates to TransactionExecutor)
// ============================================================================

Transaction* ExecutionEngine::GetCurrentTransaction() { 
    return transaction_executor_->GetCurrentTransaction(); 
}

Transaction* ExecutionEngine::GetCurrentTransactionForWrite() {
    return transaction_executor_->GetCurrentTransactionForWrite();
}

// ============================================================================
// FACTORY INITIALIZATION (Reserved for future extension)
// ============================================================================

void ExecutionEngine::InitializeExecutorFactory() {
    // ExecutorRegistry handles registration - see executor_registry.cpp
}

// ============================================================================
// MAIN EXECUTE METHOD - Clean Delegation
// ============================================================================

ExecutionResult ExecutionEngine::Execute(Statement* stmt, SessionContext* session) {
    if (stmt == nullptr) {
        return ExecutionResult::Error("Empty Statement");
    }

    // ==========================================================================
    // CONCURRENCY GATEKEEPER
    // ==========================================================================
    std::unique_lock<std::shared_mutex> exclusive_lock;
    std::shared_lock<std::shared_mutex> shared_lock;

    bool requires_exclusive = (stmt->GetType() == StatementType::RECOVER ||
                               stmt->GetType() == StatementType::CHECKPOINT);

    if (requires_exclusive) {
        exclusive_lock = std::unique_lock<std::shared_mutex>(global_lock_);
    } else {
        shared_lock = std::shared_lock<std::shared_mutex>(global_lock_);
    }

    // ==========================================================================
    // DELEGATION TO SPECIALIZED EXECUTORS
    // ==========================================================================
    try {
        Transaction* txn = GetCurrentTransactionForWrite();
        StatementType type = stmt->GetType();
        ExecutionResult res;
        
        // ----- DDL OPERATIONS (DDLExecutor) -----
        if (type == StatementType::CREATE || type == StatementType::CREATE_TABLE) {
            auto* create_stmt = dynamic_cast<CreateStatement*>(stmt);
            if (session && !session->current_db.empty()) {
                if ((session->current_db == "francodb" || session->current_db == "system") 
                    && session->role != UserRole::SUPERADMIN) {
                    return ExecutionResult::Error("Cannot create tables in reserved database");
                }
            }
            res = ddl_executor_->CreateTable(create_stmt);
        }
        else if (type == StatementType::CREATE_INDEX) {
            res = ddl_executor_->CreateIndex(dynamic_cast<CreateIndexStatement*>(stmt));
        }
        else if (type == StatementType::DROP) {
            if (session && session->current_db == "system" && session->role != UserRole::SUPERADMIN) {
                return ExecutionResult::Error("Cannot drop tables in system database");
            }
            res = ddl_executor_->DropTable(dynamic_cast<DropStatement*>(stmt));
        }
        else if (type == StatementType::DESCRIBE_TABLE) {
            res = ddl_executor_->DescribeTable(dynamic_cast<DescribeTableStatement*>(stmt));
        }
        else if (type == StatementType::SHOW_CREATE_TABLE) {
            res = ddl_executor_->ShowCreateTable(dynamic_cast<ShowCreateTableStatement*>(stmt));
        }
        else if (type == StatementType::ALTER_TABLE) {
            res = ddl_executor_->AlterTable(dynamic_cast<AlterTableStatement*>(stmt));
        }
        
        // ----- DML OPERATIONS (DMLExecutor) -----
        else if (type == StatementType::INSERT) {
            if (session && session->current_db == "system" && session->role != UserRole::SUPERADMIN) {
                return ExecutionResult::Error("Cannot modify system database tables");
            }
            res = dml_executor_->Insert(dynamic_cast<InsertStatement*>(stmt), txn);
        }
        else if (type == StatementType::SELECT) {
            res = dml_executor_->Select(dynamic_cast<SelectStatement*>(stmt), session, txn);
        }
        else if (type == StatementType::UPDATE_CMD) {
            if (session && session->current_db == "system" && session->role != UserRole::SUPERADMIN) {
                return ExecutionResult::Error("Cannot modify system database tables");
            }
            res = dml_executor_->Update(dynamic_cast<UpdateStatement*>(stmt), txn);
        }
        else if (type == StatementType::DELETE_CMD) {
            if (session && session->current_db == "system" && session->role != UserRole::SUPERADMIN) {
                return ExecutionResult::Error("Cannot modify system database tables");
            }
            res = dml_executor_->Delete(dynamic_cast<DeleteStatement*>(stmt), txn);
        }
        
        // ----- TRANSACTION OPERATIONS (TransactionExecutor) -----
        else if (type == StatementType::BEGIN) {
            res = transaction_executor_->Begin();
        }
        else if (type == StatementType::COMMIT) {
            res = transaction_executor_->Commit();
        }
        else if (type == StatementType::ROLLBACK) {
            res = transaction_executor_->Rollback();
        }
        
        // ----- DATABASE OPERATIONS (DatabaseExecutor) -----
        else if (type == StatementType::CREATE_DB) {
            res = database_executor_->CreateDatabase(dynamic_cast<CreateDatabaseStatement*>(stmt), session);
        }
        else if (type == StatementType::USE_DB) {
            BufferPoolManager* new_bpm = nullptr;
            Catalog* new_catalog = nullptr;
            res = database_executor_->UseDatabase(
                dynamic_cast<UseDatabaseStatement*>(stmt), session, &new_bpm, &new_catalog);
            
            // Update engine state after USE DATABASE
            if (res.success && new_bpm && new_catalog) {
                bpm_ = new_bpm;
                catalog_ = new_catalog;
                
                // Update executor context
                delete exec_ctx_;
                exec_ctx_ = new ExecutorContext(bpm_, catalog_, txn, log_manager_);
                
                // Update executors with new catalog
                transaction_executor_->SetCatalog(catalog_);
            }
        }
        else if (type == StatementType::DROP_DB) {
            res = database_executor_->DropDatabase(dynamic_cast<DropDatabaseStatement*>(stmt), session);
        }
        
        // ----- USER OPERATIONS (UserExecutor) -----
        else if (type == StatementType::CREATE_USER) {
            res = user_executor_->CreateUser(dynamic_cast<CreateUserStatement*>(stmt));
        }
        else if (type == StatementType::ALTER_USER_ROLE) {
            res = user_executor_->AlterUserRole(dynamic_cast<AlterUserRoleStatement*>(stmt));
        }
        else if (type == StatementType::DELETE_USER) {
            res = user_executor_->DeleteUser(dynamic_cast<DeleteUserStatement*>(stmt));
        }
        
        // ----- SYSTEM OPERATIONS (SystemExecutor) -----
        else if (type == StatementType::SHOW_DATABASES) {
            res = system_executor_->ShowDatabases(dynamic_cast<ShowDatabasesStatement*>(stmt), session);
        }
        else if (type == StatementType::SHOW_TABLES) {
            res = system_executor_->ShowTables(dynamic_cast<ShowTablesStatement*>(stmt), session);
        }
        else if (type == StatementType::SHOW_STATUS) {
            res = system_executor_->ShowStatus(dynamic_cast<ShowStatusStatement*>(stmt), session);
        }
        else if (type == StatementType::SHOW_USERS) {
            res = system_executor_->ShowUsers(dynamic_cast<ShowUsersStatement*>(stmt));
        }
        else if (type == StatementType::WHOAMI) {
            res = system_executor_->WhoAmI(dynamic_cast<WhoAmIStatement*>(stmt), session);
        }
        
        // ----- RECOVERY OPERATIONS (Kept inline - special handling) -----
        else if (type == StatementType::CHECKPOINT) {
            res = ExecuteCheckpoint();
        }
        else if (type == StatementType::RECOVER) {
            res = ExecuteRecover(dynamic_cast<RecoverStatement*>(stmt));
        }
        
        else {
            return ExecutionResult::Error("Unknown Statement Type");
        }

        // Auto-commit for single DML statements
        if (type == StatementType::INSERT ||
            type == StatementType::UPDATE_CMD ||
            type == StatementType::DELETE_CMD) {
            transaction_executor_->AutoCommitIfNeeded();
        }
        
        return res;
        
    } catch (const std::exception& e) {
        // Force rollback on error
        if (transaction_executor_->GetCurrentTransaction() && 
            transaction_executor_->GetCurrentTransaction()->GetState() == Transaction::TransactionState::RUNNING) {
            transaction_executor_->Rollback();
        }
        return ExecutionResult::Error(e.what());
    }
}

// ============================================================================
// HELPER METHODS
// ============================================================================

std::string ExecutionEngine::ValueToString(const Value& v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

// ============================================================================
// RECOVERY OPERATIONS (Kept inline - require special handling)
// ============================================================================

ExecutionResult ExecutionEngine::ExecuteCheckpoint() {
    CheckpointManager cp_mgr(bpm_, log_manager_);
    cp_mgr.BeginCheckpoint();
    return ExecutionResult::Message("CHECKPOINT SUCCESS");
}

ExecutionResult ExecutionEngine::ExecuteRecover(RecoverStatement* stmt) {
    std::cout << "[SYSTEM] Global Lock Verified. Preparing for Time Travel..." << std::endl;
    
    uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (stmt->timestamp_ > now) {
        return ExecutionResult::Error("Cannot travel to the future! Timestamp is > Now.");
    }
    
    if (stmt->timestamp_ == 0) {
        return ExecutionResult::Error("Invalid timestamp (0).");
    }

    // Force Buffer Pool Flush
    bpm_->FlushAllPages();
    bpm_->Clear(); 
    log_manager_->StopFlushThread();
    
    std::cout << "[SYSTEM] Initiating Time Travel to: " << stmt->timestamp_ << std::endl;

    try {
        CheckpointManager cp_mgr(bpm_, log_manager_);
        RecoveryManager recovery(log_manager_, catalog_, bpm_, &cp_mgr);
        
        recovery.RecoverToTime(stmt->timestamp_);
        cp_mgr.BeginCheckpoint();
        
    } catch (const std::exception& e) {
        return ExecutionResult::Error(std::string("Recovery Failed: ") + e.what());
    }

    std::cout << "[SYSTEM] Time Travel Complete. Resuming normal operations." << std::endl;
    return ExecutionResult::Message("TIME TRAVEL COMPLETE. System state reverted.");
}

} // namespace francodb


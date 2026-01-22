#pragma once

#include "execution/execution_result.h"
#include "concurrency/transaction.h"
#include "recovery/log_manager.h"
#include "catalog/catalog.h"
#include <atomic>

namespace francodb {

/**
 * TransactionExecutor - Transaction control operations
 * 
 * SOLID PRINCIPLE: Single Responsibility
 * This class handles all transaction control operations:
 * - BEGIN
 * - COMMIT
 * - ROLLBACK
 * 
 * Thread Safety: Uses atomic operations for transaction ID generation.
 * 
 * @author FrancoDB Team
 */
class TransactionExecutor {
public:
    TransactionExecutor(LogManager* log_manager, Catalog* catalog)
        : log_manager_(log_manager), 
          catalog_(catalog),
          current_transaction_(nullptr),
          in_explicit_transaction_(false) {}
    
    ~TransactionExecutor() {
        if (current_transaction_) {
            delete current_transaction_;
        }
    }
    
    // ========================================================================
    // TRANSACTION CONTROL
    // ========================================================================
    
    /**
     * Begin a new explicit transaction.
     */
    ExecutionResult Begin();
    
    /**
     * Commit the current transaction.
     */
    ExecutionResult Commit();
    
    /**
     * Rollback the current transaction.
     */
    ExecutionResult Rollback();
    
    /**
     * Auto-commit if not in explicit transaction mode.
     */
    void AutoCommitIfNeeded();
    
    // ========================================================================
    // TRANSACTION STATE ACCESS
    // ========================================================================
    
    /**
     * Get the current transaction (may be nullptr).
     */
    Transaction* GetCurrentTransaction() { return current_transaction_; }
    
    /**
     * Get or create transaction for write operations.
     */
    Transaction* GetCurrentTransactionForWrite();
    
    /**
     * Check if in explicit transaction mode.
     */
    bool IsInExplicitTransaction() const { return in_explicit_transaction_; }
    
    /**
     * Update catalog reference (after USE DATABASE).
     */
    void SetCatalog(Catalog* catalog) { catalog_ = catalog; }
    
    /**
     * Set the next transaction ID counter.
     */
    void SetNextTxnId(std::atomic<int>* next_txn_id) { next_txn_id_ = next_txn_id; }

private:
    LogManager* log_manager_;
    Catalog* catalog_;
    Transaction* current_transaction_;
    bool in_explicit_transaction_;
    std::atomic<int>* next_txn_id_ = nullptr;  // Shared with ExecutionEngine
};

} // namespace francodb


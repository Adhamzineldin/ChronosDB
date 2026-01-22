/**
 * transaction_executor.cpp
 * 
 * Implementation of Transaction Control Operations
 * Extracted from ExecutionEngine to satisfy Single Responsibility Principle.
 * 
 * @author FrancoDB Team
 */

#include "execution/transaction_executor.h"
#include "recovery/log_record.h"
#include "catalog/table_metadata.h"
#include <algorithm>

namespace francodb {

// ============================================================================
// GET TRANSACTION FOR WRITE
// ============================================================================
Transaction* TransactionExecutor::GetCurrentTransactionForWrite() {
    if (current_transaction_ == nullptr) {
        // Atomic increment ensures unique transaction IDs under concurrency
        int txn_id = next_txn_id_->fetch_add(1, std::memory_order_relaxed);
        current_transaction_ = new Transaction(txn_id);
    }
    return current_transaction_;
}

// ============================================================================
// AUTO COMMIT
// ============================================================================
void TransactionExecutor::AutoCommitIfNeeded() {
    if (!in_explicit_transaction_ && current_transaction_ != nullptr &&
        current_transaction_->GetState() == Transaction::TransactionState::RUNNING) {
        Commit();
    }
}

// ============================================================================
// BEGIN
// ============================================================================
ExecutionResult TransactionExecutor::Begin() {
    if (current_transaction_ && in_explicit_transaction_) {
        return ExecutionResult::Error("Transaction in progress");
    }
    
    if (current_transaction_) {
        Commit();
    }

    int txn_id = next_txn_id_->fetch_add(1, std::memory_order_relaxed);
    current_transaction_ = new Transaction(txn_id);
    in_explicit_transaction_ = true;

    // Log BEGIN
    if (log_manager_) {
        LogRecord rec(current_transaction_->GetTransactionId(), 
                      current_transaction_->GetPrevLSN(),
                      LogRecordType::BEGIN);
        auto lsn = log_manager_->AppendLogRecord(rec);
        current_transaction_->SetPrevLSN(lsn);
    }

    return ExecutionResult::Message(
        "BEGIN TRANSACTION " + std::to_string(current_transaction_->GetTransactionId()));
}

// ============================================================================
// COMMIT
// ============================================================================
ExecutionResult TransactionExecutor::Commit() {
    if (current_transaction_) {
        // Log COMMIT and FLUSH
        if (log_manager_) {
            LogRecord rec(current_transaction_->GetTransactionId(), 
                          current_transaction_->GetPrevLSN(),
                          LogRecordType::COMMIT);
            log_manager_->AppendLogRecord(rec);
            // FORCE FLUSH ensures Durability
            log_manager_->Flush(true);
        }

        current_transaction_->SetState(Transaction::TransactionState::COMMITTED);
        delete current_transaction_;
        current_transaction_ = nullptr;
    }
    in_explicit_transaction_ = false;
    return ExecutionResult::Message("COMMIT SUCCESS");
}

// ============================================================================
// ROLLBACK
// ============================================================================
ExecutionResult TransactionExecutor::Rollback() {
    if (!current_transaction_ || !in_explicit_transaction_) {
        return ExecutionResult::Error("No transaction to rollback");
    }

    // Log ABORT
    if (log_manager_) {
        LogRecord rec(current_transaction_->GetTransactionId(), 
                      current_transaction_->GetPrevLSN(),
                      LogRecordType::ABORT);
        log_manager_->AppendLogRecord(rec);
    }

    // In-memory Rollback Logic
    const auto& modifications = current_transaction_->GetModifications();
    std::vector<std::pair<RID, Transaction::TupleModification>> mods_vec;
    for (const auto& [rid, mod] : modifications) {
        mods_vec.push_back({rid, mod});
    }
    std::reverse(mods_vec.begin(), mods_vec.end());

    for (const auto& [rid, mod] : mods_vec) {
        if (mod.table_name.empty()) continue;
        TableMetadata* table_info = catalog_->GetTable(mod.table_name);
        if (!table_info) continue;
        
        if (mod.is_deleted) {
            table_info->table_heap_->UnmarkDelete(rid, nullptr);
        } else if (mod.old_tuple.GetLength() == 0) {
            table_info->table_heap_->MarkDelete(rid, nullptr);
        } else {
            table_info->table_heap_->UnmarkDelete(rid, nullptr);
        }
    }

    current_transaction_->SetState(Transaction::TransactionState::ABORTED);
    delete current_transaction_;
    current_transaction_ = nullptr;
    in_explicit_transaction_ = false;
    
    return ExecutionResult::Message("ROLLBACK SUCCESS");
}

} // namespace francodb


#pragma once

#include "catalog/catalog.h"
#include "storage/storage_interface.h"  // For IBufferManager
#include "concurrency/transaction.h"
#include "concurrency/lock_manager.h"
#include "recovery/log_manager.h"

namespace chronosdb {

    /**
     * ExecutorContext holds global state that all executors need.
     * 
     * CONCURRENCY FIX:
     * Added LockManager for proper row-level locking during DML operations.
     * This fixes the "Bank Problem" data corruption issue.
     */
    class ExecutorContext {
    public:
        // Accept IBufferManager for polymorphic buffer pool usage
        ExecutorContext(IBufferManager *bpm, Catalog *catalog, Transaction *txn, 
                        LogManager *log_manager, LockManager *lock_manager = nullptr)
            : bpm_(bpm), catalog_(catalog), transaction_(txn), 
              log_manager_(log_manager), lock_manager_(lock_manager) {}

        Catalog *GetCatalog() { return catalog_; }
        IBufferManager *GetBufferPoolManager() { return bpm_; }
        Transaction *GetTransaction() { return transaction_; }
        LogManager *GetLogManager() { return log_manager_; }
        LockManager *GetLockManager() { return lock_manager_; }
        
        // Allow updating when switching databases
        void SetCatalog(Catalog *catalog) { catalog_ = catalog; }
        void SetBufferPoolManager(IBufferManager *bpm) { bpm_ = bpm; }
        void SetLockManager(LockManager *lock_manager) { lock_manager_ = lock_manager; }

    private:
        IBufferManager *bpm_;
        Catalog *catalog_;
        Transaction *transaction_;
        LogManager *log_manager_;
        LockManager *lock_manager_;
    };

} // namespace chronosdb
#pragma once

#include "catalog/catalog.h"
#include "buffer/buffer_pool_manager.h"
#include "concurrency/transaction.h"

namespace francodb {

    /**
     * ExecutorContext holds global state (Catalog, BPM, Transaction) that all executors need.
     */
    class ExecutorContext {
    public:
        ExecutorContext(BufferPoolManager *bpm, Catalog *catalog, Transaction *txn)
            : bpm_(bpm), catalog_(catalog), transaction_(txn) {}

        Catalog *GetCatalog() { return catalog_; }
        BufferPoolManager *GetBufferPoolManager() { return bpm_; }
        Transaction *GetTransaction() { return transaction_; }
        
        // Allow updating catalog/bpm when switching databases
        void SetCatalog(Catalog *catalog) { catalog_ = catalog; }
        void SetBufferPoolManager(BufferPoolManager *bpm) { bpm_ = bpm; }

    private:
        BufferPoolManager *bpm_;
        Catalog *catalog_;
        Transaction *transaction_;
    };

} // namespace francodb
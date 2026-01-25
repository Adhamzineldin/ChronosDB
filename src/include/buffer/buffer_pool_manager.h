#pragma once

#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <atomic> // Don't forget atomic!

#include "storage/disk/disk_manager.h"
#include "storage/page/page.h"
#include "buffer/replacer.h" // Use the generic interface
#include "storage/storage_interface.h"  // For IBufferManager

namespace francodb {

    // Forward declaration
    class LogManager;

    /**
     * BufferPoolManager - Standard buffer pool with single mutex.
     * 
     * For high-concurrency workloads, consider using PartitionedBufferPoolManager
     * which reduces lock contention via partitioned latching.
     * 
     * Implements IBufferManager interface for polymorphic usage.
     */
    class BufferPoolManager : public IBufferManager {
    public:
        BufferPoolManager(size_t pool_size, DiskManager *disk_manager);
        ~BufferPoolManager() override;

        Page *FetchPage(page_id_t page_id) override;
        bool UnpinPage(page_id_t page_id, bool is_dirty) override;
        bool FlushPage(page_id_t page_id) override;
        Page *NewPage(page_id_t *page_id) override;
        bool DeletePage(page_id_t page_id) override;
        void FlushAllPages() override;
        
        DiskManager *GetDiskManager() override { return disk_manager_; }
        
        /**
         * Get the current pool size in pages.
         */
        size_t GetPoolSize() const override { return pool_size_; }
        
        /**
         * Set the log manager for WAL protocol enforcement.
         * When set, FlushPage will ensure the log is flushed
         * up to the page's LSN before writing data.
         */
        void SetLogManager(LogManager* log_manager) override { log_manager_ = log_manager; }

        void Clear() override;

    private:
        bool FindFreeFrame(frame_id_t *out_frame_id);

        size_t pool_size_;
        Page *pages_;
        DiskManager *disk_manager_;
        LogManager *log_manager_ = nullptr;  // Optional - for WAL protocol
    
        // Changed from LRUReplacer* to Replacer* (Polymorphism!)
        Replacer *replacer_; 
    
        std::unordered_map<page_id_t, frame_id_t> page_table_;
        std::list<frame_id_t> free_list_;
        std::mutex latch_;
    
        // Next Page ID Tracker
        std::atomic<page_id_t> next_page_id_ = 0;
    };

} // namespace francodb
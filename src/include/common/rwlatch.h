#pragma once

#include <shared_mutex>
#include <mutex>

namespace chronosdb {

    /**
     * Reader-Writer Latch.
     * Allows multiple readers OR a single writer.
     */
    class ReaderWriterLatch {
    public:
        // Acquire Write Latch (Exclusive)
        void WLock() { mutex_.lock(); }
        
        // Release Write Latch
        void WUnlock() { mutex_.unlock(); }
        
        // Acquire Read Latch (Shared)
        void RLock() { mutex_.lock_shared(); }
        
        // Release Read Latch
        void RUnlock() { mutex_.unlock_shared(); }

    private:
        std::shared_mutex mutex_;
    };

} // namespace chronosdb
#pragma once

#include <cstdint>

namespace francodb {
    // ========================================================================
    // TYPE ALIASES
    // ========================================================================
    
    // We use signed integers so we can check if(id < 0) for errors.
    using page_id_t = int32_t;
    using frame_id_t = int32_t;
    using txn_id_t = int32_t;
    using lsn_t = int32_t;
    using slot_id_t = uint32_t;

    // ========================================================================
    // STORAGE LAYOUT
    // ========================================================================
    
    // 4KB page size matches typical OS page size
    static constexpr uint32_t PAGE_SIZE = 4096;
    
    // Reserved page IDs
    static constexpr page_id_t METADATA_PAGE_ID = 0;    // Database metadata
    static constexpr page_id_t CATALOG_PAGE_ID = 1;     // System catalog
    static constexpr page_id_t BITMAP_PAGE_ID = 2;      // Free space bitmap
    static constexpr page_id_t FIRST_DATA_PAGE_ID = 3;  // First user data page
    
    // Invalid markers
    static constexpr page_id_t INVALID_PAGE_ID = -1;
    static constexpr txn_id_t INVALID_TXN_ID = -1;
    static constexpr lsn_t INVALID_LSN = -1;
    static constexpr slot_id_t INVALID_SLOT_ID = UINT32_MAX;

    // ========================================================================
    // BUFFER POOL
    // ========================================================================
    
    // Default number of pages the BufferPoolManager can hold in memory.
    // This is the INITIAL size - can be configured via command line or config file.
    // 256MB = 65536 pages * 4KB
    static constexpr uint32_t BUFFER_POOL_SIZE = 65536;
    
    // Adaptive Buffer Pool Bounds (in pages)
    static constexpr size_t BUFFER_POOL_MIN_SIZE = 16384;     // 64MB minimum
    static constexpr size_t BUFFER_POOL_MAX_SIZE = 524288;    // 2GB maximum
    static constexpr size_t BUFFER_POOL_CHUNK_SIZE = 32768;   // 128MB growth chunks
    
    // Number of buffer pool partitions for reduced contention
    static constexpr size_t BUFFER_POOL_PARTITIONS = 16;
    
    // Enable partitioned buffer pool for high-concurrency workloads
    // When true, uses PartitionedBufferPoolManager with 16 independent partitions
    // This reduces lock contention under heavy concurrent access
    static constexpr bool USE_PARTITIONED_BUFFER_POOL = false;
    
    // Enable adaptive buffer pool sizing
    // When true, pool will grow/shrink based on metrics (hit rate, eviction rate)
    static constexpr bool USE_ADAPTIVE_BUFFER_POOL = false;  // Phase 2 feature
    
    // Enable adaptive DISTRIBUTED buffer pool (RECOMMENDED for production)
    // Combines: 16 partitions + per-partition adaptive sizing + hot/cold rebalancing
    // Best option for high-concurrency + variable workloads
    static constexpr bool USE_ADAPTIVE_DISTRIBUTED_POOL = true;  // Set to true for production
    
    // Eviction batch size (for background eviction)
    static constexpr size_t EVICTION_BATCH_SIZE = 8;
    
    // Adaptation thresholds for adaptive pool
    static constexpr double BUFFER_POOL_HIT_RATE_GROW_THRESHOLD = 90.0;
    static constexpr double BUFFER_POOL_HIT_RATE_SHRINK_THRESHOLD = 98.0;
    static constexpr double BUFFER_POOL_DIRTY_RATIO_THROTTLE = 70.0;
    static constexpr uint32_t BUFFER_POOL_ADAPTATION_INTERVAL_SEC = 30;

    // ========================================================================
    // TABLE PAGE LAYOUT
    // ========================================================================
    
    // Table page header:
    // [page_id (4)] [prev_page (4)] [next_page (4)] [free_space_ptr (4)]
    // [tuple_count (4)] [checksum (4)] = 24 bytes
    static constexpr size_t TABLE_PAGE_HEADER_SIZE = 24;
    
    // Slot entry: [offset (4)] [size (4)] = 8 bytes
    static constexpr size_t TABLE_PAGE_SLOT_SIZE = 8;
    
    // Maximum tuple size (page size - header - one slot)
    static constexpr size_t MAX_TUPLE_SIZE = PAGE_SIZE - TABLE_PAGE_HEADER_SIZE - TABLE_PAGE_SLOT_SIZE;

    // ========================================================================
    // INDEX (B+ TREE)
    // ========================================================================
    
    // Default key size for index keys
    static constexpr size_t DEFAULT_KEY_SIZE = 8;
    
    // Maximum key size supported
    static constexpr size_t MAX_KEY_SIZE = 256;
    
    // B+ tree node fanout (affects tree height and I/O)
    static constexpr size_t BTREE_MAX_FANOUT = 128;
    static constexpr size_t BTREE_MIN_FANOUT = BTREE_MAX_FANOUT / 2;

    // ========================================================================
    // LOGGING & RECOVERY
    // ========================================================================
    
    // Log buffer size (64KB default)
    static constexpr size_t LOG_BUFFER_SIZE = 64 * 1024;
    
    // Checkpoint interval (in number of log records)
    static constexpr size_t CHECKPOINT_INTERVAL = 1000;
    
    // Checkpoint interval (in milliseconds)
    static constexpr uint64_t CHECKPOINT_INTERVAL_MS = 60000;  // 1 minute
    
    // Maximum log record size
    static constexpr size_t MAX_LOG_RECORD_SIZE = PAGE_SIZE;

    // ========================================================================
    // CONCURRENCY
    // ========================================================================
    
    // Maximum concurrent transactions
    static constexpr size_t MAX_TRANSACTIONS = 1024;
    
    // Lock table size (hash buckets)
    static constexpr size_t LOCK_TABLE_SIZE = 1024;
    
    // Deadlock detection interval (milliseconds)
    static constexpr uint64_t DEADLOCK_DETECTION_INTERVAL_MS = 1000;

    // ========================================================================
    // NETWORK
    // ========================================================================
    
    // Default server port
    static constexpr uint16_t DEFAULT_PORT = 2501;
    
    // Maximum client connections
    static constexpr size_t MAX_CONNECTIONS = 100;
    
    // Connection timeout (seconds)
    static constexpr uint32_t CONNECTION_TIMEOUT_SEC = 30;
    
    // Maximum query size (1MB)
    static constexpr size_t MAX_QUERY_SIZE = 1024 * 1024;

} // namespace francodb



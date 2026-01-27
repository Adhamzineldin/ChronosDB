#pragma once

#include "recovery/table_snapshot.h"
#include "recovery/checkpoint_index.h"
#include "storage/table/table_heap.h"
#include "storage/storage_interface.h"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace chronosdb {

    /**
     * CheckpointSnapshotManager - Manages persistence and loading of table snapshots
     *
     * Responsibilities:
     * 1. Create snapshots during checkpoint process
     * 2. Load snapshots for AS OF / RECOVER TO queries
     * 3. Manage snapshot lifecycle (cleanup old snapshots)
     * 4. Provide snapshot metadata and discovery
     *
     * Directory Structure:
     * data/<database>/checkpoints/<checkpoint_lsn>/<table_name>.snap
     *
     * Design Principles:
     * - Single Responsibility: Only handles snapshot I/O, not checkpoint logic
     * - Dependency Inversion: Depends on CheckpointIndex abstraction
     * - Error Resilience: Failures don't corrupt existing data
     * - Performance: Lazy loading, batch operations where possible
     */
    class CheckpointSnapshotManager {
    public:
        /**
         * Constructor
         *
         * @param base_path Base directory for all databases (e.g., "data/")
         * @param checkpoint_index Checkpoint index for finding nearest snapshots
         */
        explicit CheckpointSnapshotManager(
            const std::string& base_path,
            CheckpointIndex* checkpoint_index = nullptr);

        /**
         * Set the checkpoint index (for lazy initialization)
         */
        void SetCheckpointIndex(CheckpointIndex* index) {
            checkpoint_index_ = index;
        }

        // ====================================================================
        // SNAPSHOT CREATION
        // ====================================================================

        /**
         * Create and persist a table snapshot
         *
         * Called during checkpoint process. Creates snapshot in:
         * <base_path>/<db_name>/checkpoints/<checkpoint_lsn>/<table_name>.snap
         *
         * @param db_name Database name
         * @param table_name Table name
         * @param heap Table heap to snapshot
         * @param schema Table schema
         * @param checkpoint_lsn LSN of checkpoint
         * @param timestamp Checkpoint timestamp
         * @return true on success, false on error
         */
        bool CreateTableSnapshot(
            const std::string& db_name,
            const std::string& table_name,
            TableHeap* heap,
            const Schema& schema,
            LogRecord::lsn_t checkpoint_lsn,
            uint64_t timestamp);

        // ====================================================================
        // SNAPSHOT LOADING
        // ====================================================================

        /**
         * Load the nearest snapshot BEFORE target_time
         *
         * Uses checkpoint index to find optimal snapshot, then loads it.
         * Returns nullptr if no suitable snapshot exists.
         *
         * @param db_name Database name
         * @param table_name Table name
         * @param target_time Target timestamp (find nearest before this)
         * @return Snapshot object or nullptr
         */
        std::unique_ptr<TableSnapshot> LoadNearestSnapshot(
            const std::string& db_name,
            const std::string& table_name,
            uint64_t target_time);

        /**
         * Load a specific snapshot by LSN
         *
         * @param db_name Database name
         * @param table_name Table name
         * @param checkpoint_lsn Checkpoint LSN
         * @return Snapshot object or nullptr
         */
        std::unique_ptr<TableSnapshot> LoadSnapshot(
            const std::string& db_name,
            const std::string& table_name,
            LogRecord::lsn_t checkpoint_lsn);

        // ====================================================================
        // SNAPSHOT MANAGEMENT
        // ====================================================================

        /**
         * Clean up old snapshots, keeping only the most recent N
         *
         * @param db_name Database name
         * @param table_name Table name (empty = all tables)
         * @param keep_count Number of snapshots to keep
         * @return Number of snapshots deleted
         */
        int CleanupOldSnapshots(
            const std::string& db_name,
            const std::string& table_name,
            int keep_count);

        /**
         * List all snapshots for a table
         *
         * @param db_name Database name
         * @param table_name Table name
         * @return List of checkpoint LSNs that have snapshots
         */
        std::vector<LogRecord::lsn_t> ListSnapshots(
            const std::string& db_name,
            const std::string& table_name);

        /**
         * Check if a snapshot exists
         *
         * @param db_name Database name
         * @param table_name Table name
         * @param checkpoint_lsn Checkpoint LSN
         * @return true if snapshot file exists
         */
        bool SnapshotExists(
            const std::string& db_name,
            const std::string& table_name,
            LogRecord::lsn_t checkpoint_lsn);

        /**
         * Get total disk space used by snapshots
         *
         * @param db_name Database name (empty = all databases)
         * @return Total size in bytes
         */
        size_t GetSnapshotsDiskUsage(const std::string& db_name);

        // ====================================================================
        // CONFIGURATION
        // ====================================================================

        /**
         * Enable/disable snapshot creation
         */
        void SetEnabled(bool enabled) { enabled_ = enabled; }

        /**
         * Check if snapshots are enabled
         */
        bool IsEnabled() const { return enabled_; }

    private:
        std::string base_path_;
        CheckpointIndex* checkpoint_index_;
        bool enabled_;

        /**
         * Build path to snapshot file
         */
        std::string GetSnapshotPath(
            const std::string& db_name,
            const std::string& table_name,
            LogRecord::lsn_t checkpoint_lsn) const;

        /**
         * Build path to checkpoint directory
         */
        std::string GetCheckpointDir(
            const std::string& db_name,
            LogRecord::lsn_t checkpoint_lsn) const;

        /**
         * Build path to database checkpoints directory
         */
        std::string GetDatabaseCheckpointsDir(const std::string& db_name) const;
    };

} // namespace chronosdb

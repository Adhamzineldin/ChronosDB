#include "recovery/checkpoint_snapshot_manager.h"
#include <iostream>
#include <algorithm>

namespace chronosdb {

    // ========================================================================
    // CONSTRUCTOR
    // ========================================================================

    CheckpointSnapshotManager::CheckpointSnapshotManager(
        const std::string& base_path,
        CheckpointIndex* checkpoint_index)
        : base_path_(base_path),
          checkpoint_index_(checkpoint_index),
          enabled_(true) {

        std::cout << "[CheckpointSnapshotManager] Initialized with base path: " << base_path << std::endl;
    }

    // ========================================================================
    // SNAPSHOT CREATION
    // ========================================================================

    bool CheckpointSnapshotManager::CreateTableSnapshot(
        const std::string& db_name,
        const std::string& table_name,
        TableHeap* heap,
        const Schema& schema,
        LogRecord::lsn_t checkpoint_lsn,
        uint64_t timestamp) {

        if (!enabled_) {
            std::cout << "[CheckpointSnapshotManager] Snapshots disabled - skipping " << table_name << std::endl;
            return false;
        }

        if (!heap) {
            std::cerr << "[CheckpointSnapshotManager] ERROR: Null heap pointer" << std::endl;
            return false;
        }

        std::cout << "[CheckpointSnapshotManager] Creating snapshot for table '" << table_name
                  << "' at LSN " << checkpoint_lsn << std::endl;

        // Create snapshot from table
        auto snapshot = TableSnapshot::CreateFromTable(heap, table_name, schema, checkpoint_lsn, timestamp);

        if (!snapshot) {
            std::cerr << "[CheckpointSnapshotManager] Failed to create snapshot" << std::endl;
            return false;
        }

        // Save to disk
        std::string file_path = GetSnapshotPath(db_name, table_name, checkpoint_lsn);

        if (!snapshot->SaveToFile(file_path)) {
            std::cerr << "[CheckpointSnapshotManager] Failed to save snapshot to " << file_path << std::endl;
            return false;
        }

        std::cout << "[CheckpointSnapshotManager] Snapshot saved successfully to " << file_path << std::endl;
        return true;
    }

    // ========================================================================
    // SNAPSHOT LOADING
    // ========================================================================

    std::unique_ptr<TableSnapshot> CheckpointSnapshotManager::LoadNearestSnapshot(
        const std::string& db_name,
        const std::string& table_name,
        uint64_t target_time) {

        if (!enabled_) {
            return nullptr;
        }

        if (!checkpoint_index_) {
            std::cerr << "[CheckpointSnapshotManager] No checkpoint index available" << std::endl;
            return nullptr;
        }

        // Find nearest checkpoint before target time
        const CheckpointEntry* nearest = checkpoint_index_->FindNearestBefore(target_time);

        if (!nearest) {
            std::cout << "[CheckpointSnapshotManager] No checkpoint found before target time" << std::endl;
            return nullptr;
        }

        std::cout << "[CheckpointSnapshotManager] Found checkpoint at LSN " << nearest->lsn
                  << ", timestamp " << nearest->timestamp << std::endl;

        // Load snapshot
        return LoadSnapshot(db_name, table_name, nearest->lsn);
    }

    std::unique_ptr<TableSnapshot> CheckpointSnapshotManager::LoadSnapshot(
        const std::string& db_name,
        const std::string& table_name,
        LogRecord::lsn_t checkpoint_lsn) {

        if (!enabled_) {
            return nullptr;
        }

        std::string file_path = GetSnapshotPath(db_name, table_name, checkpoint_lsn);

        if (!std::filesystem::exists(file_path)) {
            std::cout << "[CheckpointSnapshotManager] Snapshot not found: " << file_path << std::endl;
            return nullptr;
        }

        std::cout << "[CheckpointSnapshotManager] Loading snapshot from " << file_path << std::endl;

        auto snapshot = TableSnapshot::LoadFromFile(file_path);

        if (!snapshot) {
            std::cerr << "[CheckpointSnapshotManager] Failed to load snapshot" << std::endl;
            return nullptr;
        }

        std::cout << "[CheckpointSnapshotManager] Snapshot loaded successfully ("
                  << snapshot->GetRowCount() << " rows)" << std::endl;

        return snapshot;
    }

    // ========================================================================
    // SNAPSHOT MANAGEMENT
    // ========================================================================

    int CheckpointSnapshotManager::CleanupOldSnapshots(
        const std::string& db_name,
        const std::string& table_name,
        int keep_count) {

        std::cout << "[CheckpointSnapshotManager] Cleaning up old snapshots for " << table_name
                  << " (keeping " << keep_count << ")" << std::endl;

        // Get all snapshots for this table
        auto snapshots = ListSnapshots(db_name, table_name);

        if (snapshots.size() <= static_cast<size_t>(keep_count)) {
            std::cout << "[CheckpointSnapshotManager]   No cleanup needed (" << snapshots.size() << " snapshots)" << std::endl;
            return 0;
        }

        // Sort by LSN (ascending), keep most recent
        std::sort(snapshots.begin(), snapshots.end());

        // Delete oldest snapshots
        int deleted = 0;
        size_t to_delete = snapshots.size() - keep_count;

        for (size_t i = 0; i < to_delete; i++) {
            std::string file_path = GetSnapshotPath(db_name, table_name, snapshots[i]);

            try {
                if (std::filesystem::remove(file_path)) {
                    deleted++;
                    std::cout << "[CheckpointSnapshotManager]   Deleted old snapshot: LSN " << snapshots[i] << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[CheckpointSnapshotManager]   Failed to delete snapshot: " << e.what() << std::endl;
            }
        }

        std::cout << "[CheckpointSnapshotManager] Cleanup complete. Deleted " << deleted << " snapshots" << std::endl;
        return deleted;
    }

    std::vector<LogRecord::lsn_t> CheckpointSnapshotManager::ListSnapshots(
        const std::string& db_name,
        const std::string& table_name) {

        std::vector<LogRecord::lsn_t> result;

        std::string db_checkpoints_dir = GetDatabaseCheckpointsDir(db_name);

        if (!std::filesystem::exists(db_checkpoints_dir)) {
            return result;
        }

        // Iterate through checkpoint directories
        for (const auto& entry : std::filesystem::directory_iterator(db_checkpoints_dir)) {
            if (!entry.is_directory()) continue;

            // Parse LSN from directory name
            std::string dir_name = entry.path().filename().string();
            try {
                LogRecord::lsn_t lsn = std::stoull(dir_name);

                // Check if snapshot exists for this table
                std::string snapshot_path = GetSnapshotPath(db_name, table_name, lsn);
                if (std::filesystem::exists(snapshot_path)) {
                    result.push_back(lsn);
                }
            } catch (...) {
                // Invalid directory name, skip
                continue;
            }
        }

        return result;
    }

    bool CheckpointSnapshotManager::SnapshotExists(
        const std::string& db_name,
        const std::string& table_name,
        LogRecord::lsn_t checkpoint_lsn) {

        std::string file_path = GetSnapshotPath(db_name, table_name, checkpoint_lsn);
        return std::filesystem::exists(file_path);
    }

    size_t CheckpointSnapshotManager::GetSnapshotsDiskUsage(const std::string& db_name) {
        size_t total_size = 0;

        std::string base_dir = db_name.empty() ? base_path_ : GetDatabaseCheckpointsDir(db_name);

        if (!std::filesystem::exists(base_dir)) {
            return 0;
        }

        // Recursively calculate size
        for (const auto& entry : std::filesystem::recursive_directory_iterator(base_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".snap") {
                try {
                    total_size += entry.file_size();
                } catch (...) {
                    // Ignore errors
                }
            }
        }

        return total_size;
    }

    // ========================================================================
    // PRIVATE HELPERS
    // ========================================================================

    std::string CheckpointSnapshotManager::GetSnapshotPath(
        const std::string& db_name,
        const std::string& table_name,
        LogRecord::lsn_t checkpoint_lsn) const {

        std::string checkpoint_dir = GetCheckpointDir(db_name, checkpoint_lsn);
        return checkpoint_dir + "/" + table_name + ".snap";
    }

    std::string CheckpointSnapshotManager::GetCheckpointDir(
        const std::string& db_name,
        LogRecord::lsn_t checkpoint_lsn) const {

        return GetDatabaseCheckpointsDir(db_name) + "/" + std::to_string(checkpoint_lsn);
    }

    std::string CheckpointSnapshotManager::GetDatabaseCheckpointsDir(const std::string& db_name) const {
        return base_path_ + "/" + db_name + "/checkpoints";
    }

} // namespace chronosdb

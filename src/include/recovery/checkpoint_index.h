#pragma once

#include "recovery/log_record.h"
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace chronosdb {

    /**
     * Checkpoint Entry for efficient time-travel lookups.
     *
     * Each entry represents a checkpoint in the log with:
     * - lsn: The LSN of the CHECKPOINT_END record
     * - timestamp: When the checkpoint was taken
     * - log_offset: File position AFTER the checkpoint record
     *               (where replay should start from)
     */
    struct CheckpointEntry {
        LogRecord::lsn_t lsn;
        LogRecord::timestamp_t timestamp;
        std::streampos log_offset;

        CheckpointEntry()
            : lsn(LogRecord::INVALID_LSN), timestamp(0), log_offset(0) {}

        CheckpointEntry(LogRecord::lsn_t l, LogRecord::timestamp_t ts, std::streampos offset)
            : lsn(l), timestamp(ts), log_offset(offset) {}

        // For sorting by timestamp
        bool operator<(const CheckpointEntry& other) const {
            return timestamp < other.timestamp;
        }
    };

    /**
     * Checkpoint Index for O(log K) Time-Travel Optimization
     *
     * The Problem:
     * ============
     * Current time travel (SELECT AS OF, RECOVER TO) operations replay
     * from LSN 0 every time. This is O(N) where N = total log records.
     *
     * The Solution:
     * =============
     * Build an index of all checkpoints sorted by timestamp.
     * Binary search to find the nearest checkpoint BEFORE the target time.
     * Replay only from that checkpoint offset to the target time.
     *
     * Complexity Improvement:
     * =======================
     * - Before: O(N) - replay entire log
     * - After: O(log K) lookup + O(D) replay, where:
     *   - K = number of checkpoints
     *   - D = records between nearest checkpoint and target time
     *
     * Example:
     * - Log has 1M records, checkpoint every 10K, query 5 min ago (10K records back)
     * - Before: Replay 1,000,000 records
     * - After: Binary search (7 comparisons) + replay 10,000 records
     * - 100x improvement!
     */
    class CheckpointIndex {
    public:
        CheckpointIndex() = default;

        /**
         * Find the nearest checkpoint BEFORE the target time.
         * Uses binary search for O(log K) complexity.
         *
         * @param target_time Target timestamp in microseconds
         * @return Pointer to the nearest checkpoint entry, or nullptr if none exists
         */
        const CheckpointEntry* FindNearestBefore(LogRecord::timestamp_t target_time) const {
            if (entries_.empty()) {
                return nullptr;
            }

            // Binary search for largest timestamp <= target_time
            // We want the last checkpoint that happened BEFORE our target

            // Create a dummy entry for comparison
            CheckpointEntry target;
            target.timestamp = target_time;

            // upper_bound finds first element > target, so prev is <= target
            auto it = std::upper_bound(entries_.begin(), entries_.end(), target,
                [](const CheckpointEntry& a, const CheckpointEntry& b) {
                    return a.timestamp < b.timestamp;
                });

            if (it == entries_.begin()) {
                // All checkpoints are after target_time
                return nullptr;
            }

            // Return the checkpoint just before the upper_bound
            --it;
            return &(*it);
        }

        /**
         * Add a new checkpoint entry.
         * Maintains sorted order by timestamp.
         *
         * @param lsn LSN of the CHECKPOINT_END record
         * @param timestamp Timestamp of the checkpoint
         * @param offset File offset after the checkpoint record
         */
        void AddCheckpoint(LogRecord::lsn_t lsn, LogRecord::timestamp_t timestamp, std::streampos offset) {
            entries_.emplace_back(lsn, timestamp, offset);

            // Keep sorted by timestamp (typically already in order)
            if (entries_.size() > 1) {
                auto& prev = entries_[entries_.size() - 2];
                if (timestamp < prev.timestamp) {
                    // Out of order - need to re-sort
                    std::sort(entries_.begin(), entries_.end());
                }
            }

            std::cout << "[CheckpointIndex] Added checkpoint: LSN=" << lsn
                      << ", timestamp=" << timestamp
                      << ", offset=" << offset << std::endl;
        }

        /**
         * Build the index by scanning an existing log file.
         * This is a one-time operation done at startup.
         *
         * @param log_path Path to the log file
         * @return Number of checkpoints found
         */
        size_t BuildFromLog(const std::string& log_path);

        /**
         * Save the index to a binary file for fast loading.
         *
         * @param path Path to save the index
         * @return true on success
         */
        bool SaveToFile(const std::string& path) const {
            try {
                std::filesystem::path fs_path(path);
                std::filesystem::create_directories(fs_path.parent_path());

                std::ofstream file(path, std::ios::binary | std::ios::trunc);
                if (!file.is_open()) {
                    std::cerr << "[CheckpointIndex] Cannot create index file: " << path << std::endl;
                    return false;
                }

                // Write header
                uint32_t magic = 0x43504958;  // "CPIX" - Checkpoint Index
                uint32_t version = 1;
                uint32_t count = static_cast<uint32_t>(entries_.size());

                file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
                file.write(reinterpret_cast<const char*>(&version), sizeof(version));
                file.write(reinterpret_cast<const char*>(&count), sizeof(count));

                // Write entries
                for (const auto& entry : entries_) {
                    file.write(reinterpret_cast<const char*>(&entry.lsn), sizeof(entry.lsn));
                    file.write(reinterpret_cast<const char*>(&entry.timestamp), sizeof(entry.timestamp));
                    file.write(reinterpret_cast<const char*>(&entry.log_offset), sizeof(entry.log_offset));
                }

                file.flush();
                file.close();

                std::cout << "[CheckpointIndex] Saved " << count << " checkpoints to " << path << std::endl;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "[CheckpointIndex] Error saving index: " << e.what() << std::endl;
                return false;
            }
        }

        /**
         * Load the index from a binary file.
         *
         * @param path Path to load the index from
         * @return true on success
         */
        bool LoadFromFile(const std::string& path) {
            try {
                if (!std::filesystem::exists(path)) {
                    return false;
                }

                std::ifstream file(path, std::ios::binary);
                if (!file.is_open()) {
                    return false;
                }

                // Read header
                uint32_t magic, version, count;
                file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
                file.read(reinterpret_cast<char*>(&version), sizeof(version));
                file.read(reinterpret_cast<char*>(&count), sizeof(count));

                if (magic != 0x43504958) {
                    std::cerr << "[CheckpointIndex] Invalid index file magic number" << std::endl;
                    return false;
                }

                if (version > 1) {
                    std::cerr << "[CheckpointIndex] Unsupported index version: " << version << std::endl;
                    return false;
                }

                // Read entries
                entries_.clear();
                entries_.reserve(count);

                for (uint32_t i = 0; i < count; i++) {
                    CheckpointEntry entry;
                    file.read(reinterpret_cast<char*>(&entry.lsn), sizeof(entry.lsn));
                    file.read(reinterpret_cast<char*>(&entry.timestamp), sizeof(entry.timestamp));
                    file.read(reinterpret_cast<char*>(&entry.log_offset), sizeof(entry.log_offset));
                    entries_.push_back(entry);
                }

                file.close();

                std::cout << "[CheckpointIndex] Loaded " << count << " checkpoints from " << path << std::endl;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "[CheckpointIndex] Error loading index: " << e.what() << std::endl;
                return false;
            }
        }

        /**
         * Clear all entries
         */
        void Clear() {
            entries_.clear();
        }

        /**
         * Get the number of checkpoint entries
         */
        size_t Size() const {
            return entries_.size();
        }

        /**
         * Check if the index is empty
         */
        bool Empty() const {
            return entries_.empty();
        }

        /**
         * Get all entries (for debugging/testing)
         */
        const std::vector<CheckpointEntry>& GetEntries() const {
            return entries_;
        }

    private:
        std::vector<CheckpointEntry> entries_;  // Sorted by timestamp
    };

} // namespace chronosdb

#pragma once

#include "recovery/recovery_manager.h"
#include "recovery/log_manager.h"
#include "recovery/checkpoint_manager.h"
#include "storage/table/table_heap.h"
#include "catalog/catalog.h"
#include "catalog/table_metadata.h"
#include "storage/storage_interface.h"
#include <memory>
#include <chrono>
#include <map>
#include <mutex>
#include <iostream>

namespace francodb {

    /**
     * Snapshot Cache Entry
     * Stores a base snapshot at a specific LSN for efficient time-travel queries
     */
    struct SnapshotCacheEntry {
        std::unique_ptr<TableHeap> base_heap{nullptr};
        LogRecord::lsn_t base_lsn{LogRecord::INVALID_LSN};
        uint64_t base_timestamp{0};
        std::chrono::steady_clock::time_point created_at{};
        uint64_t hit_count{0};
    };

    /**
     * Snapshot Manager - "Git Checkout --Detached" for Data
     * 
     * Performance Optimization (Bug #6 Fix):
     * ======================================
     * Instead of replaying from LSN 0 for every snapshot query, we maintain
     * a cache of "base snapshots" at checkpoint boundaries. Queries then only
     * need to replay from the nearest base snapshot to the target time.
     */
    class SnapshotManager {
    public:
        static constexpr size_t MAX_CACHE_ENTRIES_PER_TABLE = 5;
        static constexpr LogRecord::lsn_t MIN_LSN_GAP_FOR_CACHE = 1000;

        /**
         * Build a snapshot of a table at a specific timestamp.
         * Uses cached base snapshots when available (Bug #6 fix).
         */
        static std::unique_ptr<TableHeap> BuildSnapshot(
            const std::string& table_name,
            uint64_t target_time, 
            IBufferManager* bpm, 
            LogManager* log_manager,
            Catalog* catalog,
            const std::string& db_name = "") 
        {
            std::string target_db = db_name;
            if (target_db.empty() && log_manager) {
                target_db = log_manager->GetCurrentDatabase();
            }
            
            std::cout << "[SnapshotManager] Building snapshot for table '" << table_name 
                      << "' from database '" << target_db << "'" << std::endl;

            // Check cache for a suitable base snapshot
            std::string cache_key = target_db + "." + table_name;
            std::unique_ptr<TableHeap> result_heap = nullptr;
            LogRecord::lsn_t cached_lsn = LogRecord::INVALID_LSN;
            
            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                auto it = snapshot_cache_.find(cache_key);
                if (it != snapshot_cache_.end() && it->second.base_heap) {
                    // Found cached snapshot - check if it's usable
                    if (it->second.base_timestamp <= target_time) {
                        cached_lsn = it->second.base_lsn;
                        it->second.hit_count++;
                        
                        // Clone the cached heap as our starting point
                        result_heap = CloneTableHeap(it->second.base_heap.get(), bpm);
                        
                        std::cout << "[SnapshotManager] Using cached base at LSN " << cached_lsn 
                                  << " (hit count: " << it->second.hit_count << ")" << std::endl;
                    }
                }
            }
            
            if (!result_heap) {
                // No cache hit - create fresh heap and replay from beginning
                result_heap = std::make_unique<TableHeap>(bpm, nullptr);
                RecoveryManager recovery(log_manager, catalog, bpm, nullptr);
                recovery.ReplayIntoHeap(result_heap.get(), table_name, target_time, target_db);
                
                // Consider caching this snapshot for future queries
                LogRecord::lsn_t current_lsn = log_manager ? log_manager->GetNextLSN() : 0;
                if (current_lsn >= MIN_LSN_GAP_FOR_CACHE) {
                    std::lock_guard<std::mutex> lock(cache_mutex_);
                    
                    // Only cache if we don't already have a recent snapshot
                    auto it = snapshot_cache_.find(cache_key);
                    bool should_cache = true;
                    if (it != snapshot_cache_.end()) {
                        should_cache = (current_lsn - it->second.base_lsn >= MIN_LSN_GAP_FOR_CACHE);
                    }
                    
                    if (should_cache) {
                        SnapshotCacheEntry entry;
                        entry.base_heap = CloneTableHeap(result_heap.get(), bpm);
                        entry.base_lsn = current_lsn;
                        entry.base_timestamp = target_time;
                        entry.created_at = std::chrono::steady_clock::now();
                        snapshot_cache_[cache_key] = std::move(entry);
                        
                        std::cout << "[SnapshotManager] Cached snapshot at LSN " << current_lsn << std::endl;
                    }
                }
            }
            
            return result_heap;
        }

        /**
         * Build a snapshot from a human-readable timestamp string.
         */
        static std::unique_ptr<TableHeap> BuildSnapshotFromString(
            const std::string& table_name,
            const std::string& timestamp_str,
            IBufferManager* bpm,
            LogManager* log_manager,
            Catalog* catalog)
        {
            uint64_t target_time = ParseTimestamp(timestamp_str);
            return BuildSnapshot(table_name, target_time, bpm, log_manager, catalog);
        }

        /**
         * Build a snapshot at a relative time offset from now.
         */
        static std::unique_ptr<TableHeap> BuildSnapshotSecondsAgo(
            const std::string& table_name,
            uint64_t seconds_ago,
            IBufferManager* bpm,
            LogManager* log_manager,
            Catalog* catalog)
        {
            uint64_t current = LogRecord::GetCurrentTimestamp();
            uint64_t target = current - (seconds_ago * 1000000ULL);
            return BuildSnapshot(table_name, target, bpm, log_manager, catalog);
        }

        static uint64_t GetCurrentTimestamp() {
            return LogRecord::GetCurrentTimestamp();
        }

        static std::string TimestampToString(uint64_t timestamp) {
            time_t seconds = static_cast<time_t>(timestamp / 1000000ULL);
            char buffer[64];
            struct tm* tm_info = localtime(&seconds);
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
            return std::string(buffer);
        }
        
        static void ClearCache(const std::string& table_name, const std::string& db_name) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            std::string cache_key = db_name + "." + table_name;
            snapshot_cache_.erase(cache_key);
        }
        
        static void ClearAllCache() {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            snapshot_cache_.clear();
        }
        
        static size_t GetCacheSize() {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            return snapshot_cache_.size();
        }

    private:
        static inline std::map<std::string, SnapshotCacheEntry> snapshot_cache_;
        static inline std::mutex cache_mutex_;
        
        /**
         * Clone a TableHeap for snapshot usage
         */
        static std::unique_ptr<TableHeap> CloneTableHeap(TableHeap* source, IBufferManager* bpm) {
            if (!source) return nullptr;
            
            auto clone = std::make_unique<TableHeap>(bpm, nullptr);
            
            auto iter = source->Begin(nullptr);
            while (iter != source->End()) {
                Tuple tuple = *iter;
                RID rid;
                clone->InsertTuple(tuple, &rid, nullptr);
                ++iter;
            }
            
            return clone;
        }

        static uint64_t ParseTimestamp(const std::string& timestamp_str) {
            if (timestamp_str.find("ago") != std::string::npos) {
                return ParseRelativeTime(timestamp_str);
            }
            
            bool all_digits = true;
            for (char c : timestamp_str) {
                if (!std::isdigit(c)) {
                    all_digits = false;
                    break;
                }
            }
            if (all_digits && !timestamp_str.empty()) {
                return std::stoull(timestamp_str) * 1000000ULL;
            }
            
            return ParseISODateTime(timestamp_str);
        }

        static uint64_t ParseRelativeTime(const std::string& str) {
            uint64_t current = LogRecord::GetCurrentTimestamp();
            
            std::string num_str;
            for (char c : str) {
                if (std::isdigit(c)) num_str += c;
            }
            if (num_str.empty()) return current;
            
            uint64_t amount = std::stoull(num_str);
            uint64_t offset = 0;
            
            if (str.find("second") != std::string::npos) {
                offset = amount * 1000000ULL;
            } else if (str.find("minute") != std::string::npos) {
                offset = amount * 60 * 1000000ULL;
            } else if (str.find("hour") != std::string::npos) {
                offset = amount * 3600 * 1000000ULL;
            } else if (str.find("day") != std::string::npos) {
                offset = amount * 86400 * 1000000ULL;
            }
            
            return current - offset;
        }

        static uint64_t ParseISODateTime(const std::string& str) {
            struct tm tm_info = {};
            int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
            
            if (sscanf(str.c_str(), "%d-%d-%d %d:%d:%d", 
                       &year, &month, &day, &hour, &minute, &second) >= 3) {
                tm_info.tm_year = year - 1900;
                tm_info.tm_mon = month - 1;
                tm_info.tm_mday = day;
                tm_info.tm_hour = hour;
                tm_info.tm_min = minute;
                tm_info.tm_sec = second;
                
                time_t epoch = mktime(&tm_info);
                return static_cast<uint64_t>(epoch) * 1000000ULL;
            }
            
            return LogRecord::GetCurrentTimestamp();
        }
    };

} // namespace francodb


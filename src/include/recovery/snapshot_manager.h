#pragma once

#include "recovery/recovery_manager.h"
#include "recovery/log_manager.h"
#include "recovery/checkpoint_manager.h"
#include "recovery/time_travel_engine.h"
#include "storage/table/table_heap.h"
#include "storage/table/in_memory_table_heap.h"
#include "catalog/catalog.h"
#include "catalog/table_metadata.h"
#include "storage/storage_interface.h"
#include "common/logger.h"
#include <memory>
#include <chrono>
#include <map>
#include <mutex>
#include <fstream>
#include <sstream>

namespace chronosdb {

/**
 * Snapshot Manager - Time Travel Interface
 *
 * Delegates to TimeTravelEngine for efficient snapshot building.
 * Uses in-memory snapshots to bypass buffer pool for large results.
 */
class SnapshotManager {
public:
    /**
     * Build an IN-MEMORY snapshot (FAST - bypasses buffer pool)
     *
     * This is the RECOMMENDED method for time travel queries.
     * Returns an InMemoryTableHeap that stores tuples in RAM,
     * avoiding buffer pool thrashing for large snapshots.
     *
     * Performance: O(n) time, O(n) memory where n = result rows
     */
    static std::unique_ptr<InMemoryTableHeap> BuildSnapshotInMemory(
        const std::string& table_name,
        uint64_t target_time,
        LogManager* log_manager,
        Catalog* catalog,
        const std::string& db_name = "")
    {
        std::string target_db = db_name;
        if (target_db.empty() && log_manager) {
            target_db = log_manager->GetCurrentDatabase();
        }

        auto* table_info = catalog->GetTable(table_name);
        if (!table_info) {
            LOG_ERROR("SnapshotManager", "Table not found: %s", table_name.c_str());
            return nullptr;
        }

        uint64_t current_time = LogRecord::GetCurrentTimestamp();

        // Special case: current or future state - clone live table
        if (target_time >= current_time) {
            LOG_DEBUG("SnapshotManager", "Using live table for '%s' (in-memory copy)", table_name.c_str());
            auto snapshot = std::make_unique<InMemoryTableHeap>();
            auto live_heap = table_info->table_heap_.get();
            if (live_heap) {
                auto iter = live_heap->Begin(nullptr);
                while (iter != live_heap->End()) {
                    Tuple tuple = *iter;
                    RID rid;
                    snapshot->InsertTuple(tuple, &rid, nullptr);
                    ++iter;
                }
            }
            return snapshot;
        }

        // Use TimeTravelEngine for historical queries (returns InMemoryTableHeap)
        TimeTravelEngine engine(log_manager, catalog, nullptr, nullptr);
        return engine.BuildSnapshotInMemory(table_name, target_time, target_db);
    }

    /**
     * Build a snapshot using the optimal strategy (LEGACY - uses buffer pool)
     *
     * WARNING: This method uses the buffer pool and can be very slow for
     * large snapshots due to eviction overhead. Use BuildSnapshotInMemory instead.
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

        auto* table_info = catalog->GetTable(table_name);
        if (!table_info) {
            LOG_ERROR("SnapshotManager", "Table not found: %s", table_name.c_str());
            return nullptr;
        }

        uint64_t current_time = LogRecord::GetCurrentTimestamp();

        // Special case: current or future state - clone live table
        if (target_time >= current_time) {
            LOG_DEBUG("SnapshotManager", "Using live table for '%s'", table_name.c_str());
            auto snapshot = std::make_unique<TableHeap>(bpm, nullptr);
            auto live_heap = table_info->table_heap_.get();
            if (live_heap) {
                auto iter = live_heap->Begin(nullptr);
                while (iter != live_heap->End()) {
                    Tuple tuple = *iter;
                    RID rid;
                    snapshot->InsertTuple(tuple, &rid, nullptr);
                    ++iter;
                }
            }
            return snapshot;
        }

        // Use TimeTravelEngine for historical queries
        TimeTravelEngine engine(log_manager, catalog, bpm, nullptr);
        return engine.BuildSnapshot(table_name, target_time, target_db,
                                    TimeTravelEngine::Strategy::AUTO);
    }

    /**
     * Checkpoint info for navigation
     */
    struct CheckpointInfo {
        LogRecord::lsn_t lsn;
        uint64_t timestamp;
        std::streampos offset;
    };

    /**
     * Find all checkpoints in the log
     */
    static std::vector<CheckpointInfo> FindAllCheckpoints(LogManager* log_manager, const std::string& db_name) {
        std::vector<CheckpointInfo> checkpoints;

        if (!log_manager) return checkpoints;

        std::string log_path = log_manager->GetLogFilePath(db_name);
        std::ifstream log_file(log_path, std::ios::binary | std::ios::in);
        if (!log_file.is_open()) {
            LOG_DEBUG("SnapshotManager", "Cannot open log for checkpoint scan");
            return checkpoints;
        }

        LogRecord record(0, 0, LogRecordType::INVALID);
        while (ReadLogRecordSimple(log_file, record)) {
            if (record.log_record_type_ == LogRecordType::CHECKPOINT_END) {
                CheckpointInfo cp;
                cp.lsn = record.lsn_;
                cp.timestamp = record.timestamp_;
                cp.offset = log_file.tellg();
                checkpoints.push_back(cp);
            }
        }

        log_file.close();
        return checkpoints;
    }

    /**
     * Build snapshot from human-readable timestamp.
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
     * Build snapshot at relative time offset.
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

private:
    static constexpr size_t MAX_RECORD_SIZE = 10000000;

    /**
     * Simple log record reader
     */
    static bool ReadLogRecordSimple(std::ifstream& log_file, LogRecord& record) {
        std::streampos start_pos = log_file.tellg();

        int32_t size = 0;
        log_file.read(reinterpret_cast<char*>(&size), sizeof(int32_t));
        if (log_file.gcount() != sizeof(int32_t) || size <= 0 || static_cast<size_t>(size) > MAX_RECORD_SIZE) {
            return false;
        }

        log_file.read(reinterpret_cast<char*>(&record.lsn_), sizeof(LogRecord::lsn_t));
        log_file.read(reinterpret_cast<char*>(&record.prev_lsn_), sizeof(LogRecord::lsn_t));
        log_file.read(reinterpret_cast<char*>(&record.undo_next_lsn_), sizeof(LogRecord::lsn_t));
        log_file.read(reinterpret_cast<char*>(&record.txn_id_), sizeof(LogRecord::txn_id_t));
        log_file.read(reinterpret_cast<char*>(&record.timestamp_), sizeof(LogRecord::timestamp_t));

        int log_type_int;
        log_file.read(reinterpret_cast<char*>(&log_type_int), sizeof(int));
        record.log_record_type_ = static_cast<LogRecordType>(log_type_int);

        // Read db_name
        uint32_t len = 0;
        log_file.read(reinterpret_cast<char*>(&len), sizeof(uint32_t));
        if (len > 0 && len < MAX_RECORD_SIZE) {
            std::vector<char> buf(len);
            log_file.read(buf.data(), len);
            record.db_name_ = std::string(buf.begin(), buf.end());
        }

        // Read body based on type
        switch (record.log_record_type_) {
            case LogRecordType::INSERT:
                record.table_name_ = ReadString(log_file);
                record.new_value_ = ReadValue(log_file);
                break;
            case LogRecordType::UPDATE:
                record.table_name_ = ReadString(log_file);
                record.old_value_ = ReadValue(log_file);
                record.new_value_ = ReadValue(log_file);
                break;
            case LogRecordType::APPLY_DELETE:
            case LogRecordType::MARK_DELETE:
            case LogRecordType::ROLLBACK_DELETE:
                record.table_name_ = ReadString(log_file);
                record.old_value_ = ReadValue(log_file);
                break;
            case LogRecordType::CREATE_TABLE:
            case LogRecordType::DROP_TABLE:
            case LogRecordType::CLR:
                record.table_name_ = ReadString(log_file);
                break;
            case LogRecordType::CHECKPOINT_BEGIN:
            case LogRecordType::CHECKPOINT_END: {
                int32_t att_size = 0;
                log_file.read(reinterpret_cast<char*>(&att_size), sizeof(int32_t));
                for (int32_t i = 0; i < att_size && i < 10000; i++) {
                    int32_t dummy;
                    log_file.read(reinterpret_cast<char*>(&dummy), sizeof(int32_t));
                    log_file.read(reinterpret_cast<char*>(&dummy), sizeof(int32_t));
                    log_file.read(reinterpret_cast<char*>(&dummy), sizeof(int32_t));
                }
                int32_t dpt_size = 0;
                log_file.read(reinterpret_cast<char*>(&dpt_size), sizeof(int32_t));
                for (int32_t i = 0; i < dpt_size && i < 10000; i++) {
                    int32_t dummy;
                    log_file.read(reinterpret_cast<char*>(&dummy), sizeof(int32_t));
                    log_file.read(reinterpret_cast<char*>(&dummy), sizeof(int32_t));
                }
                break;
            }
            default:
                log_file.seekg(start_pos);
                log_file.seekg(size + sizeof(uint32_t), std::ios::cur);
                return true;
        }

        uint32_t crc;
        log_file.read(reinterpret_cast<char*>(&crc), sizeof(uint32_t));

        return true;
    }

    static std::string ReadString(std::ifstream& in) {
        uint32_t len = 0;
        in.read(reinterpret_cast<char*>(&len), sizeof(uint32_t));
        if (in.gcount() != sizeof(uint32_t) || len > MAX_RECORD_SIZE) return "";
        std::vector<char> buf(len);
        in.read(buf.data(), len);
        return std::string(buf.begin(), buf.end());
    }

    static Value ReadValue(std::ifstream& in) {
        int type_id = 0;
        in.read(reinterpret_cast<char*>(&type_id), sizeof(int));
        std::string s_val = ReadString(in);
        TypeId type = static_cast<TypeId>(type_id);
        if (type == TypeId::INTEGER) {
            try { return Value(type, std::stoi(s_val)); }
            catch (...) { return Value(type, 0); }
        }
        if (type == TypeId::DECIMAL) {
            try { return Value(type, std::stod(s_val)); }
            catch (...) { return Value(type, 0.0); }
        }
        return Value(type, s_val);
    }

    static uint64_t ParseTimestamp(const std::string& timestamp_str) {
        if (timestamp_str.find("ago") != std::string::npos) {
            return ParseRelativeTime(timestamp_str);
        }

        bool all_digits = true;
        for (char c : timestamp_str) {
            if (!std::isdigit(c)) { all_digits = false; break; }
        }
        if (all_digits && !timestamp_str.empty()) {
            return std::stoull(timestamp_str) * 1000000ULL;
        }

        return ParseISODateTime(timestamp_str);
    }

    static uint64_t ParseRelativeTime(const std::string& str) {
        uint64_t current = LogRecord::GetCurrentTimestamp();
        std::string num_str;
        for (char c : str) { if (std::isdigit(c)) num_str += c; }
        if (num_str.empty()) return current;

        uint64_t amount = std::stoull(num_str);
        uint64_t offset = 0;

        if (str.find("second") != std::string::npos) offset = amount * 1000000ULL;
        else if (str.find("minute") != std::string::npos) offset = amount * 60 * 1000000ULL;
        else if (str.find("hour") != std::string::npos) offset = amount * 3600 * 1000000ULL;
        else if (str.find("day") != std::string::npos) offset = amount * 86400 * 1000000ULL;

        return current - offset;
    }

    static uint64_t ParseISODateTime(const std::string& str) {
        struct tm tm_info = {};
        int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
        int parsed = 0;

        // Try ISO format: YYYY-MM-DD HH:MM:SS
        parsed = sscanf(str.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);
        if (parsed >= 3 && year > 1900) {
            tm_info.tm_year = year - 1900;
            tm_info.tm_mon = month - 1;
            tm_info.tm_mday = day;
            tm_info.tm_hour = hour;
            tm_info.tm_min = minute;
            tm_info.tm_sec = second;
            time_t epoch = mktime(&tm_info);
            return static_cast<uint64_t>(epoch) * 1000000ULL;
        }

        // Try European format: DD/MM/YYYY HH:MM:SS
        parsed = sscanf(str.c_str(), "%d/%d/%d %d:%d:%d", &day, &month, &year, &hour, &minute, &second);
        if (parsed >= 3) {
            tm_info.tm_year = year - 1900;
            tm_info.tm_mon = month - 1;
            tm_info.tm_mday = day;
            tm_info.tm_hour = hour;
            tm_info.tm_min = minute;
            tm_info.tm_sec = second;
            time_t epoch = mktime(&tm_info);
            return static_cast<uint64_t>(epoch) * 1000000ULL;
        }

        LOG_WARN("SnapshotManager", "Failed to parse timestamp: %s", str.c_str());
        return LogRecord::GetCurrentTimestamp();
    }
};

} // namespace chronosdb

#include "recovery/checkpoint_index.h"
#include <cstring>

namespace chronosdb {

    /**
     * Simple log record reader for checkpoint scanning.
     * Only extracts the fields we need (LSN, timestamp, record type).
     */
    static bool ReadLogRecordForIndex(std::ifstream& log_file,
                                       LogRecord::lsn_t& lsn,
                                       LogRecord::timestamp_t& timestamp,
                                       LogRecordType& type,
                                       std::streampos& record_end_offset) {
        // Remember start position
        std::streampos start_pos = log_file.tellg();

        // Read size
        int32_t size = 0;
        log_file.read(reinterpret_cast<char*>(&size), sizeof(int32_t));
        if (log_file.gcount() != sizeof(int32_t) || size <= 0 || size > 10000000) {
            return false;
        }

        // Read LSN
        log_file.read(reinterpret_cast<char*>(&lsn), sizeof(LogRecord::lsn_t));
        if (log_file.gcount() != sizeof(LogRecord::lsn_t)) {
            return false;
        }

        // Skip prev_lsn
        LogRecord::lsn_t prev_lsn;
        log_file.read(reinterpret_cast<char*>(&prev_lsn), sizeof(LogRecord::lsn_t));

        // Skip undo_next_lsn
        LogRecord::lsn_t undo_next_lsn;
        log_file.read(reinterpret_cast<char*>(&undo_next_lsn), sizeof(LogRecord::lsn_t));

        // Skip txn_id
        LogRecord::txn_id_t txn_id;
        log_file.read(reinterpret_cast<char*>(&txn_id), sizeof(LogRecord::txn_id_t));

        // Read timestamp
        log_file.read(reinterpret_cast<char*>(&timestamp), sizeof(LogRecord::timestamp_t));
        if (log_file.gcount() != sizeof(LogRecord::timestamp_t)) {
            return false;
        }

        // Read record type
        int log_type_int;
        log_file.read(reinterpret_cast<char*>(&log_type_int), sizeof(int));
        if (log_file.gcount() != sizeof(int)) {
            return false;
        }
        type = static_cast<LogRecordType>(log_type_int);

        // Seek to end of record using size field
        // Record layout: [size(4)] [data(size-4)] [crc(4)]
        // Total bytes = 4 + size
        log_file.seekg(start_pos);
        log_file.seekg(sizeof(int32_t) + size, std::ios::cur);

        // Record the end position (where we should continue from)
        record_end_offset = log_file.tellg();

        return true;
    }

    size_t CheckpointIndex::BuildFromLog(const std::string& log_path) {
        entries_.clear();

        if (!std::filesystem::exists(log_path)) {
            std::cout << "[CheckpointIndex] Log file not found: " << log_path << std::endl;
            return 0;
        }

        std::ifstream log_file(log_path, std::ios::binary | std::ios::in);
        if (!log_file.is_open()) {
            std::cerr << "[CheckpointIndex] Cannot open log file: " << log_path << std::endl;
            return 0;
        }

        std::cout << "[CheckpointIndex] Scanning log for checkpoints: " << log_path << std::endl;

        LogRecord::lsn_t lsn;
        LogRecord::timestamp_t timestamp;
        LogRecordType type;
        std::streampos record_end_offset;

        size_t records_scanned = 0;
        size_t checkpoints_found = 0;

        while (ReadLogRecordForIndex(log_file, lsn, timestamp, type, record_end_offset)) {
            records_scanned++;

            if (type == LogRecordType::CHECKPOINT_END) {
                // The offset is the position AFTER this record
                // This is where replay should start from
                entries_.emplace_back(lsn, timestamp, record_end_offset);
                checkpoints_found++;

                std::cout << "[CheckpointIndex]   Found checkpoint #" << checkpoints_found
                          << ": LSN=" << lsn
                          << ", timestamp=" << timestamp
                          << ", offset=" << record_end_offset << std::endl;
            }
        }

        log_file.close();

        // Ensure sorted by timestamp
        std::sort(entries_.begin(), entries_.end());

        std::cout << "[CheckpointIndex] Scan complete. Scanned " << records_scanned
                  << " records, found " << checkpoints_found << " checkpoints." << std::endl;

        return checkpoints_found;
    }

} // namespace chronosdb

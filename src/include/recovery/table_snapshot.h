#pragma once

#include "recovery/log_record.h"
#include "storage/table/table_heap.h"
#include "storage/table/schema.h"
#include "common/value.h"
#include "storage/storage_interface.h"
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <filesystem>

namespace chronosdb {

    /**
     * TableSnapshot - Represents a complete table state at a specific point in time
     *
     * This is the core class for Git-style checkpoint snapshots. Each snapshot stores:
     * - Complete table schema
     * - All row data at checkpoint time
     * - Metadata (LSN, timestamp)
     *
     * Snapshots enable O(log K + S + D) time travel queries instead of O(N):
     * - K = number of checkpoints (binary search)
     * - S = snapshot size (load from disk)
     * - D = delta records (replay log)
     *
     * Design Principles:
     * - Immutable: Once created, snapshots never change
     * - Self-contained: Include all data needed to reconstruct table
     * - Versioned: Binary format supports future extensions
     * - Efficient: Binary serialization with optional compression
     */
    class TableSnapshot {
    public:
        /**
         * Magic number for snapshot files: "SNAP" in ASCII
         */
        static constexpr uint32_t MAGIC_NUMBER = 0x534E4150;

        /**
         * Current snapshot format version
         */
        static constexpr uint32_t VERSION = 1;

        /**
         * Factory method: Create snapshot from live table
         *
         * @param heap Table heap to snapshot
         * @param table_name Name of the table
         * @param schema Table schema
         * @param checkpoint_lsn LSN of the checkpoint
         * @param timestamp Timestamp when checkpoint was taken
         * @return Snapshot object or nullptr on error
         */
        static std::unique_ptr<TableSnapshot> CreateFromTable(
            TableHeap* heap,
            const std::string& table_name,
            const Schema& schema,
            LogRecord::lsn_t checkpoint_lsn,
            uint64_t timestamp);

        /**
         * Factory method: Load snapshot from disk
         *
         * @param file_path Path to snapshot file
         * @return Snapshot object or nullptr on error
         */
        static std::unique_ptr<TableSnapshot> LoadFromFile(const std::string& file_path);

        /**
         * Persist snapshot to disk
         *
         * @param file_path Path where snapshot should be saved
         * @return true on success, false on error
         */
        bool SaveToFile(const std::string& file_path) const;

        /**
         * Convert snapshot to TableHeap for query execution
         *
         * @param bpm Buffer pool manager for heap allocation
         * @return New TableHeap containing snapshot data
         */
        std::unique_ptr<TableHeap> ToTableHeap(IBufferManager* bpm) const;

        // Accessors
        LogRecord::lsn_t GetCheckpointLSN() const { return checkpoint_lsn_; }
        uint64_t GetTimestamp() const { return timestamp_; }
        const std::string& GetTableName() const { return table_name_; }
        const Schema& GetSchema() const { return schema_; }
        size_t GetRowCount() const { return rows_.size(); }

        /**
         * Get all rows (for direct access in optimization scenarios)
         */
        const std::vector<std::vector<Value>>& GetRows() const { return rows_; }

        /**
         * Get memory footprint of snapshot in bytes
         */
        size_t GetMemorySize() const;

    private:
        // Private constructor - use factory methods
        TableSnapshot()
            : checkpoint_lsn_(LogRecord::INVALID_LSN),
              timestamp_(0),
              table_name_(""),
              schema_(std::vector<Column>{}) {}  // Empty schema

        // Metadata
        LogRecord::lsn_t checkpoint_lsn_;
        uint64_t timestamp_;
        std::string table_name_;
        Schema schema_;

        // Data - in-memory representation
        std::vector<std::vector<Value>> rows_;

        // Serialization helpers
        bool WriteHeader(std::ofstream& file) const;
        bool WriteSchema(std::ofstream& file) const;
        bool WriteData(std::ofstream& file) const;

        bool ReadHeader(std::ifstream& file);
        bool ReadSchema(std::ifstream& file);
        bool ReadData(std::ifstream& file);

        // Utility functions
        static bool WriteString(std::ofstream& file, const std::string& str);
        static std::string ReadString(std::ifstream& file);
        static bool WriteValue(std::ofstream& file, const Value& value);
        static Value ReadValue(std::ifstream& file, TypeId type);
    };

} // namespace chronosdb

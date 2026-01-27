#include "recovery/table_snapshot.h"
#include "storage/table/tuple.h"
#include <iostream>
#include <cstring>

namespace chronosdb {

    // ========================================================================
    // FACTORY METHODS
    // ========================================================================

    std::unique_ptr<TableSnapshot> TableSnapshot::CreateFromTable(
        TableHeap* heap,
        const std::string& table_name,
        const Schema& schema,
        LogRecord::lsn_t checkpoint_lsn,
        uint64_t timestamp) {

        if (!heap) {
            std::cerr << "[TableSnapshot] ERROR: Null heap pointer" << std::endl;
            return nullptr;
        }

        auto snapshot = std::unique_ptr<TableSnapshot>(new TableSnapshot());
        snapshot->checkpoint_lsn_ = checkpoint_lsn;
        snapshot->timestamp_ = timestamp;
        snapshot->table_name_ = table_name;
        snapshot->schema_ = schema;

        std::cout << "[TableSnapshot] Creating snapshot of table '" << table_name
                  << "' at LSN " << checkpoint_lsn << std::endl;

        // Read all tuples from heap
        size_t row_count = 0;
        auto iter = heap->Begin(nullptr);
        while (iter != heap->End()) {
            std::vector<Value> row;
            row.reserve(schema.GetColumnCount());

            // Extract values from tuple
            for (uint32_t i = 0; i < schema.GetColumnCount(); i++) {
                row.push_back((*iter).GetValue(schema, i));
            }

            snapshot->rows_.push_back(std::move(row));
            ++iter;
            row_count++;

            // Progress reporting for large tables
            if (row_count % 10000 == 0) {
                std::cout << "[TableSnapshot]   Captured " << row_count << " rows..." << std::endl;
            }
        }

        std::cout << "[TableSnapshot] Snapshot created with " << row_count << " rows" << std::endl;
        return snapshot;
    }

    std::unique_ptr<TableSnapshot> TableSnapshot::LoadFromFile(const std::string& file_path) {
        if (!std::filesystem::exists(file_path)) {
            std::cerr << "[TableSnapshot] File not found: " << file_path << std::endl;
            return nullptr;
        }

        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[TableSnapshot] Cannot open file: " << file_path << std::endl;
            return nullptr;
        }

        auto snapshot = std::unique_ptr<TableSnapshot>(new TableSnapshot());

        // Read header
        if (!snapshot->ReadHeader(file)) {
            std::cerr << "[TableSnapshot] Failed to read header from " << file_path << std::endl;
            return nullptr;
        }

        // Read schema
        if (!snapshot->ReadSchema(file)) {
            std::cerr << "[TableSnapshot] Failed to read schema from " << file_path << std::endl;
            return nullptr;
        }

        // Read data
        if (!snapshot->ReadData(file)) {
            std::cerr << "[TableSnapshot] Failed to read data from " << file_path << std::endl;
            return nullptr;
        }

        file.close();

        std::cout << "[TableSnapshot] Loaded snapshot from " << file_path
                  << " (" << snapshot->rows_.size() << " rows)" << std::endl;

        return snapshot;
    }

    // ========================================================================
    // PERSISTENCE
    // ========================================================================

    bool TableSnapshot::SaveToFile(const std::string& file_path) const {
        // Create directory if needed
        std::filesystem::path path(file_path);
        std::filesystem::create_directories(path.parent_path());

        // Write to temporary file first (atomic write)
        std::string temp_path = file_path + ".tmp";
        std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);

        if (!file.is_open()) {
            std::cerr << "[TableSnapshot] Cannot create file: " << temp_path << std::endl;
            return false;
        }

        // Write snapshot
        if (!WriteHeader(file)) {
            std::cerr << "[TableSnapshot] Failed to write header" << std::endl;
            file.close();
            std::filesystem::remove(temp_path);
            return false;
        }

        if (!WriteSchema(file)) {
            std::cerr << "[TableSnapshot] Failed to write schema" << std::endl;
            file.close();
            std::filesystem::remove(temp_path);
            return false;
        }

        if (!WriteData(file)) {
            std::cerr << "[TableSnapshot] Failed to write data" << std::endl;
            file.close();
            std::filesystem::remove(temp_path);
            return false;
        }

        file.flush();
        file.close();

        // Atomic rename
        try {
            std::filesystem::rename(temp_path, file_path);
        } catch (const std::exception& e) {
            std::cerr << "[TableSnapshot] Failed to rename temp file: " << e.what() << std::endl;
            return false;
        }

        std::cout << "[TableSnapshot] Saved snapshot to " << file_path
                  << " (" << rows_.size() << " rows)" << std::endl;

        return true;
    }

    // ========================================================================
    // CONVERSION TO TABLE HEAP
    // ========================================================================

    std::unique_ptr<TableHeap> TableSnapshot::ToTableHeap(IBufferManager* bpm) const {
        if (!bpm) {
            std::cerr << "[TableSnapshot] ERROR: Null buffer pool manager" << std::endl;
            return nullptr;
        }

        auto heap = std::make_unique<TableHeap>(bpm, nullptr);

        std::cout << "[TableSnapshot] Converting snapshot to TableHeap (" << rows_.size() << " rows)" << std::endl;

        size_t inserted = 0;
        for (const auto& row : rows_) {
            // Create tuple from row values
            Tuple tuple(row, schema_);

            // Insert into heap
            RID rid;
            if (heap->InsertTuple(tuple, &rid, nullptr)) {
                inserted++;
            } else {
                std::cerr << "[TableSnapshot] WARNING: Failed to insert tuple" << std::endl;
            }

            // Progress reporting
            if (inserted % 10000 == 0 && inserted > 0) {
                std::cout << "[TableSnapshot]   Inserted " << inserted << " rows..." << std::endl;
            }
        }

        std::cout << "[TableSnapshot] Conversion complete. Inserted " << inserted << " rows" << std::endl;
        return heap;
    }

    // ========================================================================
    // MEMORY SIZE
    // ========================================================================

    size_t TableSnapshot::GetMemorySize() const {
        size_t size = sizeof(TableSnapshot);

        // Add table name
        size += table_name_.capacity();

        // Add schema (approximate)
        size += schema_.GetColumnCount() * 100;  // Rough estimate per column

        // Add row data
        for (const auto& row : rows_) {
            for (const auto& value : row) {
                size += value.ToString().size() + 16;  // Value overhead
            }
        }

        return size;
    }

    // ========================================================================
    // SERIALIZATION - HEADER
    // ========================================================================

    bool TableSnapshot::WriteHeader(std::ofstream& file) const {
        // Magic number
        file.write(reinterpret_cast<const char*>(&MAGIC_NUMBER), sizeof(MAGIC_NUMBER));

        // Version
        file.write(reinterpret_cast<const char*>(&VERSION), sizeof(VERSION));

        // Checkpoint LSN
        file.write(reinterpret_cast<const char*>(&checkpoint_lsn_), sizeof(checkpoint_lsn_));

        // Timestamp
        file.write(reinterpret_cast<const char*>(&timestamp_), sizeof(timestamp_));

        // Table name
        if (!WriteString(file, table_name_)) {
            return false;
        }

        // Row count
        uint32_t row_count = static_cast<uint32_t>(rows_.size());
        file.write(reinterpret_cast<const char*>(&row_count), sizeof(row_count));

        return file.good();
    }

    bool TableSnapshot::ReadHeader(std::ifstream& file) {
        // Magic number
        uint32_t magic;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (magic != MAGIC_NUMBER) {
            std::cerr << "[TableSnapshot] Invalid magic number: " << std::hex << magic << std::endl;
            return false;
        }

        // Version
        uint32_t version;
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version > VERSION) {
            std::cerr << "[TableSnapshot] Unsupported version: " << version << std::endl;
            return false;
        }

        // Checkpoint LSN
        file.read(reinterpret_cast<char*>(&checkpoint_lsn_), sizeof(checkpoint_lsn_));

        // Timestamp
        file.read(reinterpret_cast<char*>(&timestamp_), sizeof(timestamp_));

        // Table name
        table_name_ = ReadString(file);
        if (table_name_.empty()) {
            return false;
        }

        // Row count (will be used when reading data)
        uint32_t row_count;
        file.read(reinterpret_cast<char*>(&row_count), sizeof(row_count));
        rows_.reserve(row_count);

        return file.good();
    }

    // ========================================================================
    // SERIALIZATION - SCHEMA
    // ========================================================================

    bool TableSnapshot::WriteSchema(std::ofstream& file) const {
        // Column count
        uint32_t col_count = schema_.GetColumnCount();
        file.write(reinterpret_cast<const char*>(&col_count), sizeof(col_count));

        // For each column
        for (uint32_t i = 0; i < col_count; i++) {
            const Column& col = schema_.GetColumn(i);

            // Column name
            if (!WriteString(file, col.GetName())) {
                return false;
            }

            // Type ID
            int32_t type_id = static_cast<int32_t>(col.GetType());
            file.write(reinterpret_cast<const char*>(&type_id), sizeof(type_id));

            // Max length (for VARCHAR)
            uint32_t max_len = col.GetLength();
            file.write(reinterpret_cast<const char*>(&max_len), sizeof(max_len));

            // Flags (nullable, etc.)
            uint32_t flags = 0;
            if (col.IsNullable()) flags |= 0x01;
            file.write(reinterpret_cast<const char*>(&flags), sizeof(flags));
        }

        return file.good();
    }

    bool TableSnapshot::ReadSchema(std::ifstream& file) {
        // Column count
        uint32_t col_count;
        file.read(reinterpret_cast<char*>(&col_count), sizeof(col_count));
        if (col_count == 0 || col_count > 1000) {
            std::cerr << "[TableSnapshot] Invalid column count: " << col_count << std::endl;
            return false;
        }

        // Read each column
        std::vector<Column> columns;
        columns.reserve(col_count);

        for (uint32_t i = 0; i < col_count; i++) {
            // Column name
            std::string col_name = ReadString(file);
            if (col_name.empty()) {
                return false;
            }

            // Type ID
            int32_t type_id;
            file.read(reinterpret_cast<char*>(&type_id), sizeof(type_id));
            TypeId type = static_cast<TypeId>(type_id);

            // Max length
            uint32_t max_len;
            file.read(reinterpret_cast<char*>(&max_len), sizeof(max_len));

            // Flags
            uint32_t flags;
            file.read(reinterpret_cast<char*>(&flags), sizeof(flags));
            bool nullable = (flags & 0x01) != 0;

            // Create column
            Column col(col_name, type, max_len);
            if (nullable) {
                // Note: Column class may not have SetNullable, adjust as needed
            }

            columns.push_back(col);
        }

        // Create schema
        schema_ = Schema(columns);

        return file.good();
    }

    // ========================================================================
    // SERIALIZATION - DATA
    // ========================================================================

    bool TableSnapshot::WriteData(std::ofstream& file) const {
        size_t rows_written = 0;

        for (const auto& row : rows_) {
            // Write each value in the row
            for (uint32_t i = 0; i < row.size(); i++) {
                if (!WriteValue(file, row[i])) {
                    return false;
                }
            }

            rows_written++;

            // Progress reporting
            if (rows_written % 10000 == 0) {
                std::cout << "[TableSnapshot]   Wrote " << rows_written << " rows..." << std::endl;
            }
        }

        return file.good();
    }

    bool TableSnapshot::ReadData(std::ifstream& file) {
        const uint32_t col_count = schema_.GetColumnCount();
        size_t rows_read = 0;

        while (file.good() && rows_read < rows_.capacity()) {
            std::vector<Value> row;
            row.reserve(col_count);

            // Read each column value
            for (uint32_t i = 0; i < col_count; i++) {
                const Column& col = schema_.GetColumn(i);
                Value val = ReadValue(file, col.GetType());

                if (!file.good()) {
                    break;
                }

                row.push_back(val);
            }

            if (row.size() == col_count) {
                rows_.push_back(std::move(row));
                rows_read++;

                // Progress reporting
                if (rows_read % 10000 == 0) {
                    std::cout << "[TableSnapshot]   Read " << rows_read << " rows..." << std::endl;
                }
            } else {
                break;  // Incomplete row
            }
        }

        return rows_read > 0;
    }

    // ========================================================================
    // UTILITY FUNCTIONS
    // ========================================================================

    bool TableSnapshot::WriteString(std::ofstream& file, const std::string& str) {
        uint32_t len = static_cast<uint32_t>(str.size());
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        if (len > 0) {
            file.write(str.data(), len);
        }
        return file.good();
    }

    std::string TableSnapshot::ReadString(std::ifstream& file) {
        uint32_t len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));

        if (len == 0 || len > 1000000) {  // Sanity check
            return "";
        }

        std::vector<char> buf(len);
        file.read(buf.data(), len);

        if (!file.good()) {
            return "";
        }

        return std::string(buf.begin(), buf.end());
    }

    bool TableSnapshot::WriteValue(std::ofstream& file, const Value& value) {
        // Write value as string (simple approach)
        std::string val_str = value.ToString();
        return WriteString(file, val_str);
    }

    Value TableSnapshot::ReadValue(std::ifstream& file, TypeId type) {
        std::string val_str = ReadString(file);

        if (val_str.empty() && type != TypeId::VARCHAR) {
            // Return default value for empty string
            if (type == TypeId::INTEGER) {
                return Value(type, 0);
            } else if (type == TypeId::DECIMAL) {
                return Value(type, 0.0);
            } else if (type == TypeId::BOOLEAN) {
                return Value(type, false);
            }
        }

        // Parse based on type
        try {
            if (type == TypeId::INTEGER) {
                return Value(type, std::stoi(val_str));
            } else if (type == TypeId::DECIMAL) {
                return Value(type, std::stod(val_str));
            } else if (type == TypeId::BOOLEAN) {
                bool b = (val_str == "true" || val_str == "1");
                return Value(type, b);
            } else {
                return Value(type, val_str);
            }
        } catch (...) {
            // Return default on parse error
            if (type == TypeId::INTEGER) {
                return Value(type, 0);
            } else if (type == TypeId::DECIMAL) {
                return Value(type, 0.0);
            } else if (type == TypeId::BOOLEAN) {
                return Value(type, false);
            } else {
                return Value(type, std::string(""));
            }
        }
    }

} // namespace chronosdb

#include "recovery/recovery_manager.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <stack> // Required for Rollback
#include "storage/table/table_heap.h"
#include "catalog/catalog.h"
#include "common/type.h" 
#include "common/value.h"

namespace francodb {

    // --- STATIC HELPERS ---
    static std::string ReadString(std::ifstream& in) {
        uint32_t len = 0;
        in.read(reinterpret_cast<char*>(&len), sizeof(uint32_t));
        if (in.gcount() != sizeof(uint32_t)) return "";
        std::vector<char> buf(len);
        in.read(buf.data(), len);
        return std::string(buf.begin(), buf.end());
    }

    static Value ReadValue(std::ifstream& in) {
        int type_id = 0;
        in.read(reinterpret_cast<char*>(&type_id), sizeof(int));
        std::string s_val = ReadString(in);
        
        TypeId type = static_cast<TypeId>(type_id);
        if (type == TypeId::INTEGER) { try { return Value(type, std::stoi(s_val)); } catch (...) { return Value(type, 0); } }
        if (type == TypeId::DECIMAL) { try { return Value(type, std::stod(s_val)); } catch (...) { return Value(type, 0.0); } }
        return Value(type, s_val);
    }

    // --- RECOVERY LOGIC ---

    void RecoveryManager::RunRecoveryLoop(uint64_t stop_at_time, uint64_t start_offset) {
        std::string filename = log_manager_->GetLogFileName();
        std::ifstream log_file(filename, std::ios::binary | std::ios::in);
        if (!log_file.is_open()) return;

        if (start_offset > 0) log_file.seekg(start_offset);

        std::cout << "=== RECOVERY STARTED ===" << std::endl;

        while (log_file.peek() != EOF) {
            int32_t size = 0;
            log_file.read(reinterpret_cast<char*>(&size), sizeof(int32_t));
            if (log_file.gcount() != sizeof(int32_t)) break;
            if (size <= 0 || size > 10000000) break;

            LogRecord::lsn_t lsn, prev_lsn;
            LogRecord::txn_id_t txn_id;
            LogRecord::timestamp_t timestamp; 
            int log_type_int;

            log_file.read(reinterpret_cast<char*>(&lsn), sizeof(lsn));
            log_file.read(reinterpret_cast<char*>(&prev_lsn), sizeof(prev_lsn));
            log_file.read(reinterpret_cast<char*>(&txn_id), sizeof(txn_id));
            log_file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
            log_file.read(reinterpret_cast<char*>(&log_type_int), sizeof(int));
            
            LogRecordType type = static_cast<LogRecordType>(log_type_int);

            if (stop_at_time > 0 && timestamp > stop_at_time) break;

            std::string table_name;
            Value v1, v2;
            
            auto table_info = catalog_->GetTable(table_name);

            switch (type) {
                case LogRecordType::INSERT: {
                    table_name = ReadString(log_file);
                    v1 = ReadValue(log_file);
                    table_info = catalog_->GetTable(table_name);
                    if (table_info) {
                        std::vector<Value> values = {v1}; 
                        Tuple tuple(values, table_info->schema_);
                        RID rid;
                        table_info->table_heap_->InsertTuple(tuple, &rid, nullptr);
                    }
                    break;
                }
                case LogRecordType::UPDATE: {
                    table_name = ReadString(log_file);
                    v1 = ReadValue(log_file); // Old
                    v2 = ReadValue(log_file); // New
                    table_info = catalog_->GetTable(table_name);
                    if (table_info) {
                        auto it = table_info->table_heap_->Begin(nullptr);
                        while (it != table_info->table_heap_->End()) {
                            if ((*it).GetValue(table_info->schema_, 0).ToString() == v1.ToString()) { 
                                std::vector<Value> new_vals = {v2};
                                Tuple new_tuple(new_vals, table_info->schema_);
                                table_info->table_heap_->UpdateTuple(new_tuple, it.GetRID(), nullptr);
                            }
                            ++it;
                        }
                    }
                    break;
                }
                case LogRecordType::APPLY_DELETE: {
                    table_name = ReadString(log_file);
                    v1 = ReadValue(log_file);
                    table_info = catalog_->GetTable(table_name);
                    if (table_info) {
                        auto it = table_info->table_heap_->Begin(nullptr);
                        while (it != table_info->table_heap_->End()) {
                            if ((*it).GetValue(table_info->schema_, 0).ToString() == v1.ToString()) {
                                table_info->table_heap_->MarkDelete(it.GetRID(), nullptr);
                            }
                            ++it;
                        }
                    }
                    break;
                }
                default: break;
            }
        }
        std::cout << "=== RECOVERY COMPLETE ===" << std::endl;
    }

    void RecoveryManager::RollbackToTime(uint64_t target_time) {
        std::string filename = log_manager_->GetLogFileName();
        std::ifstream log_file(filename, std::ios::binary | std::ios::in);
        if (!log_file.is_open()) return;

        std::cout << "=== ROLLBACK STARTED (Target: " << target_time << ") ===" << std::endl;

        std::vector<std::streampos> future_log_offsets;
        
        while (log_file.peek() != EOF) {
            std::streampos current_pos = log_file.tellg();
            int32_t size = 0;
            log_file.read(reinterpret_cast<char*>(&size), sizeof(int32_t));
            if (log_file.gcount() != sizeof(int32_t)) break;
            if (size <= 0 || size > 10000000) break;

            log_file.seekg(12, std::ios::cur); // Skip LSN, Prev, Txn
            
            LogRecord::timestamp_t timestamp;
            log_file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
            
            if (timestamp > target_time) {
                future_log_offsets.push_back(current_pos);
            }
            
            log_file.seekg(current_pos);
            log_file.seekg(size, std::ios::cur);
        }

        std::cout << "[ANALYSIS] Found " << future_log_offsets.size() << " operations to UNDO." << std::endl;

        for (auto it = future_log_offsets.rbegin(); it != future_log_offsets.rend(); ++it) {
            log_file.seekg(*it);
            
            int32_t size;
            log_file.read(reinterpret_cast<char*>(&size), sizeof(int32_t));
            
            LogRecord::lsn_t lsn, prev_lsn;
            LogRecord::txn_id_t txn_id;
            LogRecord::timestamp_t timestamp; 
            int log_type_int;

            log_file.read(reinterpret_cast<char*>(&lsn), sizeof(lsn));
            log_file.read(reinterpret_cast<char*>(&prev_lsn), sizeof(prev_lsn));
            log_file.read(reinterpret_cast<char*>(&txn_id), sizeof(txn_id));
            log_file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
            log_file.read(reinterpret_cast<char*>(&log_type_int), sizeof(int));
            
            LogRecordType type = static_cast<LogRecordType>(log_type_int);
            std::string table_name;
            Value v1, v2;

            switch (type) {
                case LogRecordType::INSERT: { // UNDO -> DELETE
                    table_name = ReadString(log_file);
                    v1 = ReadValue(log_file); 
                    auto table_info = catalog_->GetTable(table_name);
                    if (table_info) {
                        auto iter = table_info->table_heap_->Begin(nullptr);
                        while (iter != table_info->table_heap_->End()) {
                             if ((*iter).GetValue(table_info->schema_, 0).ToString() == v1.ToString()) {
                                 table_info->table_heap_->MarkDelete(iter.GetRID(), nullptr);
                                 std::cout << "[UNDO] Reverted INSERT: " << v1.ToString() << std::endl;
                                 break;
                             }
                             ++iter;
                        }
                    }
                    break;
                }
                case LogRecordType::APPLY_DELETE: { // UNDO -> INSERT
                    table_name = ReadString(log_file);
                    v1 = ReadValue(log_file);
                    auto table_info = catalog_->GetTable(table_name);
                    if (table_info) {
                        std::vector<Value> vals = {v1};
                        Tuple t(vals, table_info->schema_);
                        RID rid;
                        table_info->table_heap_->InsertTuple(t, &rid, nullptr);
                        std::cout << "[UNDO] Reverted DELETE: " << v1.ToString() << std::endl;
                    }
                    break;
                }
                case LogRecordType::UPDATE: { // UNDO -> REVERSE UPDATE
                    table_name = ReadString(log_file);
                    v1 = ReadValue(log_file); // Old
                    v2 = ReadValue(log_file); // New
                    auto table_info = catalog_->GetTable(table_name);
                    if (table_info) {
                        auto iter = table_info->table_heap_->Begin(nullptr);
                        while (iter != table_info->table_heap_->End()) {
                             if ((*iter).GetValue(table_info->schema_, 0).ToString() == v2.ToString()) { // Find NEW
                                 std::vector<Value> original_vals = {v1}; // Swap back to OLD
                                 Tuple original_tuple(original_vals, table_info->schema_);
                                 table_info->table_heap_->UpdateTuple(original_tuple, iter.GetRID(), nullptr);
                                 std::cout << "[UNDO] Reverted UPDATE" << std::endl;
                                 break;
                             }
                             ++iter;
                        }
                    }
                    break;
                }
                default: break;
            }
        }
        std::cout << "=== ROLLBACK COMPLETE ===" << std::endl;
    }
    
    // USED FOR RECOVER TO 'TIMESTAMP'
    void RecoveryManager::RecoverToTime(uint64_t target_time) {
        RollbackToTime(target_time);
    }
    
    // [FIXED] USED FOR AS OF 'TIMESTAMP' (SHADOW COPY)
    void RecoveryManager::ReplayIntoHeap(TableHeap* target_heap, std::string target_table_name, uint64_t target_time) {
        std::string filename = log_manager_->GetLogFileName();
        std::ifstream log_file(filename, std::ios::binary | std::ios::in);
        if (!log_file.is_open()) return;

        std::cout << "[SNAPSHOT] Building '" << target_table_name << "' as of " << target_time << std::endl;

        while (log_file.peek() != EOF) {
            // [FIX] MUST READ HEADER TO GET TIMESTAMP AND TYPE!
            int32_t size = 0;
            log_file.read(reinterpret_cast<char*>(&size), sizeof(int32_t));
            if (log_file.gcount() != sizeof(int32_t)) break;
            if (size <= 0 || size > 10000000) break;

            LogRecord::lsn_t lsn, prev_lsn;
            LogRecord::txn_id_t txn_id;
            LogRecord::timestamp_t timestamp; 
            int log_type_int;

            log_file.read(reinterpret_cast<char*>(&lsn), sizeof(lsn));
            log_file.read(reinterpret_cast<char*>(&prev_lsn), sizeof(prev_lsn));
            log_file.read(reinterpret_cast<char*>(&txn_id), sizeof(txn_id));
            log_file.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
            log_file.read(reinterpret_cast<char*>(&log_type_int), sizeof(int));
            
            LogRecordType type = static_cast<LogRecordType>(log_type_int);

            // 1. Stop if we pass the target time
            if (timestamp > target_time) break;

            // 2. Process Record
            if (type == LogRecordType::INSERT || type == LogRecordType::UPDATE || type == LogRecordType::APPLY_DELETE) {
                std::string logged_table_name = ReadString(log_file);
                Value v1 = ReadValue(log_file);
                Value v2 = (type == LogRecordType::UPDATE) ? ReadValue(log_file) : Value(TypeId::INVALID, 0);

                if (logged_table_name != target_table_name) {
                    continue; 
                }

                if (type == LogRecordType::INSERT) {
                     auto table_info = catalog_->GetTable(target_table_name);
                     if(table_info) {
                        std::vector<Value> vals = {v1};
                        Tuple t(vals, table_info->schema_);
                        RID rid;
                        target_heap->InsertTuple(t, &rid, nullptr);
                     }
                }
                else if (type == LogRecordType::APPLY_DELETE) {
                    auto table_info = catalog_->GetTable(target_table_name);
                    if(table_info) {
                         auto iter = target_heap->Begin(nullptr);
                         while (iter != target_heap->End()) {
                            if ((*iter).GetValue(table_info->schema_, 0).ToString() == v1.ToString()) {
                                target_heap->MarkDelete(iter.GetRID(), nullptr);
                            }
                            ++iter;
                         }
                    }
                }
                else if (type == LogRecordType::UPDATE) {
                    auto table_info = catalog_->GetTable(target_table_name);
                    if(table_info) {
                         auto iter = target_heap->Begin(nullptr);
                         while (iter != target_heap->End()) {
                            if ((*iter).GetValue(table_info->schema_, 0).ToString() == v1.ToString()) {
                                std::vector<Value> vals = {v2};
                                Tuple t(vals, table_info->schema_);
                                target_heap->UpdateTuple(t, iter.GetRID(), nullptr);
                            }
                            ++iter;
                         }
                    }
                }
            }
        }
    }
    
    // Stub for ARIES compliance
    void RecoveryManager::ARIES() {
        RunRecoveryLoop(0, 0);
    }

} // namespace francodb
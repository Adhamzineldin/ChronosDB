#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <sstream>
#include <iostream>

#include "catalog/table_metadata.h"
#include "catalog/index_info.h"
#include "storage/storage_interface.h"  // For IBufferManager
#include "storage/page/free_page_manager.h"

namespace chronosdb {

    // Forward declaration
    class BufferPoolManager;

    // View metadata
    struct ViewInfo {
        std::string name;
        std::string select_query;
    };

    // Procedure metadata
    struct ProcedureInfo {
        std::string name;
        std::vector<std::pair<std::string, std::string>> parameters; // name, type
        std::string body; // raw SQL between BEGIN...END
    };

    // Trigger metadata
    struct TriggerInfo {
        std::string name;
        std::string table_name;
        std::string timing;  // "BEFORE" or "AFTER"
        std::string event;   // "INSERT", "UPDATE", "DELETE"
        std::string body;    // raw SQL between BEGIN...END
    };

    // Schedule metadata
    struct ScheduleInfo {
        std::string name;
        int interval_seconds;
        std::string sql;
    };

    class Catalog {
    public:
        // Accept IBufferManager interface for polymorphic buffer pool usage
        // Allows both BufferPoolManager and PartitionedBufferPoolManager
        Catalog(IBufferManager *bpm);
        ~Catalog();

        TableMetadata *CreateTable(const std::string &table_name, const Schema &schema);
        TableMetadata *GetTable(const std::string &name);
        IndexInfo *CreateIndex(const std::string &index_name, const std::string &table_name, const std::string &col_name);
        IndexInfo *CreateIndex(const std::string &index_name, const std::string &table_name,
                               const std::string &col_name, const std::string &index_type);
        std::vector<IndexInfo*> GetTableIndexes(const std::string &table_name);
        bool DropTable(const std::string &table_name);
        void SaveCatalog();
        void LoadCatalog();
        IndexInfo *GetIndex(const std::string &index_name);
        std::vector<std::string> GetAllTableNames();
        std::vector<TableMetadata*> GetAllTables();

        // Views
        bool CreateView(const std::string &view_name, const std::string &select_query);
        ViewInfo* GetView(const std::string &view_name);
        bool DropView(const std::string &view_name);
        bool IsView(const std::string &name);
        std::vector<std::string> GetAllViewNames();

        // Procedures
        bool CreateProcedure(const std::string &name, const ProcedureInfo &proc);
        ProcedureInfo* GetProcedure(const std::string &name);
        bool DropProcedure(const std::string &name);
        std::vector<std::string> GetAllProcedureNames();

        // Triggers
        bool CreateTrigger(const std::string &name, const TriggerInfo &trigger);
        std::vector<TriggerInfo*> GetTableTriggers(const std::string &table_name,
                                                    const std::string &timing,
                                                    const std::string &event);
        bool DropTrigger(const std::string &name);
        std::vector<std::string> GetAllTriggerNames();

        // Schedules
        bool CreateSchedule(const std::string &name, const ScheduleInfo &schedule);
        bool DropSchedule(const std::string &name);
        std::vector<ScheduleInfo> GetAllSchedules();

        // Schema info for ER diagram
        struct FullSchemaInfo {
            struct TableInfo {
                std::string name;
                std::vector<std::pair<std::string, std::string>> columns; // name, type
                std::vector<std::string> primary_keys;
                struct FKInfo {
                    std::string column;
                    std::string ref_table;
                    std::string ref_column;
                };
                std::vector<FKInfo> foreign_keys;
            };
            std::vector<TableInfo> tables;
        };
        FullSchemaInfo GetFullSchema();

    private:
        IBufferManager *bpm_;
        std::mutex latch_;
        std::atomic<uint32_t> next_table_oid_;
        std::unordered_map<uint32_t, std::unique_ptr<TableMetadata>> tables_;
        std::unordered_map<std::string, uint32_t> names_to_oid_;

        std::unordered_map<std::string, std::unique_ptr<IndexInfo>> indexes_;
        std::unordered_map<std::string, IndexInfo*> index_names_;
        std::unordered_map<std::string, std::unique_ptr<ViewInfo>> views_;
        std::unordered_map<std::string, std::unique_ptr<ProcedureInfo>> procedures_;
        std::unordered_map<std::string, std::unique_ptr<TriggerInfo>> triggers_;
        std::unordered_map<std::string, std::unique_ptr<ScheduleInfo>> schedules_;
    };
}
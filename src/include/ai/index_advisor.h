#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace chronosdb {

class Catalog;

namespace ai {

struct IndexSuggestion {
    std::string table;
    std::string column;
    std::string reason;
    int query_count;
    std::string suggested_sql;
    std::string index_type; // "BTREE" or "HASH"
    double score;
};

class IndexAdvisor {
public:
    static IndexAdvisor& Instance() {
        static IndexAdvisor instance;
        return instance;
    }

    void RecordAccess(const std::string& table, const std::string& column, const std::string& op) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = table + "." + column;
        auto& stats = column_stats_[key];
        stats.access_count++;
        if (op == "=") stats.equality_count++;
        else stats.range_count++;
    }

    std::vector<IndexSuggestion> GetSuggestions(Catalog* catalog) const;

private:
    IndexAdvisor() = default;

    struct ColumnStats {
        size_t access_count = 0;
        size_t equality_count = 0;
        size_t range_count = 0;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ColumnStats> column_stats_;
};

} // namespace ai
} // namespace chronosdb

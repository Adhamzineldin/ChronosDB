#include "ai/index_advisor.h"
#include "catalog/catalog.h"

namespace chronosdb {
namespace ai {

std::vector<IndexSuggestion> IndexAdvisor::GetSuggestions(Catalog* catalog) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<IndexSuggestion> suggestions;

    if (!catalog) return suggestions;

    for (const auto& [key, stats] : column_stats_) {
        if (stats.access_count < 5) continue; // Need enough data

        size_t dot = key.find('.');
        if (dot == std::string::npos) continue;

        std::string table = key.substr(0, dot);
        std::string column = key.substr(dot + 1);

        // Check if index already exists
        auto indexes = catalog->GetTableIndexes(table);
        bool has_index = false;
        for (auto* idx : indexes) {
            if (idx->col_name_ == column) {
                has_index = true;
                break;
            }
        }

        if (has_index) continue;

        IndexSuggestion suggestion;
        suggestion.table = table;
        suggestion.column = column;
        suggestion.query_count = static_cast<int>(stats.access_count);
        suggestion.score = static_cast<double>(stats.access_count);

        if (stats.equality_count > stats.range_count) {
            suggestion.index_type = "HASH";
            suggestion.reason = "High equality lookups (" + std::to_string(stats.equality_count) + " queries)";
            suggestion.suggested_sql = "CREATE HASH INDEX idx_" + table + "_" + column +
                                       " ON " + table + "(" + column + ");";
        } else {
            suggestion.index_type = "BTREE";
            suggestion.reason = "Frequent range/equality lookups (" + std::to_string(stats.access_count) + " queries)";
            suggestion.suggested_sql = "CREATE INDEX idx_" + table + "_" + column +
                                       " ON " + table + "(" + column + ");";
        }

        suggestions.push_back(suggestion);
    }

    // Sort by score descending
    std::sort(suggestions.begin(), suggestions.end(),
        [](const IndexSuggestion& a, const IndexSuggestion& b) {
            return a.score > b.score;
        });

    return suggestions;
}

} // namespace ai
} // namespace chronosdb

#pragma once

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <chrono>
#include <cstdint>

namespace chronosdb {

struct QueryRecord {
    uint64_t timestamp_us;
    std::string query;
    std::string user;
    std::string database;
    bool success;
    double execution_time_ms;
};

class QueryHistory {
public:
    static QueryHistory& Instance() {
        static QueryHistory instance;
        return instance;
    }

    void Record(const QueryRecord& record) {
        std::lock_guard<std::mutex> lock(mutex_);
        history_.push_back(record);
        if (history_.size() > max_records_) {
            history_.pop_front();
        }
    }

    std::vector<QueryRecord> GetRecent(size_t limit) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<QueryRecord> result;
        size_t count = std::min(limit, history_.size());
        auto it = history_.rbegin();
        for (size_t i = 0; i < count && it != history_.rend(); ++i, ++it) {
            result.push_back(*it);
        }
        return result;
    }

    double GetQueriesPerSecond(int window_seconds = 60) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (history_.empty()) return 0.0;

        auto now = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        uint64_t cutoff = now - (static_cast<uint64_t>(window_seconds) * 1000000ULL);

        int count = 0;
        for (auto it = history_.rbegin(); it != history_.rend(); ++it) {
            if (it->timestamp_us >= cutoff) count++;
            else break;
        }
        return static_cast<double>(count) / window_seconds;
    }

    size_t GetTotalCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return history_.size();
    }

private:
    QueryHistory() = default;
    mutable std::mutex mutex_;
    std::deque<QueryRecord> history_;
    size_t max_records_ = 10000;
};

} // namespace chronosdb

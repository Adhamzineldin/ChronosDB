#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <deque>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <algorithm>

namespace chronosdb {
namespace ai {

struct BlockedQuery {
    int id;
    std::string sql;
    std::string user;
    std::string reason;
    uint64_t timestamp;
    bool approved;
};

class QueryFirewall {
public:
    static QueryFirewall& Instance() {
        static QueryFirewall instance;
        return instance;
    }

    // Returns true if query is allowed, false if blocked
    bool Check(const std::string& sql, const std::string& user) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Rate limiting
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        auto& user_info = user_rates_[user];
        if (now - user_info.window_start > 60) {
            user_info.count = 0;
            user_info.window_start = now;
        }
        user_info.count++;

        if (user_info.count > rate_limit_) {
            BlockQuery(sql, user, "RATE_LIMIT", now * 1000000ULL);
            return false;
        }

        // Pattern-based detection
        std::string upper_sql = sql;
        std::transform(upper_sql.begin(), upper_sql.end(), upper_sql.begin(), ::toupper);

        // Check for SQL injection patterns
        if (upper_sql.find("'; DROP") != std::string::npos ||
            upper_sql.find("1=1") != std::string::npos ||
            upper_sql.find("UNION SELECT") != std::string::npos ||
            upper_sql.find("--") != std::string::npos) {
            BlockQuery(sql, user, "SQL_INJECTION", now * 1000000ULL);
            return false;
        }

        // Check for XSS patterns
        if (sql.find("<script") != std::string::npos ||
            sql.find("javascript:") != std::string::npos) {
            BlockQuery(sql, user, "XSS_ATTEMPT", now * 1000000ULL);
            return false;
        }

        return true;
    }

    bool Approve(int query_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& q : blocked_) {
            if (q.id == query_id) {
                q.approved = true;
                return true;
            }
        }
        return false;
    }

    std::vector<BlockedQuery> GetBlocked() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::vector<BlockedQuery>(blocked_.begin(), blocked_.end());
    }

    void SetRateLimit(int limit) { rate_limit_ = limit; }

private:
    QueryFirewall() = default;

    void BlockQuery(const std::string& sql, const std::string& user,
                    const std::string& reason, uint64_t timestamp) {
        BlockedQuery bq;
        bq.id = next_id_++;
        bq.sql = sql;
        bq.user = user;
        bq.reason = reason;
        bq.timestamp = timestamp;
        bq.approved = false;
        blocked_.push_back(bq);
        if (blocked_.size() > 1000) blocked_.pop_front();
    }

    struct UserRate {
        int count = 0;
        int64_t window_start = 0;
    };

    mutable std::mutex mutex_;
    std::deque<BlockedQuery> blocked_;
    std::unordered_map<std::string, UserRate> user_rates_;
    int rate_limit_ = 1000;
    std::atomic<int> next_id_{1};
};

} // namespace ai
} // namespace chronosdb

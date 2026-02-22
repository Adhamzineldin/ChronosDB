#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstdint>

namespace chronosdb {

class ReplicationManager {
public:
    enum class Role { STANDALONE, PRIMARY, REPLICA };

    static ReplicationManager& Instance() {
        static ReplicationManager instance;
        return instance;
    }

    void SetRole(Role role) {
        std::lock_guard<std::mutex> lock(mutex_);
        role_ = role;
    }

    Role GetRole() const { return role_; }

    // Primary side
    void AddReplica(const std::string& host, int port) {
        std::lock_guard<std::mutex> lock(mutex_);
        replica_hosts_.push_back(host + ":" + std::to_string(port));
    }

    void RemoveReplica(const std::string& host, int port) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string target = host + ":" + std::to_string(port);
        replica_hosts_.erase(
            std::remove(replica_hosts_.begin(), replica_hosts_.end(), target),
            replica_hosts_.end());
    }

    void StreamWAL(const std::vector<uint8_t>& log_record) {
        if (role_ != Role::PRIMARY) return;
        std::lock_guard<std::mutex> lock(mutex_);
        last_wal_lsn_++;
        // In a real implementation, send to connected replicas via TCP
    }

    // Replica side
    void ConnectToPrimary(const std::string& host, int port) {
        std::lock_guard<std::mutex> lock(mutex_);
        primary_host_ = host;
        primary_port_ = port;
        primary_alive_ = true;
    }

    void ApplyWAL(const std::vector<uint8_t>& log_record) {
        // Apply log record to local storage
        std::lock_guard<std::mutex> lock(mutex_);
        last_wal_lsn_++;
    }

    // Health & Failover
    void StartHealthCheck(int interval_ms = 5000) {
        health_check_interval_ = interval_ms;
    }

    void PromoteToMaster() {
        std::lock_guard<std::mutex> lock(mutex_);
        role_ = Role::PRIMARY;
        primary_host_.clear();
    }

    bool IsPrimaryAlive() const { return primary_alive_; }

    struct ReplicationStatus {
        Role role;
        int replica_count;
        uint64_t last_wal_lsn;
        uint64_t replica_lag_bytes;
        bool primary_alive;
        std::vector<std::string> replica_hosts;
        std::string primary_host;
    };

    ReplicationStatus GetStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {
            role_,
            static_cast<int>(replica_hosts_.size()),
            last_wal_lsn_,
            0,
            primary_alive_,
            replica_hosts_,
            primary_host_.empty() ? "N/A" : primary_host_ + ":" + std::to_string(primary_port_)
        };
    }

private:
    ReplicationManager() = default;

    mutable std::mutex mutex_;
    std::atomic<Role> role_{Role::STANDALONE};
    std::vector<std::string> replica_hosts_;
    std::string primary_host_;
    int primary_port_ = 2501;
    std::atomic<bool> primary_alive_{false};
    uint64_t last_wal_lsn_ = 0;
    int health_check_interval_ = 5000;
};

} // namespace chronosdb

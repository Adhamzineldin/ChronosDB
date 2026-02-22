#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>

namespace chronosdb {

struct ScheduledJob {
    std::string name;
    std::string sql;
    int interval_seconds;
    bool enabled = true;
    uint64_t last_run = 0;
    int run_count = 0;
};

class Scheduler {
public:
    static Scheduler& Instance() {
        static Scheduler instance;
        return instance;
    }

    void Start(std::function<void(const std::string&)> executor) {
        if (running_) return;
        executor_ = executor;
        running_ = true;
        worker_ = std::thread([this]() { WorkerLoop(); });
    }

    void Stop() {
        running_ = false;
        if (worker_.joinable()) worker_.join();
    }

    void AddJob(const ScheduledJob& job) {
        std::lock_guard<std::mutex> lock(mutex_);
        // Remove existing job with same name
        jobs_.erase(
            std::remove_if(jobs_.begin(), jobs_.end(),
                [&](const ScheduledJob& j) { return j.name == job.name; }),
            jobs_.end());
        jobs_.push_back(job);
    }

    bool RemoveJob(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::remove_if(jobs_.begin(), jobs_.end(),
            [&](const ScheduledJob& j) { return j.name == name; });
        if (it == jobs_.end()) return false;
        jobs_.erase(it, jobs_.end());
        return true;
    }

    std::vector<ScheduledJob> GetJobs() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return jobs_;
    }

private:
    Scheduler() = default;
    ~Scheduler() { Stop(); }

    void WorkerLoop() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!running_) break;

            auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& job : jobs_) {
                if (!job.enabled) continue;
                if (now - static_cast<int64_t>(job.last_run) >= job.interval_seconds) {
                    job.last_run = now;
                    job.run_count++;
                    if (executor_) {
                        try { executor_(job.sql); } catch (...) {}
                    }
                }
            }
        }
    }

    mutable std::mutex mutex_;
    std::vector<ScheduledJob> jobs_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::function<void(const std::string&)> executor_;
};

} // namespace chronosdb

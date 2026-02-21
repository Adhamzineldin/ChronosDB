/**
 * ChronosDB Benchmark Suite
 *
 * Tests and compares performance of key database components:
 * 1. Sequential Scan vs Index Scan vs Hash Index
 * 2. Buffer Pool Strategies
 * 3. AI Optimizer convergence
 * 4. Time Travel performance
 * 5. Write Throughput
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <iomanip>
#include <numeric>
#include <filesystem>
#include <sstream>
#include <algorithm>

#include "storage/disk/disk_manager.h"
#include "buffer/buffer_pool_manager.h"
#include "catalog/catalog.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "execution/execution_engine.h"
#include "common/auth_manager.h"
#include "network/database_registry.h"
#include "recovery/log_manager.h"

using namespace chronosdb;
namespace fs = std::filesystem;

// ============================================================================
// TIMING UTILITIES
// ============================================================================

struct BenchmarkResult {
    std::string name;
    double avg_ms;
    double min_ms;
    double max_ms;
    int runs;
    std::string extra;
};

class Timer {
public:
    void start() { start_ = std::chrono::high_resolution_clock::now(); }
    double stop_ms() {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }
private:
    std::chrono::high_resolution_clock::time_point start_;
};

std::vector<double> runBenchmark(int runs, std::function<void()> fn) {
    std::vector<double> times;
    Timer timer;
    for (int i = 0; i < runs; i++) {
        timer.start();
        fn();
        times.push_back(timer.stop_ms());
    }
    return times;
}

BenchmarkResult summarize(const std::string& name, const std::vector<double>& times, const std::string& extra = "") {
    BenchmarkResult r;
    r.name = name;
    r.runs = static_cast<int>(times.size());
    r.avg_ms = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    r.min_ms = *std::min_element(times.begin(), times.end());
    r.max_ms = *std::max_element(times.begin(), times.end());
    r.extra = extra;
    return r;
}

void printResult(const BenchmarkResult& r) {
    std::cout << "  " << std::left << std::setw(28) << r.name
              << std::right << std::fixed << std::setprecision(2)
              << std::setw(8) << r.avg_ms << "ms avg"
              << " (" << r.runs << " runs)";
    if (!r.extra.empty()) std::cout << "  " << r.extra;
    std::cout << std::endl;
}

// ============================================================================
// BENCHMARK SETUP HELPERS
// ============================================================================

struct BenchmarkEnv {
    std::string db_path;
    std::unique_ptr<DiskManager> disk_mgr;
    std::unique_ptr<BufferPoolManager> bpm;
    std::unique_ptr<Catalog> catalog;
    std::unique_ptr<AuthManager> auth_mgr;
    std::unique_ptr<DatabaseRegistry> db_registry;
    std::unique_ptr<LogManager> log_mgr;
    std::unique_ptr<ExecutionEngine> engine;
    SessionContext session;

    BenchmarkEnv(const std::string& name) {
        db_path = "benchmark_" + name;

        // Clean up any previous benchmark data
        if (fs::exists(db_path)) {
            fs::remove_all(db_path);
        }
        fs::create_directories(db_path);

        std::string db_file = db_path + "/" + name + ".chronosdb";
        disk_mgr = std::make_unique<DiskManager>(db_file);
        bpm = std::make_unique<BufferPoolManager>(4096, disk_mgr.get());
        catalog = std::make_unique<Catalog>(bpm.get());
        auth_mgr = std::make_unique<AuthManager>();
        db_registry = std::make_unique<DatabaseRegistry>();

        std::string log_file = db_path + "/" + name + ".wal";
        log_mgr = std::make_unique<LogManager>(log_file);

        engine = std::make_unique<ExecutionEngine>(
            bpm.get(), catalog.get(), auth_mgr.get(), db_registry.get(), log_mgr.get(), false);

        session.current_user = "benchmark";
        session.current_db = name;
        session.role = UserRole::SUPERADMIN;
        session.is_authenticated = true;
    }

    ExecutionResult execute(const std::string& sql) {
        Lexer lexer(sql);
        Parser parser(std::move(lexer));
        auto stmt = parser.ParseQuery();
        return engine->Execute(stmt.get(), &session);
    }

    ~BenchmarkEnv() {
        engine.reset();
        catalog.reset();
        bpm.reset();
        disk_mgr.reset();
        log_mgr.reset();

        // Cleanup temp files
        try {
            if (fs::exists(db_path)) {
                fs::remove_all(db_path);
            }
        } catch (...) {}
    }
};

// ============================================================================
// BENCHMARK 1: Scan Strategy Comparison
// ============================================================================

void benchmarkScanStrategies(int row_count = 10000) {
    std::cout << "\n[1/5] Scan Strategy Comparison (" << row_count << " rows)" << std::endl;

    BenchmarkEnv env("scan_bench");

    // Create table
    env.execute("CREATE TABLE bench (id INT PRIMARY KEY, name VARCHAR, age INT);");

    // Insert rows
    for (int i = 0; i < row_count; i++) {
        std::string sql = "INSERT INTO bench VALUES (" + std::to_string(i) +
                          ", 'user" + std::to_string(i) + "', " + std::to_string(20 + (i % 50)) + ");";
        env.execute(sql);
    }

    int lookup_id = row_count / 2; // Look up a middle row
    std::string query = "SELECT * FROM bench WHERE id = " + std::to_string(lookup_id) + ";";

    // Benchmark 1: Sequential Scan (no index)
    auto seq_times = runBenchmark(10, [&]() {
        env.execute(query);
    });
    auto seq_result = summarize("Sequential Scan", seq_times);

    // Create B+ Tree index
    env.execute("CREATE INDEX idx_bench_id ON bench(id);");

    // Benchmark 2: B+ Tree Index Scan
    auto btree_times = runBenchmark(10, [&]() {
        env.execute(query);
    });

    // Drop B+Tree index (by recreating env with hash index)
    // Instead, create a hash index on a different column for comparison
    env.execute("CREATE HASH INDEX idx_bench_age ON bench(age);");

    std::string hash_query = "SELECT * FROM bench WHERE age = 25;";

    // Benchmark 3: Hash Index Scan
    auto hash_times = runBenchmark(10, [&]() {
        env.execute(hash_query);
    });

    // Print results
    printResult(seq_result);

    double btree_speedup = seq_result.avg_ms / (std::max(0.01, summarize("", btree_times).avg_ms));
    auto btree_result = summarize("B+ Tree Index Scan", btree_times,
        "[" + std::to_string(static_cast<int>(btree_speedup)) + "x faster]");
    printResult(btree_result);

    double hash_speedup = seq_result.avg_ms / (std::max(0.01, summarize("", hash_times).avg_ms));
    auto hash_result = summarize("Hash Index Scan", hash_times,
        "[" + std::to_string(static_cast<int>(hash_speedup)) + "x faster]");
    printResult(hash_result);
}

// ============================================================================
// BENCHMARK 2: Buffer Pool Strategies
// ============================================================================

void benchmarkBufferPool(int access_count = 10000) {
    std::cout << "\n[2/5] Buffer Pool Performance (" << access_count << " page accesses)" << std::endl;

    BenchmarkEnv env("bpool_bench");

    // Create table with enough data to span multiple pages
    env.execute("CREATE TABLE pages (id INT PRIMARY KEY, data VARCHAR);");

    for (int i = 0; i < 500; i++) {
        std::string data(100, 'A' + (i % 26));
        env.execute("INSERT INTO pages VALUES (" + std::to_string(i) + ", '" + data + "');");
    }

    // Benchmark: random access pattern
    auto times = runBenchmark(5, [&]() {
        for (int i = 0; i < access_count / 5; i++) {
            int id = (i * 37) % 500; // Pseudo-random pattern
            env.execute("SELECT * FROM pages WHERE id = " + std::to_string(id) + ";");
        }
    });

    auto result = summarize("Default Buffer Pool", times);
    printResult(result);

    // Sequential access pattern
    auto seq_times = runBenchmark(5, [&]() {
        for (int i = 0; i < access_count / 5; i++) {
            int id = i % 500;
            env.execute("SELECT * FROM pages WHERE id = " + std::to_string(id) + ";");
        }
    });

    auto seq_result = summarize("Sequential Access", seq_times);
    printResult(seq_result);
}

// ============================================================================
// BENCHMARK 3: AI Optimizer
// ============================================================================

void benchmarkAIOptimizer() {
    std::cout << "\n[3/5] AI Optimizer Convergence" << std::endl;

    BenchmarkEnv env("ai_bench");

    env.execute("CREATE TABLE ai_test (id INT PRIMARY KEY, val INT);");
    env.execute("CREATE INDEX idx_ai_id ON ai_test(id);");

    for (int i = 0; i < 1000; i++) {
        env.execute("INSERT INTO ai_test VALUES (" + std::to_string(i) + ", " + std::to_string(i * 10) + ");");
    }

    // Run queries to let AI learn
    auto first_10_times = runBenchmark(10, [&]() {
        env.execute("SELECT * FROM ai_test WHERE id = 500;");
    });

    // Run more queries for AI to converge
    for (int i = 0; i < 50; i++) {
        env.execute("SELECT * FROM ai_test WHERE id = " + std::to_string(i * 20) + ";");
    }

    auto last_10_times = runBenchmark(10, [&]() {
        env.execute("SELECT * FROM ai_test WHERE id = 500;");
    });

    auto early_result = summarize("First 10 queries (exploring)", first_10_times);
    auto late_result = summarize("After 50 queries (learned)", last_10_times);

    printResult(early_result);
    printResult(late_result);

    double improvement = ((early_result.avg_ms - late_result.avg_ms) / std::max(0.01, early_result.avg_ms)) * 100;
    std::cout << "  Improvement: " << std::fixed << std::setprecision(1) << improvement << "%" << std::endl;
}

// ============================================================================
// BENCHMARK 4: Time Travel
// ============================================================================

void benchmarkTimeTravel() {
    std::cout << "\n[4/5] Time Travel Performance" << std::endl;

    BenchmarkEnv env("tt_bench");

    env.execute("CREATE TABLE history (id INT PRIMARY KEY, version INT);");

    // Insert initial data
    for (int i = 0; i < 100; i++) {
        env.execute("INSERT INTO history VALUES (" + std::to_string(i) + ", 1);");
    }

    // Do some updates to create history
    for (int v = 2; v <= 5; v++) {
        for (int i = 0; i < 100; i++) {
            env.execute("UPDATE history SET version = " + std::to_string(v) +
                        " WHERE id = " + std::to_string(i) + ";");
        }
    }

    // Benchmark: current state query
    auto current_times = runBenchmark(10, [&]() {
        env.execute("SELECT * FROM history WHERE id = 50;");
    });

    auto current_result = summarize("Current State Query", current_times);
    printResult(current_result);

    std::cout << "  (Time Travel AS OF queries require WAL log)" << std::endl;
}

// ============================================================================
// BENCHMARK 5: Write Throughput
// ============================================================================

void benchmarkWriteThroughput() {
    std::cout << "\n[5/5] Write Throughput" << std::endl;

    // Single row inserts
    {
        BenchmarkEnv env("write_single");
        env.execute("CREATE TABLE single_w (id INT PRIMARY KEY, name VARCHAR, val INT);");

        Timer timer;
        timer.start();
        int count = 1000;
        for (int i = 0; i < count; i++) {
            env.execute("INSERT INTO single_w VALUES (" + std::to_string(i) +
                        ", 'item" + std::to_string(i) + "', " + std::to_string(i * 10) + ");");
        }
        double elapsed = timer.stop_ms();

        double rows_per_sec = (count / elapsed) * 1000.0;
        std::cout << "  " << std::left << std::setw(28) << "Single-Row INSERT"
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(8) << elapsed << "ms"
                  << "  (" << static_cast<int>(rows_per_sec) << " rows/sec)" << std::endl;
    }

    // Batch inserts (multi-row)
    {
        BenchmarkEnv env("write_batch");
        env.execute("CREATE TABLE batch_w (id INT PRIMARY KEY, name VARCHAR, val INT);");

        Timer timer;
        timer.start();
        int total = 1000;
        int batch_size = 50;
        for (int b = 0; b < total; b += batch_size) {
            std::string sql = "INSERT INTO batch_w VALUES ";
            for (int i = b; i < b + batch_size && i < total; i++) {
                if (i > b) sql += ", ";
                sql += "(" + std::to_string(i) + ", 'item" + std::to_string(i) + "', " + std::to_string(i * 10) + ")";
            }
            sql += ";";
            env.execute(sql);
        }
        double elapsed = timer.stop_ms();

        double rows_per_sec = (total / elapsed) * 1000.0;
        std::cout << "  " << std::left << std::setw(28) << ("Batch INSERT (size " + std::to_string(batch_size) + ")")
                  << std::right << std::fixed << std::setprecision(2)
                  << std::setw(8) << elapsed << "ms"
                  << "  (" << static_cast<int>(rows_per_sec) << " rows/sec)" << std::endl;
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "=== ChronosDB Benchmark Suite ===" << std::endl;
    std::cout << "Running benchmarks..." << std::endl;

    Timer total_timer;
    total_timer.start();

    try {
        benchmarkScanStrategies(1000);  // Use smaller count for quick benchmark
        benchmarkBufferPool(1000);
        benchmarkAIOptimizer();
        benchmarkTimeTravel();
        benchmarkWriteThroughput();
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] Benchmark failed: " << e.what() << std::endl;
        return 1;
    }

    double total_ms = total_timer.stop_ms();
    std::cout << "\n=== Benchmark Complete ===" << std::endl;
    std::cout << "Total time: " << std::fixed << std::setprecision(1) << total_ms / 1000.0 << "s" << std::endl;

    return 0;
}

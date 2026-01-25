#pragma once

#include "buffer/buffer_pool_manager.h"
#include "storage/storage_interface.h"
#include "storage/disk/disk_manager.h"
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <iostream>

namespace francodb {

/**
 * Buffer Pool Metrics for adaptive sizing decisions
 */
struct BufferPoolMetrics {
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};
    std::atomic<uint64_t> evictions{0};
    std::atomic<uint64_t> dirty_pages{0};
    std::atomic<uint64_t> total_pages{0};
    
    double GetHitRate() const {
        uint64_t hits = cache_hits.load();
        uint64_t misses = cache_misses.load();
        uint64_t total = hits + misses;
        return total > 0 ? static_cast<double>(hits) / total * 100.0 : 100.0;
    }
    
    double GetDirtyRatio() const {
        uint64_t dirty = dirty_pages.load();
        uint64_t total = total_pages.load();
        return total > 0 ? static_cast<double>(dirty) / total * 100.0 : 0.0;
    }
    
    double GetEvictionRate() const {
        // Evictions per total operations
        uint64_t evict = evictions.load();
        uint64_t total = cache_hits.load() + cache_misses.load();
        return total > 0 ? static_cast<double>(evict) / total * 100.0 : 0.0;
    }
    
    void Reset() {
        cache_hits = 0;
        cache_misses = 0;
        evictions = 0;
    }
};

/**
 * Adaptive Buffer Pool Configuration
 */
struct AdaptivePoolConfig {
    // Size bounds (in pages)
    size_t min_pool_size = 64 * 1024;      // 256MB minimum (64K * 4KB pages)
    size_t max_pool_size = 512 * 1024;     // 2GB maximum (or 50% RAM)
    size_t initial_pool_size = 256 * 1024; // 1GB initial
    
    // Chunk size for growth/shrink (in pages)
    size_t chunk_size = 32 * 1024;         // 128MB chunks
    
    // Thresholds for adaptation
    double hit_rate_grow_threshold = 90.0;   // Grow if hit rate < 90%
    double hit_rate_shrink_threshold = 98.0; // Consider shrink if > 98%
    double dirty_ratio_throttle = 70.0;      // Throttle writers if dirty > 70%
    double eviction_rate_grow_threshold = 5.0; // Grow if eviction rate > 5%
    
    // Adaptation interval
    uint32_t adaptation_interval_seconds = 30;
    
    // Memory pressure detection
    double max_memory_usage_ratio = 0.5;    // Never use more than 50% of system RAM
};

/**
 * Buffer Pool Chunk - A segment of the buffer pool
 * Allows cheap growth and shrink without reallocation
 */
class BufferPoolChunk {
public:
    BufferPoolChunk(size_t num_pages, DiskManager* disk_manager)
        : size_(num_pages), disk_manager_(disk_manager) {
        bpm_ = std::make_unique<BufferPoolManager>(num_pages, disk_manager_);
    }
    
    BufferPoolManager* GetBPM() { return bpm_.get(); }
    size_t GetSize() const { return size_; }
    
private:
    size_t size_;
    DiskManager* disk_manager_;
    std::unique_ptr<BufferPoolManager> bpm_;
};

/**
 * Adaptive Buffer Pool Manager
 * 
 * A policy-driven, dynamically-sized buffer pool that:
 * 1. Starts with configured base size
 * 2. Grows when hit rate drops or eviction rate is high
 * 3. Shrinks when memory pressure rises
 * 4. Uses chunk-based allocation to avoid realloc fragmentation
 * 
 * Implements IBufferManager for drop-in replacement.
 */
class AdaptiveBufferPoolManager : public IBufferManager {
public:
    AdaptiveBufferPoolManager(DiskManager* disk_manager, 
                               const AdaptivePoolConfig& config = AdaptivePoolConfig())
        : disk_manager_(disk_manager), config_(config), running_(false) {
        
        // Initialize with configured size, rounded to chunk boundaries
        size_t initial_chunks = (config_.initial_pool_size + config_.chunk_size - 1) / config_.chunk_size;
        initial_chunks = std::max(initial_chunks, size_t(1));
        
        for (size_t i = 0; i < initial_chunks; i++) {
            AddChunk();
        }
        
        std::cout << "[AdaptivePool] Initialized with " << chunks_.size() << " chunks ("
                  << GetTotalPages() << " pages, " << GetTotalPages() * 4 / 1024 << " MB)" << std::endl;
    }
    
    ~AdaptiveBufferPoolManager() override {
        StopAdaptationThread();
    }
    
    // ========================================================================
    // IBufferManager Interface Implementation
    // ========================================================================
    
    Page* FetchPage(page_id_t page_id) override {
        // Simple hash-based routing to chunks
        size_t chunk_idx = page_id % chunks_.size();
        
        std::shared_lock<std::shared_mutex> lock(chunks_mutex_);
        if (chunk_idx >= chunks_.size()) {
            chunk_idx = 0;
        }
        
        Page* page = chunks_[chunk_idx]->GetBPM()->FetchPage(page_id);
        
        if (page) {
            metrics_.cache_hits++;
        } else {
            metrics_.cache_misses++;
        }
        
        return page;
    }
    
    Page* NewPage(page_id_t* page_id) override {
        std::shared_lock<std::shared_mutex> lock(chunks_mutex_);
        
        // Try each chunk until one succeeds
        for (size_t i = 0; i < chunks_.size(); i++) {
            Page* page = chunks_[i]->GetBPM()->NewPage(page_id);
            if (page) {
                metrics_.total_pages++;
                return page;
            }
        }
        
        // All chunks full - trigger growth signal
        metrics_.cache_misses++;
        return nullptr;
    }
    
    bool UnpinPage(page_id_t page_id, bool is_dirty) override {
        size_t chunk_idx = page_id % chunks_.size();
        
        std::shared_lock<std::shared_mutex> lock(chunks_mutex_);
        if (chunk_idx >= chunks_.size()) {
            chunk_idx = 0;
        }
        
        if (is_dirty) {
            metrics_.dirty_pages++;
        }
        
        return chunks_[chunk_idx]->GetBPM()->UnpinPage(page_id, is_dirty);
    }
    
    bool DeletePage(page_id_t page_id) override {
        size_t chunk_idx = page_id % chunks_.size();
        
        std::shared_lock<std::shared_mutex> lock(chunks_mutex_);
        if (chunk_idx >= chunks_.size()) {
            chunk_idx = 0;
        }
        
        return chunks_[chunk_idx]->GetBPM()->DeletePage(page_id);
    }
    
    bool FlushPage(page_id_t page_id) override {
        size_t chunk_idx = page_id % chunks_.size();
        
        std::shared_lock<std::shared_mutex> lock(chunks_mutex_);
        if (chunk_idx >= chunks_.size()) {
            chunk_idx = 0;
        }
        
        return chunks_[chunk_idx]->GetBPM()->FlushPage(page_id);
    }
    
    void FlushAllPages() override {
        std::shared_lock<std::shared_mutex> lock(chunks_mutex_);
        for (auto& chunk : chunks_) {
            chunk->GetBPM()->FlushAllPages();
        }
        metrics_.dirty_pages = 0;
    }
    
    DiskManager* GetDiskManager() override {
        return disk_manager_;
    }
    
    size_t GetPoolSize() const override {
        return GetTotalPages();
    }
    
    void Clear() override {
        FlushAllPages();
        // Note: We don't deallocate chunks, just flush them
    }
    
    void SetLogManager(class LogManager* log_manager) override {
        std::shared_lock<std::shared_mutex> lock(chunks_mutex_);
        for (auto& chunk : chunks_) {
            chunk->GetBPM()->SetLogManager(log_manager);
        }
    }
    
    // ========================================================================
    // Adaptive Pool Management
    // ========================================================================
    
    void StartAdaptationThread() {
        if (running_.load()) return;
        
        running_ = true;
        adaptation_thread_ = std::thread(&AdaptiveBufferPoolManager::AdaptationLoop, this);
        std::cout << "[AdaptivePool] Adaptation thread started (interval: " 
                  << config_.adaptation_interval_seconds << "s)" << std::endl;
    }
    
    void StopAdaptationThread() {
        running_ = false;
        if (adaptation_thread_.joinable()) {
            adaptation_thread_.join();
        }
    }
    
    const BufferPoolMetrics& GetMetrics() const { return metrics_; }
    
    size_t GetTotalPages() const {
        std::shared_lock<std::shared_mutex> lock(chunks_mutex_);
        size_t total = 0;
        for (const auto& chunk : chunks_) {
            total += chunk->GetSize();
        }
        return total;
    }
    
    size_t GetChunkCount() const {
        std::shared_lock<std::shared_mutex> lock(chunks_mutex_);
        return chunks_.size();
    }
    
    void PrintStatus() const {
        std::cout << "[AdaptivePool] Status:" << std::endl;
        std::cout << "  Chunks: " << GetChunkCount() << std::endl;
        std::cout << "  Total Pages: " << GetTotalPages() << std::endl;
        std::cout << "  Memory: " << GetTotalPages() * 4 / 1024 << " MB" << std::endl;
        std::cout << "  Hit Rate: " << metrics_.GetHitRate() << "%" << std::endl;
        std::cout << "  Eviction Rate: " << metrics_.GetEvictionRate() << "%" << std::endl;
        std::cout << "  Dirty Ratio: " << metrics_.GetDirtyRatio() << "%" << std::endl;
    }

private:
    void AddChunk() {
        std::unique_lock<std::shared_mutex> lock(chunks_mutex_);
        
        size_t current_total = 0;
        for (const auto& chunk : chunks_) {
            current_total += chunk->GetSize();
        }
        
        if (current_total + config_.chunk_size > config_.max_pool_size) {
            std::cout << "[AdaptivePool] Cannot grow: would exceed max pool size" << std::endl;
            return;
        }
        
        auto chunk = std::make_unique<BufferPoolChunk>(config_.chunk_size, disk_manager_);
        chunks_.push_back(std::move(chunk));
        
        std::cout << "[AdaptivePool] Added chunk. Total: " << chunks_.size() 
                  << " chunks (" << (current_total + config_.chunk_size) * 4 / 1024 << " MB)" << std::endl;
    }
    
    void RemoveChunk() {
        std::unique_lock<std::shared_mutex> lock(chunks_mutex_);
        
        if (chunks_.size() <= 1) {
            return; // Keep at least one chunk
        }
        
        size_t current_total = 0;
        for (const auto& chunk : chunks_) {
            current_total += chunk->GetSize();
        }
        
        if (current_total - config_.chunk_size < config_.min_pool_size) {
            std::cout << "[AdaptivePool] Cannot shrink: would go below min pool size" << std::endl;
            return;
        }
        
        // Flush the last chunk before removing
        chunks_.back()->GetBPM()->FlushAllPages();
        chunks_.pop_back();
        
        std::cout << "[AdaptivePool] Removed chunk. Total: " << chunks_.size() 
                  << " chunks (" << (current_total - config_.chunk_size) * 4 / 1024 << " MB)" << std::endl;
    }
    
    void AdaptationLoop() {
        while (running_.load()) {
            // Sleep in small increments to respond to shutdown
            for (uint32_t i = 0; i < config_.adaptation_interval_seconds * 10 && running_.load(); i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            if (!running_.load()) break;
            
            // Collect metrics
            double hit_rate = metrics_.GetHitRate();
            double eviction_rate = metrics_.GetEvictionRate();
            double dirty_ratio = metrics_.GetDirtyRatio();
            
            // Decision logic
            bool should_grow = false;
            bool should_shrink = false;
            
            // Grow if hit rate is too low
            if (hit_rate < config_.hit_rate_grow_threshold) {
                should_grow = true;
                std::cout << "[AdaptivePool] Low hit rate (" << hit_rate 
                          << "%) - considering growth" << std::endl;
            }
            
            // Grow if eviction rate is high
            if (eviction_rate > config_.eviction_rate_grow_threshold) {
                should_grow = true;
                std::cout << "[AdaptivePool] High eviction rate (" << eviction_rate 
                          << "%) - considering growth" << std::endl;
            }
            
            // Shrink if hit rate is very high and we have excess capacity
            if (hit_rate > config_.hit_rate_shrink_threshold && GetChunkCount() > 1) {
                should_shrink = true;
                std::cout << "[AdaptivePool] High hit rate (" << hit_rate 
                          << "%) - considering shrink" << std::endl;
            }
            
            // Check system memory pressure (simplified - would need OS-specific calls)
            // For now, use dirty ratio as a proxy
            if (dirty_ratio > config_.dirty_ratio_throttle) {
                should_grow = false;  // Don't grow under pressure
                std::cout << "[AdaptivePool] High dirty ratio (" << dirty_ratio 
                          << "%) - throttling growth" << std::endl;
            }
            
            // Apply decision
            if (should_grow && !should_shrink) {
                AddChunk();
            } else if (should_shrink && !should_grow) {
                RemoveChunk();
            }
            
            // Reset metrics for next interval
            metrics_.Reset();
        }
    }
    
    DiskManager* disk_manager_;
    AdaptivePoolConfig config_;
    BufferPoolMetrics metrics_;
    
    mutable std::shared_mutex chunks_mutex_;
    std::vector<std::unique_ptr<BufferPoolChunk>> chunks_;
    
    std::atomic<bool> running_;
    std::thread adaptation_thread_;
};

} // namespace francodb


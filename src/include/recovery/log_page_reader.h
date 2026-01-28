#pragma once

#include "storage/storage_interface.h"
#include "common/config.h"
#include "common/logger.h"
#include <string>
#include <fstream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <list>
#include <cstring>

namespace chronosdb {

/**
 * LogPageReader - Buffer Pool Integrated Log Reading
 *
 * Treats the log file as a series of pages managed by a dedicated
 * cache that shares memory budget with the main buffer pool.
 *
 * Features:
 * - Page-based caching for log reads (reuses hot log pages)
 * - LRU eviction when cache is full
 * - Integrates with overall memory budget
 * - Sequential read optimization with prefetching
 */
class LogPageReader {
public:
    // Use same page size as data pages for consistency
    static constexpr size_t LOG_PAGE_SIZE = PAGE_SIZE;

    /**
     * Construct with shared memory budget.
     *
     * @param max_cached_pages Maximum log pages to cache (part of total memory budget)
     */
    explicit LogPageReader(size_t max_cached_pages = 64)
        : max_cached_pages_(max_cached_pages), current_file_size_(0) {}

    ~LogPageReader() { Close(); }

    /**
     * Open a log file for reading.
     */
    bool Open(const std::string& path) {
        Close();
        log_path_ = path;
        log_file_.open(path, std::ios::binary);
        if (!log_file_.is_open()) {
            LOG_DEBUG("LogPageReader", "Could not open log file: %s", path.c_str());
            return false;
        }

        // Get file size
        log_file_.seekg(0, std::ios::end);
        current_file_size_ = static_cast<size_t>(log_file_.tellg());
        log_file_.seekg(0, std::ios::beg);

        LOG_DEBUG("LogPageReader", "Opened log file: %s (%zu bytes)",
                  path.c_str(), current_file_size_);
        return true;
    }

    void Close() {
        if (log_file_.is_open()) {
            log_file_.close();
        }
        page_cache_.clear();
        lru_list_.clear();
        current_file_size_ = 0;
    }

    bool IsOpen() const { return log_file_.is_open(); }
    size_t GetFileSize() const { return current_file_size_; }

    /**
     * Read data at a specific offset through the page cache.
     *
     * @param offset Byte offset in log file
     * @param buffer Output buffer
     * @param length Number of bytes to read
     * @return Number of bytes actually read
     */
    size_t Read(size_t offset, char* buffer, size_t length) {
        if (!log_file_.is_open() || offset >= current_file_size_) {
            return 0;
        }

        size_t bytes_read = 0;
        while (length > 0 && offset < current_file_size_) {
            size_t page_num = offset / LOG_PAGE_SIZE;
            size_t page_offset = offset % LOG_PAGE_SIZE;
            size_t bytes_available = std::min(LOG_PAGE_SIZE - page_offset,
                                              current_file_size_ - offset);
            size_t to_copy = std::min(length, bytes_available);

            // Get page from cache or load it
            const char* page_data = GetPage(page_num);
            if (!page_data) {
                break; // Read error
            }

            std::memcpy(buffer + bytes_read, page_data + page_offset, to_copy);
            bytes_read += to_copy;
            offset += to_copy;
            length -= to_copy;
        }

        return bytes_read;
    }

    /**
     * Sequential read with automatic position tracking.
     */
    size_t ReadSequential(char* buffer, size_t length) {
        size_t read = Read(read_position_, buffer, length);
        read_position_ += read;
        return read;
    }

    void Seek(size_t position) { read_position_ = position; }
    size_t Tell() const { return read_position_; }
    bool Eof() const { return read_position_ >= current_file_size_; }

    /**
     * Get cache statistics.
     */
    size_t GetCacheHits() const { return cache_hits_; }
    size_t GetCacheMisses() const { return cache_misses_; }
    double GetHitRate() const {
        size_t total = cache_hits_ + cache_misses_;
        return total > 0 ? (100.0 * cache_hits_ / total) : 0.0;
    }

private:
    struct CachedPage {
        std::vector<char> data;
        size_t page_num;
    };

    const char* GetPage(size_t page_num) {
        // Check cache first
        auto it = page_cache_.find(page_num);
        if (it != page_cache_.end()) {
            cache_hits_++;
            // Move to front of LRU
            TouchLRU(page_num);
            return it->second.data.data();
        }

        cache_misses_++;

        // Need to load from disk
        if (page_cache_.size() >= max_cached_pages_) {
            EvictLRU();
        }

        // Load page
        CachedPage page;
        page.page_num = page_num;
        page.data.resize(LOG_PAGE_SIZE);

        size_t file_offset = page_num * LOG_PAGE_SIZE;
        log_file_.seekg(static_cast<std::streamoff>(file_offset));
        if (!log_file_) {
            log_file_.clear();
            return nullptr;
        }

        size_t to_read = std::min(LOG_PAGE_SIZE, current_file_size_ - file_offset);
        log_file_.read(page.data.data(), static_cast<std::streamsize>(to_read));
        if (log_file_.gcount() == 0) {
            return nullptr;
        }

        // Add to cache
        page_cache_[page_num] = std::move(page);
        lru_list_.push_front(page_num);

        return page_cache_[page_num].data.data();
    }

    void TouchLRU(size_t page_num) {
        lru_list_.remove(page_num);
        lru_list_.push_front(page_num);
    }

    void EvictLRU() {
        if (lru_list_.empty()) return;
        size_t victim = lru_list_.back();
        lru_list_.pop_back();
        page_cache_.erase(victim);
    }

    std::string log_path_;
    std::ifstream log_file_;
    size_t max_cached_pages_;
    size_t current_file_size_;
    size_t read_position_ = 0;

    std::unordered_map<size_t, CachedPage> page_cache_;
    std::list<size_t> lru_list_;

    size_t cache_hits_ = 0;
    size_t cache_misses_ = 0;
};

} // namespace chronosdb

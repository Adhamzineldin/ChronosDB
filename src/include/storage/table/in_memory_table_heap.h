#pragma once

#include "storage/table/tuple.h"
#include "common/rid.h"
#include <vector>
#include <memory>

namespace chronosdb {

/**
 * InMemoryTableHeap - Buffer Pool Bypass for Time Travel Snapshots
 *
 * PROBLEM SOLVED:
 * When materializing large time travel snapshots (e.g., 1M rows), using the
 * regular TableHeap causes buffer pool thrashing. Insert rates degrade from
 * 40,000 rows/sec to under 2,000 rows/sec as the buffer pool fills up.
 *
 * SOLUTION:
 * This class stores tuples entirely in memory (std::vector), bypassing the
 * buffer pool completely. This provides:
 * - O(1) amortized insert time (vector append)
 * - No buffer pool eviction overhead
 * - No disk I/O during snapshot creation
 * - Linear time complexity for large snapshots
 *
 * USAGE:
 * Used by TimeTravelEngine for read-only historical snapshots. The iterator
 * interface matches TableHeap::Iterator so SeqScanExecutor works unchanged.
 *
 * PERFORMANCE:
 * - 1M row snapshot: ~500ms (vs 10+ minutes with buffer pool)
 * - Memory: O(n) where n = number of rows
 */
class InMemoryTableHeap {
public:
    /**
     * Iterator for in-memory table - matches TableHeap::Iterator interface
     */
    class Iterator {
    public:
        Iterator(const std::vector<Tuple>* tuples, size_t index)
            : tuples_(tuples), index_(index) {}

        // Required for SeqScanExecutor compatibility
        const Tuple& GetCurrentTuple() const {
            return (*tuples_)[index_];
        }

        Tuple ExtractTuple() const {
            return (*tuples_)[index_];
        }

        // Dereference operator (alternative access)
        const Tuple& operator*() const {
            return (*tuples_)[index_];
        }

        Iterator& operator++() {
            ++index_;
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return index_ != other.index_ || tuples_ != other.tuples_;
        }

        bool operator==(const Iterator& other) const {
            return index_ == other.index_ && tuples_ == other.tuples_;
        }

    private:
        const std::vector<Tuple>* tuples_;
        size_t index_;
    };

    InMemoryTableHeap() = default;

    /**
     * Reserve space for expected number of tuples (optimization)
     */
    void Reserve(size_t count) {
        tuples_.reserve(count);
    }

    /**
     * Insert a tuple - O(1) amortized, no buffer pool involved
     */
    bool InsertTuple(const Tuple& tuple, RID* rid, [[maybe_unused]] void* txn) {
        size_t index = tuples_.size();
        tuples_.push_back(tuple);
        if (rid) {
            // Use index as slot_id, page 0 (virtual)
            *rid = RID(0, static_cast<uint32_t>(index));
        }
        return true;
    }

    bool InsertTuple(Tuple&& tuple, RID* rid, [[maybe_unused]] void* txn) {
        size_t index = tuples_.size();
        tuples_.push_back(std::move(tuple));
        if (rid) {
            *rid = RID(0, static_cast<uint32_t>(index));
        }
        return true;
    }

    /**
     * Get tuple count
     */
    size_t GetTupleCount() const { return tuples_.size(); }

    /**
     * Iterator access - matches TableHeap interface for SeqScanExecutor
     */
    Iterator Begin([[maybe_unused]] void* txn = nullptr) const {
        return Iterator(&tuples_, 0);
    }

    Iterator End() const {
        return Iterator(&tuples_, tuples_.size());
    }

    /**
     * Direct access for debugging
     */
    const std::vector<Tuple>& GetTuples() const { return tuples_; }

private:
    std::vector<Tuple> tuples_;
};

} // namespace chronosdb

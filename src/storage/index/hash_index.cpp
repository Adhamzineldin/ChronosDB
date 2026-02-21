#include "storage/index/hash_index.h"
#include "storage/page/page.h"
#include <cstring>
#include <iostream>

namespace chronosdb {

// ============================================================================
// Constructor: Create a new hash index with fresh directory and bucket pages
// ============================================================================

HashIndex::HashIndex(const std::string& name, IBufferManager* bpm, uint32_t num_buckets)
    : name_(name), bpm_(bpm), directory_page_id_(INVALID_PAGE_ID), num_buckets_(num_buckets) {

    // Allocate the directory page (stores page_id_t for each bucket)
    page_id_t dir_page_id;
    Page* dir_page = bpm_->NewPage(&dir_page_id);
    if (!dir_page) {
        std::cerr << "[HASH INDEX] Failed to allocate directory page for " << name_ << std::endl;
        return;
    }
    directory_page_id_ = dir_page_id;

    // Initialize directory: allocate one bucket page per bucket
    char* dir_data = dir_page->GetData();
    std::memset(dir_data, 0, PAGE_SIZE);

    for (uint32_t i = 0; i < num_buckets_; i++) {
        page_id_t bucket_page_id;
        Page* bucket_page = bpm_->NewPage(&bucket_page_id);
        if (!bucket_page) {
            std::cerr << "[HASH INDEX] Failed to allocate bucket page " << i << std::endl;
            break;
        }

        // Initialize bucket page: count=0, overflow_page_id=INVALID_PAGE_ID, entries zeroed
        char* bucket_data = bucket_page->GetData();
        std::memset(bucket_data, 0, PAGE_SIZE);
        uint32_t count = 0;
        page_id_t overflow_id = INVALID_PAGE_ID;
        std::memcpy(bucket_data, &count, sizeof(uint32_t));
        std::memcpy(bucket_data + sizeof(uint32_t), &overflow_id, sizeof(page_id_t));

        bpm_->UnpinPage(bucket_page_id, true);

        // Store bucket page id in directory
        std::memcpy(dir_data + i * sizeof(page_id_t), &bucket_page_id, sizeof(page_id_t));
    }

    bpm_->UnpinPage(dir_page_id, true);
}

// ============================================================================
// Constructor: Reconstruct from existing directory page
// ============================================================================

HashIndex::HashIndex(const std::string& name, IBufferManager* bpm,
                     page_id_t directory_page_id, uint32_t num_buckets)
    : name_(name), bpm_(bpm), directory_page_id_(directory_page_id), num_buckets_(num_buckets) {
    // Directory and bucket pages already exist on disk - nothing to allocate
}

// ============================================================================
// Hash Function: extract int32 from key data, modulo num_buckets
// ============================================================================

uint32_t HashIndex::Hash(const GenericKey<8>& key) const {
    int32_t val = 0;
    std::memcpy(&val, key.data, sizeof(int32_t));
    // Use unsigned to avoid negative modulo issues
    uint32_t uval = static_cast<uint32_t>(val);
    return uval % num_buckets_;
}

// ============================================================================
// GetBucketPageId: read bucket's page_id from directory page
// ============================================================================

page_id_t HashIndex::GetBucketPageId(uint32_t bucket_idx) {
    if (bucket_idx >= num_buckets_) return INVALID_PAGE_ID;

    Page* dir_page = bpm_->FetchPage(directory_page_id_);
    if (!dir_page) return INVALID_PAGE_ID;

    page_id_t bucket_page_id;
    std::memcpy(&bucket_page_id, dir_page->GetData() + bucket_idx * sizeof(page_id_t), sizeof(page_id_t));

    bpm_->UnpinPage(directory_page_id_, false);
    return bucket_page_id;
}

// ============================================================================
// Insert: find bucket, scan for empty slot, create overflow if full
// ============================================================================

bool HashIndex::Insert(const GenericKey<8>& key, const RID& rid) {
    uint32_t bucket_idx = Hash(key);
    page_id_t bucket_page_id = GetBucketPageId(bucket_idx);
    if (bucket_page_id == INVALID_PAGE_ID) return false;

    // Walk the chain of bucket pages (including overflow pages)
    page_id_t current_page_id = bucket_page_id;

    while (current_page_id != INVALID_PAGE_ID) {
        Page* page = bpm_->FetchPage(current_page_id);
        if (!page) return false;

        char* data = page->GetData();

        // Read header
        uint32_t count;
        page_id_t overflow_id;
        std::memcpy(&count, data, sizeof(uint32_t));
        std::memcpy(&overflow_id, data + sizeof(uint32_t), sizeof(page_id_t));

        // Scan entries for an empty slot
        char* entries_start = data + BUCKET_HEADER_SIZE;
        for (uint32_t i = 0; i < ENTRIES_PER_BUCKET; i++) {
            BucketEntry entry;
            std::memcpy(&entry, entries_start + i * sizeof(BucketEntry), sizeof(BucketEntry));

            if (!entry.occupied) {
                // Found an empty slot - insert here
                entry.key = key;
                entry.rid = rid;
                entry.occupied = true;
                std::memcpy(entries_start + i * sizeof(BucketEntry), &entry, sizeof(BucketEntry));

                // Update count
                count++;
                std::memcpy(data, &count, sizeof(uint32_t));

                bpm_->UnpinPage(current_page_id, true);
                return true;
            }
        }

        // Bucket page is full - check for overflow
        if (overflow_id != INVALID_PAGE_ID) {
            bpm_->UnpinPage(current_page_id, false);
            current_page_id = overflow_id;
        } else {
            // Create a new overflow page
            page_id_t new_overflow_id;
            Page* overflow_page = bpm_->NewPage(&new_overflow_id);
            if (!overflow_page) {
                bpm_->UnpinPage(current_page_id, false);
                return false;
            }

            // Initialize the new overflow page
            char* overflow_data = overflow_page->GetData();
            std::memset(overflow_data, 0, PAGE_SIZE);
            uint32_t new_count = 0;
            page_id_t new_overflow_next = INVALID_PAGE_ID;
            std::memcpy(overflow_data, &new_count, sizeof(uint32_t));
            std::memcpy(overflow_data + sizeof(uint32_t), &new_overflow_next, sizeof(page_id_t));

            // Insert the entry into the first slot of the new overflow page
            BucketEntry new_entry;
            new_entry.key = key;
            new_entry.rid = rid;
            new_entry.occupied = true;
            std::memcpy(overflow_data + BUCKET_HEADER_SIZE, &new_entry, sizeof(BucketEntry));

            new_count = 1;
            std::memcpy(overflow_data, &new_count, sizeof(uint32_t));

            bpm_->UnpinPage(new_overflow_id, true);

            // Link overflow page from the current page
            std::memcpy(data + sizeof(uint32_t), &new_overflow_id, sizeof(page_id_t));
            bpm_->UnpinPage(current_page_id, true);
            return true;
        }
    }

    return false;
}

// ============================================================================
// GetValue: scan bucket + overflow chain for matching keys
// ============================================================================

std::vector<RID> HashIndex::GetValue(const GenericKey<8>& key) {
    std::vector<RID> result;

    uint32_t bucket_idx = Hash(key);
    page_id_t bucket_page_id = GetBucketPageId(bucket_idx);
    if (bucket_page_id == INVALID_PAGE_ID) return result;

    page_id_t current_page_id = bucket_page_id;

    while (current_page_id != INVALID_PAGE_ID) {
        Page* page = bpm_->FetchPage(current_page_id);
        if (!page) break;

        char* data = page->GetData();

        // Read header
        page_id_t overflow_id;
        std::memcpy(&overflow_id, data + sizeof(uint32_t), sizeof(page_id_t));

        // Scan entries
        char* entries_start = data + BUCKET_HEADER_SIZE;
        for (uint32_t i = 0; i < ENTRIES_PER_BUCKET; i++) {
            BucketEntry entry;
            std::memcpy(&entry, entries_start + i * sizeof(BucketEntry), sizeof(BucketEntry));

            if (entry.occupied && entry.key == key) {
                result.push_back(entry.rid);
            }
        }

        bpm_->UnpinPage(current_page_id, false);
        current_page_id = overflow_id;
    }

    return result;
}

// ============================================================================
// Remove: find and mark as unoccupied
// ============================================================================

bool HashIndex::Remove(const GenericKey<8>& key) {
    uint32_t bucket_idx = Hash(key);
    page_id_t bucket_page_id = GetBucketPageId(bucket_idx);
    if (bucket_page_id == INVALID_PAGE_ID) return false;

    bool removed = false;
    page_id_t current_page_id = bucket_page_id;

    while (current_page_id != INVALID_PAGE_ID) {
        Page* page = bpm_->FetchPage(current_page_id);
        if (!page) break;

        char* data = page->GetData();

        // Read header
        uint32_t count;
        page_id_t overflow_id;
        std::memcpy(&count, data, sizeof(uint32_t));
        std::memcpy(&overflow_id, data + sizeof(uint32_t), sizeof(page_id_t));

        // Scan entries
        char* entries_start = data + BUCKET_HEADER_SIZE;
        for (uint32_t i = 0; i < ENTRIES_PER_BUCKET; i++) {
            BucketEntry entry;
            std::memcpy(&entry, entries_start + i * sizeof(BucketEntry), sizeof(BucketEntry));

            if (entry.occupied && entry.key == key) {
                entry.occupied = false;
                std::memcpy(entries_start + i * sizeof(BucketEntry), &entry, sizeof(BucketEntry));

                if (count > 0) count--;
                std::memcpy(data, &count, sizeof(uint32_t));

                removed = true;
            }
        }

        bpm_->UnpinPage(current_page_id, removed);

        if (removed) return true;
        current_page_id = overflow_id;
    }

    return removed;
}

} // namespace chronosdb

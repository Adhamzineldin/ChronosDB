#pragma once

#include <string>
#include <vector>
#include <cstring>
#include "common/config.h"
#include "common/rid.h"
#include "storage/index/index_key.h"
#include "storage/storage_interface.h"

namespace chronosdb {

class HashIndex {
public:
    HashIndex(const std::string& name, IBufferManager* bpm, uint32_t num_buckets = 64);

    // Reconstruct from existing directory page
    HashIndex(const std::string& name, IBufferManager* bpm, page_id_t directory_page_id, uint32_t num_buckets);

    bool Insert(const GenericKey<8>& key, const RID& rid);
    bool Remove(const GenericKey<8>& key);
    std::vector<RID> GetValue(const GenericKey<8>& key);

    page_id_t GetDirectoryPageId() const { return directory_page_id_; }
    uint32_t GetNumBuckets() const { return num_buckets_; }

private:
    uint32_t Hash(const GenericKey<8>& key) const;
    page_id_t GetBucketPageId(uint32_t bucket_idx);

    std::string name_;
    IBufferManager* bpm_;
    page_id_t directory_page_id_;
    uint32_t num_buckets_;

    // Bucket page layout constants
    struct BucketEntry {
        GenericKey<8> key;
        RID rid;
        bool occupied = false;
    };

    static constexpr uint32_t BUCKET_HEADER_SIZE = sizeof(uint32_t) + sizeof(page_id_t); // count + overflow_page_id
    static constexpr uint32_t ENTRIES_PER_BUCKET = (PAGE_SIZE - BUCKET_HEADER_SIZE) / sizeof(BucketEntry);
};

} // namespace chronosdb

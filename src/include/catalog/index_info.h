#pragma once
#include <string>
#include <memory>
#include "storage/index/b_plus_tree.h"
#include "storage/index/hash_index.h"
#include "storage/table/schema.h"
#include "storage/index/index_key.h"
#include "common/config.h"
#include "common/rid.h"
#include "storage/storage_interface.h"

namespace chronosdb {

    enum class IndexType { BTREE, HASH };

    struct IndexInfo {
        // B+ Tree index constructor (backward compatible)
        IndexInfo(std::string name, std::string table_name, std::string col_name,
                  TypeId key_type, IBufferManager *bpm);

        // Hash index constructor
        IndexInfo(std::string name, std::string table_name, std::string col_name,
                  TypeId key_type, IBufferManager *bpm, IndexType type);

        std::string name_;
        std::string table_name_;
        std::string col_name_;
        IndexType index_type_ = IndexType::BTREE;

        // The B+Tree Instance (for BTREE type)
        std::unique_ptr<BPlusTree<GenericKey<8>, RID, GenericComparator<8>>> b_plus_tree_;

        // The Hash Index Instance (for HASH type)
        std::unique_ptr<HashIndex> hash_index_;
    };

} // namespace chronosdb

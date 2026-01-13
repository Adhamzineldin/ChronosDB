#pragma once
#include <string>
#include <memory>
#include "storage/index/b_plus_tree.h"
#include "storage/table/schema.h"
#include "storage/index/index_key.h" 

namespace francodb {

    struct IndexInfo {
        IndexInfo(std::string name, std::string table_name, std::string col_name, 
                  TypeId key_type, BufferPoolManager *bpm)
            : name_(std::move(name)), table_name_(std::move(table_name)), col_name_(std::move(col_name)) {
        
            // Initialize B+Tree with GenericKey<8> (Fits int64 and double)
            b_plus_tree_ = std::make_unique<BPlusTree<GenericKey<8>, RID, GenericComparator<8>>>(
                name_, bpm, GenericComparator<8>(key_type));
        }

        std::string name_;
        std::string table_name_;
        std::string col_name_;
    
        // The B+Tree Instance
        std::unique_ptr<BPlusTree<GenericKey<8>, RID, GenericComparator<8>>> b_plus_tree_;
    };

} // namespace francodb
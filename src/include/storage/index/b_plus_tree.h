#pragma once

#include <string>
#include <vector>
#include "concurrency/transaction.h"
#include "storage/index/index_iterator.h"
#include "storage/page/b_plus_tree_internal_page.h"
#include "storage/page/b_plus_tree_leaf_page.h"
#include "storage/storage_interface.h"  // For IBufferManager
#include "common/rwlatch.h" 

namespace francodb {

#define B_PLUS_TREE_TYPE BPlusTree<KeyType, ValueType, KeyComparator>

template <typename KeyType, typename ValueType, typename KeyComparator>
class BPlusTree {
public:
    // Accept IBufferManager for polymorphic buffer pool usage
    explicit BPlusTree(std::string name, IBufferManager *buffer_pool_manager, const KeyComparator &comparator,
                       int leaf_max_size = 0, int internal_max_size = 0);

    bool IsEmpty() const;

    // --- MAIN OPERATIONS ---
    bool Insert(const KeyType &key, const ValueType &value, Transaction *transaction = nullptr);

    // FIX: This now actually works
    void Remove(const KeyType &key, Transaction *transaction = nullptr);

    bool GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *transaction = nullptr);
    
    // --- PERSISTENCE ---
    void SetRootPageId(page_id_t root_page_id) { root_page_id_ = root_page_id; }
    page_id_t GetRootPageId() const { return root_page_id_; }

    void Print(IBufferManager *bpm);
    void Draw(IBufferManager *bpm, const std::string &outf);

private:
    void StartNewTree(const KeyType &key, const ValueType &value);

    // FIX: Optimized "Safe Mode" helpers
    bool InsertIntoLeafPessimistic(const KeyType &key, const ValueType &value, Transaction *txn);
    
    bool SplitInsert(BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *leaf, Page *leaf_page, const KeyType &key, const ValueType &value);
    
    bool InsertIntoParentRecursive(page_id_t parent_id, const KeyType &key, page_id_t left_child_id, page_id_t right_child_id);

    // Stub for crabbing logic (not used in Global Lock mode)
    enum class OpType { READ, INSERT, REMOVE };
    Page *FindLeafPage(const KeyType &key, bool leftMost = false, OpType op = OpType::READ, Transaction *txn = nullptr);

    bool InsertIntoLeafOptimistic(const KeyType &key, const ValueType &value, page_id_t root_id, Transaction *txn);

    bool InsertIntoLeaf(const KeyType &key, const ValueType &value, Transaction *txn, bool optimistic);

    void InsertIntoParent(BPlusTreePage *old_node, const KeyType &key, BPlusTreePage *new_node, Transaction *txn);

    template<class N>
    N *Split(N *node);

    std::string index_name_;
    page_id_t root_page_id_;
    IBufferManager *buffer_pool_manager_;
    KeyComparator comparator_;
    int leaf_max_size_;
    int internal_max_size_;
    
    ReaderWriterLatch root_latch_; 
};

} // namespace francodb
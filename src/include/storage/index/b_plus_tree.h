#pragma once

#include <queue>
#include <string>
#include <vector>

#include "concurrency/transaction.h"
#include "storage/index/index_iterator.h"
#include "storage/page/b_plus_tree_internal_page.h"
#include "storage/page/b_plus_tree_leaf_page.h"
#include "buffer/buffer_pool_manager.h"
#include "common/rwlatch.h" 
#include <unordered_map>

namespace francodb {

#define B_PLUS_TREE_TYPE BPlusTree<KeyType, ValueType, KeyComparator>

template <typename KeyType, typename ValueType, typename KeyComparator>
class BPlusTree {
    // FIX: Renamed DELETE to REMOVE to avoid Windows macro conflict
    enum class OpType { READ, INSERT, REMOVE };

public:
    explicit BPlusTree(std::string name, BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
                       int leaf_max_size = 0, int internal_max_size = 0);

    bool IsEmpty() const;

    // --- MAIN OPERATIONS ---
    bool Insert(const KeyType &key, const ValueType &value, Transaction *transaction = nullptr);

    void Remove(const KeyType &key, Transaction *transaction = nullptr);

    bool GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *transaction = nullptr);
    
    // --- PERSISTENCE ---
    void SetRootPageId(page_id_t root_page_id) { root_page_id_ = root_page_id; }
    page_id_t GetRootPageId() const { return root_page_id_; }

    // --- DEBUGGING ---
    void Print(BufferPoolManager *bpm);
    void Draw(BufferPoolManager *bpm, const std::string &outf);

private:
    // --- HELPER FUNCTIONS ---
    void StartNewTree(const KeyType &key, const ValueType &value);

    bool InsertIntoLeaf(const KeyType &key, const ValueType &value, Transaction *transaction = nullptr, bool optimistic = false);

    void InsertIntoParent(BPlusTreePage *old_node, const KeyType &key, BPlusTreePage *new_node, Transaction *transaction = nullptr);

    void InsertIntoParentHelper(BPlusTreePage *old_node, const KeyType &key,
                            BPlusTreePage *new_node,
                            std::unordered_map<page_id_t, Page*> &page_map);
    
    template <typename N>
    N *Split(N *node);

    // FIX: OpType matches the enum above
    Page *FindLeafPage(const KeyType &key, bool leftMost = false, OpType op = OpType::READ, Transaction *txn = nullptr);

    // --- MEMBER VARIABLES ---
    std::string index_name_;
    page_id_t root_page_id_;
    BufferPoolManager *buffer_pool_manager_;
    KeyComparator comparator_;
    int leaf_max_size_;
    int internal_max_size_;
    
    // Global Latch
    ReaderWriterLatch root_latch_; 
};

} // namespace francodb
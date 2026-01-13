#pragma once

#include <queue>
#include <string>
#include <vector>

#include "concurrency/transaction.h" // We will create a dummy for this later
#include "storage/index/index_iterator.h" // Placeholder for Phase 4
#include "storage/page/b_plus_tree_internal_page.h"
#include "storage/page/b_plus_tree_leaf_page.h"
#include "buffer/buffer_pool_manager.h"

namespace francodb {

#define B_PLUS_TREE_TYPE BPlusTree<KeyType, ValueType, KeyComparator>

/**
 * The Main B+ Tree Class.
 * * - KeyType: The type of key (int, string, etc.)
 * - ValueType: Usually RID (Record ID)
 * - KeyComparator: Function to compare two keys ( <, >, = )
 */
template <typename KeyType, typename ValueType, typename KeyComparator>
class BPlusTree {
public:
    explicit BPlusTree(std::string name, BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
                       int leaf_max_size = 0, int internal_max_size = 0);

    // Returns true if the tree is empty
    bool IsEmpty() const;

    // --- MAIN OPERATIONS ---

    // Returns true if the key/value was inserted
    bool Insert(const KeyType &key, const ValueType &value, Transaction *transaction = nullptr);

    // Returns true if the key was removed
    void Remove(const KeyType &key, Transaction *transaction = nullptr);

    // Search for a key and store the value in 'result'.
    // Returns true if found.
    bool GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *transaction = nullptr);

    // --- DEBUGGING ---
    // (Optional: Prints the tree structure to console)
    void Print(BufferPoolManager *bpm);
    void Draw(BufferPoolManager *bpm, const std::string &outf);

private:
    // --- HELPER FUNCTIONS ---
    
    // Traverses the tree from Root down to the specific Leaf that *should* contain the key.
    // This is used by Insert, Delete, and GetValue.
    Page *FindLeafPage(const KeyType &key, bool leftMost = false);

    // Start a new tree (creates the first root node)
    void StartNewTree(const KeyType &key, const ValueType &value);

    // Insert into a leaf that has space
    bool InsertIntoLeaf(const KeyType &key, const ValueType &value, Transaction *transaction = nullptr);

    // Handle split (Complex logic for when a page is full)
    template <typename N>
    N *Split(N *node);

    // Insert entry into parent (after a split)
    void InsertIntoParent(BPlusTreePage *old_node, const KeyType &key, BPlusTreePage *new_node, Transaction *transaction = nullptr);

    // --- MEMBER VARIABLES ---
    std::string index_name_;
    page_id_t root_page_id_;
    BufferPoolManager *buffer_pool_manager_;
    KeyComparator comparator_;
    int leaf_max_size_;
    int internal_max_size_;
};

} // namespace francodb
#include <string>
#include "common/exception.h"
#include "common/rid.h"
#include "storage/index/b_plus_tree.h"
#include "storage/index/index_key.h"

namespace francodb {
    
    // Constructor
    template<typename KeyType, typename ValueType, typename KeyComparator>
    BPlusTree<KeyType, ValueType, KeyComparator>::BPlusTree(
        std::string name, BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
        int leaf_max_size, int internal_max_size)
        : index_name_(std::move(name)),
          root_page_id_(INVALID_PAGE_ID),
          buffer_pool_manager_(buffer_pool_manager),
          comparator_(comparator),
          leaf_max_size_(leaf_max_size),
          internal_max_size_(internal_max_size) {
    }

    // IsEmpty
    template<typename KeyType, typename ValueType, typename KeyComparator>
    bool BPlusTree<KeyType, ValueType, KeyComparator>::IsEmpty() const {
        return root_page_id_ == INVALID_PAGE_ID;
    }

    /*****************************************************************************
     * SEARCH
     *****************************************************************************/

    template<typename KeyType, typename ValueType, typename KeyComparator>
    bool BPlusTree<KeyType, ValueType, KeyComparator>::GetValue(const KeyType &key,
                                                                std::vector<ValueType> *result,
                                                                Transaction *transaction) {
        (void) transaction;
        Page *page = FindLeafPage(key);
        if (page == nullptr) return false;

        auto *leaf = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(page->GetData());
        ValueType val;
        bool found = leaf->Lookup(key, val, comparator_);

        buffer_pool_manager_->UnpinPage(leaf->GetPageId(), false);
        if (found) {
            result->push_back(val);
            return true;
        }
        return false;
    }

    template<typename KeyType, typename ValueType, typename KeyComparator>
    Page *BPlusTree<KeyType, ValueType, KeyComparator>::FindLeafPage(const KeyType &key, bool leftMost) {
        (void) leftMost;
        if (IsEmpty()) return nullptr;

        Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
        if (page == nullptr) throw Exception(ExceptionType::OUT_OF_RANGE, "Out of memory");

        auto *node = reinterpret_cast<BPlusTreePage *>(page->GetData());

        while (!node->IsLeafPage()) {
            auto *internal = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(node);
            page_id_t child_page_id = internal->Lookup(key, comparator_);

            buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
            page = buffer_pool_manager_->FetchPage(child_page_id);
            if (page == nullptr) throw Exception(ExceptionType::OUT_OF_RANGE, "Out of memory");
            
            node = reinterpret_cast<BPlusTreePage *>(page->GetData());
        }
        return page;
    }

    /*****************************************************************************
     * INSERTION
     *****************************************************************************/

    template<typename KeyType, typename ValueType, typename KeyComparator>
    bool BPlusTree<KeyType, ValueType, KeyComparator>::Insert(const KeyType &key, const ValueType &value,
                                                              Transaction *transaction) {
        if (IsEmpty()) {
            StartNewTree(key, value);
            return true;
        }
        return InsertIntoLeaf(key, value, transaction);
    }

    template<typename KeyType, typename ValueType, typename KeyComparator>
    void BPlusTree<KeyType, ValueType, KeyComparator>::StartNewTree(const KeyType &key, const ValueType &value) {
        page_id_t new_page_id;
        Page *page = buffer_pool_manager_->NewPage(&new_page_id);
        if (page == nullptr) throw Exception(ExceptionType::OUT_OF_RANGE, "Out of memory");

        auto *root = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(page->GetData());
        root->Init(new_page_id, INVALID_PAGE_ID, leaf_max_size_);
        root_page_id_ = new_page_id;

        root->SetKeyAt(0, key);
        root->SetValueAt(0, value);
        root->SetSize(1);
        buffer_pool_manager_->UnpinPage(new_page_id, true);
    }

    // --- FIX: Logic to Find Correct Insertion Spot ---
    template <typename KeyType, typename ValueType, typename KeyComparator>
    int GetInsertionIndex(BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *leaf, const KeyType &key, const KeyComparator &comp) {
        int size = leaf->GetSize();
        int index = 0;
        // Find first key > new_key
        for (int i = 0; i < size; i++) {
            if (comp(key, leaf->KeyAt(i)) < 0) { // key < current
                index = i;
                break;
            }
            index++;
        }
        return index;
    }

    template <typename KeyType, typename ValueType, typename KeyComparator>
    bool BPlusTree<KeyType, ValueType, KeyComparator>::InsertIntoLeaf(const KeyType &key, const ValueType &value,
                                                                      Transaction *transaction) {
        (void)transaction;

        Page *page = FindLeafPage(key);
        auto *leaf = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(page->GetData());

        ValueType v;
        if (leaf->Lookup(key, v, comparator_)) {
            buffer_pool_manager_->UnpinPage(leaf->GetPageId(), false);
            return false; // Duplicate
        }

        int size = leaf->GetSize();
        int max_size = leaf->GetMaxSize();

        if (size < max_size) {
            // FIX: Use Helper to find correct sorted index
            int index = GetInsertionIndex(leaf, key, comparator_);
            
            for (int i = size; i > index; i--) {
                leaf->SetKeyAt(i, leaf->KeyAt(i - 1));
                leaf->SetValueAt(i, leaf->ValueAt(i - 1));
            }
            leaf->SetKeyAt(index, key);
            leaf->SetValueAt(index, value);
            leaf->SetSize(size + 1);
            
            buffer_pool_manager_->UnpinPage(leaf->GetPageId(), true);
            return true;
        }

        // Split Logic
        auto *new_leaf = Split(leaf);
        
        // FIX: Compare properly (< 0 check)
        // If key < new_leaf->KeyAt(0), it belongs to the OLD (left) leaf
        if (comparator_(key, new_leaf->KeyAt(0)) < 0) {
            int index = GetInsertionIndex(leaf, key, comparator_);
            for (int i = leaf->GetSize(); i > index; i--) {
                leaf->SetKeyAt(i, leaf->KeyAt(i - 1));
                leaf->SetValueAt(i, leaf->ValueAt(i - 1));
            }
            leaf->SetKeyAt(index, key);
            leaf->SetValueAt(index, value);
            leaf->SetSize(leaf->GetSize() + 1);
        } else {
            // Insert into NEW (right) leaf
            int index = GetInsertionIndex(new_leaf, key, comparator_);
            for (int i = new_leaf->GetSize(); i > index; i--) {
                new_leaf->SetKeyAt(i, new_leaf->KeyAt(i - 1));
                new_leaf->SetValueAt(i, new_leaf->ValueAt(i - 1));
            }
            new_leaf->SetKeyAt(index, key);
            new_leaf->SetValueAt(index, value);
            new_leaf->SetSize(new_leaf->GetSize() + 1);
        }

        InsertIntoParent(leaf, new_leaf->KeyAt(0), new_leaf);

        buffer_pool_manager_->UnpinPage(leaf->GetPageId(), true);
        buffer_pool_manager_->UnpinPage(new_leaf->GetPageId(), true);
        return true;
    }

    template<typename KeyType, typename ValueType, typename KeyComparator>
    template<typename N>
    N *BPlusTree<KeyType, ValueType, KeyComparator>::Split(N *node) {
        page_id_t new_page_id;
        Page *new_page = buffer_pool_manager_->NewPage(&new_page_id);
        if (new_page == nullptr) throw Exception(ExceptionType::OUT_OF_RANGE, "Out of memory");

        auto *new_node = reinterpret_cast<N *>(new_page->GetData());
        new_node->Init(new_page_id, node->GetParentPageId(), node->GetMaxSize());

        int total_size = node->GetSize();
        int split_index = (total_size + 1) / 2;
        int move_count = total_size - split_index;

        for (int i = 0; i < move_count; i++) {
            new_node->SetKeyAt(i, node->KeyAt(split_index + i));
            new_node->SetValueAt(i, node->ValueAt(split_index + i));
        }

        node->SetSize(split_index);
        new_node->SetSize(move_count);

        if (node->IsLeafPage()) {
            auto *leaf = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(node);
            auto *new_leaf = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(new_node);
            new_leaf->SetNextPageId(leaf->GetNextPageId());
            leaf->SetNextPageId(new_leaf->GetPageId());
        }
        return new_node;
    }

    template<typename KeyType, typename ValueType, typename KeyComparator>
    void BPlusTree<KeyType, ValueType, KeyComparator>::InsertIntoParent(BPlusTreePage *old_node, const KeyType &key,
                                                                        BPlusTreePage *new_node,
                                                                        Transaction *transaction) {
        (void) transaction;
        if (old_node->IsRootPage()) {
            page_id_t new_root_id;
            Page *page = buffer_pool_manager_->NewPage(&new_root_id);
            if (page == nullptr) throw Exception(ExceptionType::OUT_OF_RANGE, "Out of memory");

            auto *new_root = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(page->GetData());
            new_root->Init(new_root_id, INVALID_PAGE_ID, internal_max_size_);

            new_root->SetValueAt(0, old_node->GetPageId());
            new_root->SetKeyAt(1, key);
            new_root->SetValueAt(1, new_node->GetPageId());
            new_root->SetSize(2);

            old_node->SetParentPageId(new_root_id);
            new_node->SetParentPageId(new_root_id);

            root_page_id_ = new_root_id;
            buffer_pool_manager_->UnpinPage(new_root_id, true);
            return;
        }

        page_id_t parent_id = old_node->GetParentPageId();
        Page *page = buffer_pool_manager_->FetchPage(parent_id);
        if (page == nullptr) throw Exception(ExceptionType::OUT_OF_RANGE, "Out of memory");

        auto *parent = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(page->GetData());

        if (parent->GetSize() < parent->GetMaxSize()) {
            int size = parent->GetSize();
            int index = -1;
            for (int i = 0; i < size; i++) {
                if (parent->ValueAt(i) == old_node->GetPageId()) {
                    index = i;
                    break;
                }
            }

            for (int i = size; i > index + 1; i--) {
                parent->SetKeyAt(i, parent->KeyAt(i - 1));
                parent->SetValueAt(i, parent->ValueAt(i - 1));
            }

            parent->SetKeyAt(index + 1, key);
            parent->SetValueAt(index + 1, new_node->GetPageId());
            parent->SetSize(size + 1);

            new_node->SetParentPageId(parent_id);
            buffer_pool_manager_->UnpinPage(parent_id, true);
            return;
        }
        buffer_pool_manager_->UnpinPage(parent_id, false);
    }

    template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
} // namespace francodb
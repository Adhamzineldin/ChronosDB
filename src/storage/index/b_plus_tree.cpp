#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include "common/exception.h"
#include "common/rid.h"
#include "storage/index/b_plus_tree.h"
#include "storage/index/index_key.h"

namespace francodb {

    // --- CONSTRUCTOR ---
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

    // --- IS EMPTY ---
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
        Page *page = FindLeafPage(key, false, OpType::READ, transaction);
        if (page == nullptr) return false;

        auto *leaf = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(page->GetData());
        ValueType val;
        bool found = leaf->Lookup(key, val, comparator_);

        page->RUnlock();
        buffer_pool_manager_->UnpinPage(leaf->GetPageId(), false);

        if (found) {
            result->push_back(val);
            return true;
        }
        return false;
    }

    /*****************************************************************************
     * INSERTION
     *****************************************************************************/
    template<typename KeyType, typename ValueType, typename KeyComparator>
    bool BPlusTree<KeyType, ValueType, KeyComparator>::Insert(const KeyType &key, const ValueType &value,
                                                              Transaction *transaction) {
        // Optimistic Attempt
        if (InsertIntoLeaf(key, value, transaction, true)) return true;
        // Pessimistic Attempt (Global Lock)
        return InsertIntoLeaf(key, value, transaction, false);
    }

    // --- GENERIC INSERT HELPER ---
    template <typename N, typename K, typename V, typename C>
    void InsertGeneric(N *node, const K &key, const V &value, const C &cmp) {
        int size = node->GetSize();
        int index = 0;
        for (int i = 0; i < size; i++) {
            if (cmp(key, node->KeyAt(i)) < 0) {
                index = i; break;
            }
            index++;
        }
        for (int i = size; i > index; i--) {
            node->SetKeyAt(i, node->KeyAt(i - 1));
            node->SetValueAt(i, node->ValueAt(i - 1));
        }
        node->SetKeyAt(index, key);
        node->SetValueAt(index, value);
        node->SetSize(size + 1);
    }

    template <typename KeyType, typename ValueType, typename KeyComparator>
    bool BPlusTree<KeyType, ValueType, KeyComparator>::InsertIntoLeaf(const KeyType &key, const ValueType &value,
                                                                      Transaction *txn, bool optimistic) {
        if (optimistic) {
            Page *page = FindLeafPage(key, false, OpType::INSERT, txn);
            if (page == nullptr) return false;

            auto *leaf = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(page->GetData());
            
            ValueType v;
            if (leaf->Lookup(key, v, comparator_)) {
                page->WUnlock();
                buffer_pool_manager_->UnpinPage(leaf->GetPageId(), false);
                return false;
            }

            if (leaf->GetSize() < leaf->GetMaxSize()) {
                InsertGeneric(leaf, key, value, comparator_);
                page->WUnlock();
                buffer_pool_manager_->UnpinPage(leaf->GetPageId(), true);
                return true;
            }
            
            page->WUnlock();
            buffer_pool_manager_->UnpinPage(leaf->GetPageId(), false);
            return false;
        } else {
            root_latch_.WLock();
            if (IsEmpty()) {
                StartNewTree(key, value);
                root_latch_.WUnlock();
                return true;
            }

            std::vector<Page*> transaction_pages;
            std::unordered_map<page_id_t, Page*> page_map;
            
            Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
            page->WLock();
            transaction_pages.push_back(page);
            page_map[root_page_id_] = page;

            auto *node = reinterpret_cast<BPlusTreePage *>(page->GetData());
            while (!node->IsLeafPage()) {
                auto *internal = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(node);
                page_id_t child_id = internal->Lookup(key, comparator_);
                Page *child = buffer_pool_manager_->FetchPage(child_id);
                child->WLock();
                transaction_pages.push_back(child);
                page_map[child_id] = child;
                page = child;
                node = reinterpret_cast<BPlusTreePage *>(page->GetData());
            }

            auto *leaf = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(page->GetData());
            
            if (leaf->GetSize() < leaf->GetMaxSize()) {
                InsertGeneric(leaf, key, value, comparator_);
            } else {
                // SPLIT LEAF
                page_id_t new_id;
                Page *new_page = buffer_pool_manager_->NewPage(&new_id);
                new_page->WLock();
                auto *new_leaf = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(new_page->GetData());
                new_leaf->Init(new_id, leaf->GetParentPageId(), leaf->GetMaxSize());

                // Copy ALL items to vector
                std::vector<std::pair<KeyType, ValueType>> items;
                for(int i=0; i<leaf->GetSize(); i++) items.push_back({leaf->KeyAt(i), leaf->ValueAt(i)});
                
                // Insert New Item
                auto it = std::lower_bound(items.begin(), items.end(), key, 
                    [&](const auto& pair, const auto& k) { return comparator_(pair.first, k) < 0; });
                items.insert(it, {key, value});

                // Write back split
                int total = items.size();
                int split_idx = total / 2;
                
                leaf->SetSize(split_idx);
                for(int i=0; i<split_idx; i++) {
                    leaf->SetKeyAt(i, items[i].first);
                    leaf->SetValueAt(i, items[i].second);
                }
                
                new_leaf->SetSize(total - split_idx);
                for(int i=0; i<(total-split_idx); i++) {
                    new_leaf->SetKeyAt(i, items[split_idx+i].first);
                    new_leaf->SetValueAt(i, items[split_idx+i].second);
                }

                // Link
                new_leaf->SetNextPageId(leaf->GetNextPageId());
                leaf->SetNextPageId(new_id);

                // Push Up - pass the page_map for looking up already-locked pages
                InsertIntoParentHelper(leaf, new_leaf->KeyAt(0), new_leaf, page_map);
                
                new_page->WUnlock();
                buffer_pool_manager_->UnpinPage(new_id, true);
            }

            // Cleanup
            for (auto *p : transaction_pages) {
                p->WUnlock();
                buffer_pool_manager_->UnpinPage(p->GetPageId(), true);
            }
            root_latch_.WUnlock();
            return true;
        }
    }

    template<typename KeyType, typename ValueType, typename KeyComparator>
    void BPlusTree<KeyType, ValueType, KeyComparator>::InsertIntoParentHelper(
        BPlusTreePage *old_node, const KeyType &key, BPlusTreePage *new_node,
        std::unordered_map<page_id_t, Page*> &page_map) {
        
        if (old_node->IsRootPage()) {
            page_id_t new_root_id;
            Page *page = buffer_pool_manager_->NewPage(&new_root_id);
            page->WLock();
            auto *new_root = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(page->GetData());
            new_root->Init(new_root_id, INVALID_PAGE_ID, internal_max_size_);
            new_root->SetValueAt(0, old_node->GetPageId());
            new_root->SetKeyAt(1, key);
            new_root->SetValueAt(1, new_node->GetPageId());
            new_root->SetSize(2);
            old_node->SetParentPageId(new_root_id);
            new_node->SetParentPageId(new_root_id);
            root_page_id_ = new_root_id;
            page->WUnlock();
            buffer_pool_manager_->UnpinPage(new_root_id, true);
            return;
        }

        page_id_t parent_id = old_node->GetParentPageId();
        
        // Use already-locked parent from page_map
        Page *page = page_map[parent_id];
        auto *parent = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(page->GetData());

        if (parent->GetSize() < parent->GetMaxSize()) {
            InsertGeneric(parent, key, new_node->GetPageId(), comparator_);
            new_node->SetParentPageId(parent_id);
            return;
        }

        // --- INTERNAL NODE SPLIT ---
        page_id_t new_pid;
        Page *new_ppage = buffer_pool_manager_->NewPage(&new_pid);
        new_ppage->WLock();
        auto *new_parent = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(new_ppage->GetData());
        new_parent->Init(new_pid, parent->GetParentPageId(), parent->GetMaxSize());

        std::vector<std::pair<KeyType, page_id_t>> items;
        items.push_back({KeyType{}, parent->ValueAt(0)}); 
        
        for(int i=1; i<parent->GetSize(); i++) {
            items.push_back({parent->KeyAt(i), parent->ValueAt(i)});
        }

        auto it = std::lower_bound(items.begin() + 1, items.end(), key, 
             [&](const auto& pair, const auto& k) { return comparator_(pair.first, k) < 0; });
        items.insert(it, {key, new_node->GetPageId()});

        int total = items.size();
        int split_idx = total / 2;
        
        parent->SetSize(split_idx);
        for(int i=0; i<split_idx; i++) {
            if(i>0) parent->SetKeyAt(i, items[i].first);
            parent->SetValueAt(i, items[i].second);
        }

        KeyType up_key = items[split_idx].first;
        
        int new_count = total - split_idx;
        new_parent->SetSize(new_count);
        new_parent->SetValueAt(0, items[split_idx].second);
        
        for(int i=1; i<new_count; i++) {
            new_parent->SetKeyAt(i, items[split_idx + i].first);
            new_parent->SetValueAt(i, items[split_idx + i].second);
        }
        
        // Update parent IDs for children moved to new_parent
        for(int i=0; i<new_count; i++) {
            page_id_t child_id = new_parent->ValueAt(i);
            if (child_id == new_node->GetPageId()) {
                new_node->SetParentPageId(new_pid);
            } else if (page_map.find(child_id) != page_map.end()) {
                // Child is already locked in our transaction
                auto *cn = reinterpret_cast<BPlusTreePage *>(page_map[child_id]->GetData());
                cn->SetParentPageId(new_pid);
            } else {
                // Child not in our transaction path, safe to fetch
                Page *cp = buffer_pool_manager_->FetchPage(child_id);
                cp->WLock();
                auto *cn = reinterpret_cast<BPlusTreePage *>(cp->GetData());
                cn->SetParentPageId(new_pid);
                cp->WUnlock();
                buffer_pool_manager_->UnpinPage(child_id, true);
            }
        }

        // Add new parent to page_map before recursing
        page_map[new_pid] = new_ppage;

        // Recurse
        InsertIntoParentHelper(parent, up_key, new_parent, page_map);

        new_ppage->WUnlock();
        buffer_pool_manager_->UnpinPage(new_pid, true);
    }

    template<typename KeyType, typename ValueType, typename KeyComparator>
    void BPlusTree<KeyType, ValueType, KeyComparator>::InsertIntoParent(BPlusTreePage *old_node, const KeyType &key,
                                                                        BPlusTreePage *new_node, Transaction *txn) {
        (void)txn;
        std::unordered_map<page_id_t, Page*> empty_map;
        InsertIntoParentHelper(old_node, key, new_node, empty_map);
    }

    // --- HELPER: FindLeafPage ---
    template<typename KeyType, typename ValueType, typename KeyComparator>
    Page *BPlusTree<KeyType, ValueType, KeyComparator>::FindLeafPage(const KeyType &key, bool leftMost, OpType op, Transaction *txn) {
        (void)leftMost; (void)txn;
        
        root_latch_.RLock();
        if (IsEmpty()) {
            root_latch_.RUnlock();
            return nullptr;
        }

        Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
        page->RLock(); 
        root_latch_.RUnlock();

        auto *node = reinterpret_cast<BPlusTreePage *>(page->GetData());
        while (!node->IsLeafPage()) {
            auto *internal = reinterpret_cast<BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *>(node);
            page_id_t child_id = internal->Lookup(key, comparator_);
            Page *child = buffer_pool_manager_->FetchPage(child_id);
            child->RLock();
            page->RUnlock(); 
            buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
            
            page = child;
            node = reinterpret_cast<BPlusTreePage *>(page->GetData());
        }

        if (op == OpType::INSERT || op == OpType::REMOVE) {
            page->RUnlock();
            page->WLock();
        }
        return page;
    }

    // --- HELPER: StartNewTree ---
    template<typename KeyType, typename ValueType, typename KeyComparator>
    void BPlusTree<KeyType, ValueType, KeyComparator>::StartNewTree(const KeyType &key, const ValueType &value) {
        page_id_t new_page_id;
        Page *page = buffer_pool_manager_->NewPage(&new_page_id);
        auto *root = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(page->GetData());
        root->Init(new_page_id, INVALID_PAGE_ID, leaf_max_size_);
        root_page_id_ = new_page_id;
        root->SetKeyAt(0, key);
        root->SetValueAt(0, value);
        root->SetSize(1);
        buffer_pool_manager_->UnpinPage(new_page_id, true);
    }
    
    template<typename KeyType, typename ValueType, typename KeyComparator>
    template<typename N>
    N *BPlusTree<KeyType, ValueType, KeyComparator>::Split(N *node) { (void)node; return nullptr; }

    template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
} // namespace francodb
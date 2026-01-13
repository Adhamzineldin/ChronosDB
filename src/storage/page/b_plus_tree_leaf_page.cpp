#include <sstream>
#include "common/exception.h"
#include "common/rid.h"
#include "storage/page/b_plus_tree_leaf_page.h"

namespace francodb {
    /*****************************************************************************
     * HELPER METHODS AND UTILITIES
     *****************************************************************************/

    /**
     * Init: Initialize the page.
     * We set the Type, Parent, MaxSize, PageID, and ensure NextPage is Invalid.
     */
    template<typename KeyType, typename ValueType, typename KeyComparator>
    void B_PLUS_TREE_LEAF_PAGE_TYPE::Init(page_id_t page_id, page_id_t parent_id, int max_size) {
        SetPageType(IndexPageType::LEAF_PAGE);
        SetSize(0);
        SetMaxSize(max_size);
        SetParentPageId(parent_id);
        SetPageId(page_id);
        SetNextPageId(INVALID_PAGE_ID);
    }

    /**
     * GetNextPageId: Returns the Page ID of the *next* leaf in the chain.
     */
    template<typename KeyType, typename ValueType, typename KeyComparator>
    page_id_t B_PLUS_TREE_LEAF_PAGE_TYPE::GetNextPageId() const {
        return next_page_id_;
    }

    template<typename KeyType, typename ValueType, typename KeyComparator>
    void B_PLUS_TREE_LEAF_PAGE_TYPE::SetNextPageId(page_id_t next_page_id) {
        next_page_id_ = next_page_id;
    }

    /**
     * Helper methods for setting/getting keys and values
     */
    template<typename KeyType, typename ValueType, typename KeyComparator>
    KeyType B_PLUS_TREE_LEAF_PAGE_TYPE::KeyAt(int index) const {
        return array_[index].first;
    }

    template<typename KeyType, typename ValueType, typename KeyComparator>
    void B_PLUS_TREE_LEAF_PAGE_TYPE::SetKeyAt(int index, const KeyType &key) {
        array_[index].first = key;
    }

    template<typename KeyType, typename ValueType, typename KeyComparator>
    ValueType B_PLUS_TREE_LEAF_PAGE_TYPE::ValueAt(int index) const {
        return array_[index].second;
    }

    template<typename KeyType, typename ValueType, typename KeyComparator>
    void B_PLUS_TREE_LEAF_PAGE_TYPE::SetValueAt(int index, const ValueType &value) {
        array_[index].second = value;
    }

    template<typename KeyType, typename ValueType, typename KeyComparator>
    const std::pair<KeyType, ValueType> &B_PLUS_TREE_LEAF_PAGE_TYPE::GetItem(int index) {
        return array_[index];
    }

    /**
     * KeyIndex: Returns the index of a key using Binary Search.
     * If key does not exist, it returns the index where it *should* be inserted.
     */
    template<typename KeyType, typename ValueType, typename KeyComparator>
    int B_PLUS_TREE_LEAF_PAGE_TYPE::KeyIndex(const KeyType &key, const KeyComparator &comparator) const {
        int size = GetSize();
        if (size == 0) return 0;

        int left = 0;
        int right = size - 1;
        while (left <= right) {
            int mid = (left + right) / 2;
            KeyType mid_key = array_[mid].first;

            // Correct std::less usage:
            // 1. Is mid < key? -> Go Right
            if (comparator(mid_key, key)) {
                left = mid + 1;
            }
            // 2. Is key < mid? -> Go Left
            else if (comparator(key, mid_key)) {
                right = mid - 1;
            }
            // 3. They are equal
            else {
                return mid;
            }
        }
        return left;
    }

    /**
     * Lookup: Find the value for a given key.
     * Returns true if found, false if not.
     */
    template<typename KeyType, typename ValueType, typename KeyComparator>
    bool B_PLUS_TREE_LEAF_PAGE_TYPE::Lookup(const KeyType &key, ValueType &value,
                                            const KeyComparator &comparator) const {
        int index = KeyIndex(key, comparator);

        // Check if index is valid AND if keys are equal
        // If !(key < array[index]) AND !(array[index] < key), then they are equal.
        if (index < GetSize()) {
            KeyType existing_key = array_[index].first;
            bool key_is_smaller = comparator(key, existing_key);
            bool existing_is_smaller = comparator(existing_key, key);

            if (!key_is_smaller && !existing_is_smaller) {
                value = array_[index].second;
                return true;
            }
        }
        return false;
    }

    template class BPlusTreeLeafPage<int, int, std::less<int> >;
} // namespace francodb

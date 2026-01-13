#include <iostream>
#include <sstream>

#include "common/exception.h"
#include "storage/page/b_plus_tree_internal_page.h"

namespace francodb {
    /*****************************************************************************
     * HELPER METHODS AND UTILITIES
     *****************************************************************************/
    /*
     * Init: Reset memory, set type to INTERNAL, and set parent.
     */
    template<typename KeyType, typename ValueType, typename KeyComparator>
    void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Init(page_id_t page_id, page_id_t parent_id, int max_size) {
        SetPageType(IndexPageType::INTERNAL_PAGE);
        SetSize(0);
        SetMaxSize(max_size);
        SetParentPageId(parent_id);
        SetPageId(page_id);
    }

    /*
     * KeyAt / ValueAt: Simple array accessors
     */
    template<typename KeyType, typename ValueType, typename KeyComparator>
    KeyType B_PLUS_TREE_INTERNAL_PAGE_TYPE::KeyAt(int index) const {
        return array_[index].first;
    }

    template<typename KeyType, typename ValueType, typename KeyComparator>
    void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetKeyAt(int index, const KeyType &key) {
        array_[index].first = key;
    }

    template<typename KeyType, typename ValueType, typename KeyComparator>
    ValueType B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueAt(int index) const {
        return array_[index].second;
    }

    template<typename KeyType, typename ValueType, typename KeyComparator>
    void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetValueAt(int index, const ValueType &value) {
        array_[index].second = value;
    }

    /*
     * Lookup: The core of the B+ Tree navigation.
     * We want to find the LAST key that is <= the search key.
     * This tells us which child pointer to follow.
     */
    template<typename KeyType, typename ValueType, typename KeyComparator>
    ValueType B_PLUS_TREE_INTERNAL_PAGE_TYPE::Lookup(const KeyType &key, const KeyComparator &comparator) const {
        auto *array = array_;
        int size = GetSize();

        // Start at 1 (Internal pages usually skip the first key slot)
        int left = 1;
        int right = size - 1;

        while (left <= right) {
            int mid = (left + right) / 2;
            KeyType mid_key = array[mid].first;

            // If mid_key <= key, we want to look to the right to find the *largest* key <= target
            // So if mid_key <= key (meaning !(key < mid_key)), we move left up.

            if (comparator(key, mid_key)) {
                // key < mid_key -> Target is to the left
                right = mid - 1;
            } else {
                // key >= mid_key -> Target might be here or to the right
                left = mid + 1;
            }
        }

        // 'right' is the index of the largest key <= target
        return array[right].second;
    }

    template class BPlusTreeInternalPage<int, int, std::less<int> >;
} // namespace francodb

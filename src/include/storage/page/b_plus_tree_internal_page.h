#pragma once

#include <queue>
#include "storage/page/b_plus_tree_page.h"

namespace francodb {

    /**
     * Internal Page for B+ Tree.
     *
     * Store n keys and n+1 pointers (page IDs).
     * However, since we need to store them in an array, we treat it as N pairs.
     * The first Key is technically "invalid" or ignored, but we keep the structure consistent.
     *
     * Internal Page Format (Keys & Values):
     * ----------------------------------------------------------------------------------
     * | Header (20B) | Key(0) | PageID(0) | Key(1) | PageID(1) | ... | Key(N) | PageID(N) |
     * ----------------------------------------------------------------------------------
     */

#define B_PLUS_TREE_INTERNAL_PAGE_TYPE BPlusTreeInternalPage<KeyType, ValueType, KeyComparator>

    template <typename KeyType, typename ValueType, typename KeyComparator>
    class BPlusTreeInternalPage : public BPlusTreePage {
    public:
        
        using MappingType = std::pair<KeyType, ValueType>;
        
        // Initialize the page
        void Init(page_id_t page_id, page_id_t parent_id = INVALID_PAGE_ID, int max_size = 0);

        // Main Function: Find the child page ID that *might* contain our key
        ValueType Lookup(const KeyType &key, const KeyComparator &comparator) const;

        // Helper to set keys/values at a specific index
        void SetKeyAt(int index, const KeyType &key);
        KeyType KeyAt(int index) const;
        void SetValueAt(int index, const ValueType &value);
        ValueType ValueAt(int index) const;

    private:
        // MAPPING TYPE: A pair of <Key, PageID>
        // This is the Flexible Array Member (Size 1 is a placeholder).
        // In memory, this array actually extends as far as the page size allows.
        MappingType array_[1]; 
    };

} // namespace francodb
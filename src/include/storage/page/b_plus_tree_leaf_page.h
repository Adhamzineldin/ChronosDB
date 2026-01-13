#pragma once

#include <utility>
#include <vector>

#include "storage/page/b_plus_tree_page.h"
#include "common/rid.h"

namespace francodb {

#define B_PLUS_TREE_LEAF_PAGE_TYPE BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>

    /**
     * Leaf Page Format:
     * ----------------------------------------------------------------------
     * | Header (24B) | Key(0) | Value(0) | Key(1) | Value(1) | ... |
     * ----------------------------------------------------------------------
     * * Header fields (Inherited + New):
     * - PageType (4)
     * - LSN (4)
     * - CurrentSize (4)
     * - MaxSize (4)
     * - ParentPageId (4)
     * - PageId (4)
     * - NextPageId (4)  <-- NEW! Used for scanning (Linked List)
     */

    template <typename KeyType, typename ValueType, typename KeyComparator>
    class BPlusTreeLeafPage : public BPlusTreePage {
    public:
        // Define the pair type for easier usage
        using MappingType = std::pair<KeyType, ValueType>;

        // Init method
        void Init(page_id_t page_id, page_id_t parent_id = INVALID_PAGE_ID, int max_size = 0);

        // Get the next page in the linked list
        page_id_t GetNextPageId() const;
        void SetNextPageId(page_id_t next_page_id);

        // Look up a specific key
        // Returns true if found, sets "value" to the result
        bool Lookup(const KeyType &key, ValueType &value, const KeyComparator &comparator) const;

        // Getter/Setter for array access
        KeyType KeyAt(int index) const;
        ValueType ValueAt(int index) const;
        void SetKeyAt(int index, const KeyType &key);
        void SetValueAt(int index, const ValueType &value);
    
        // Returns the item at index
        const MappingType &GetItem(int index);

        // Returns the index of the key (or where it should be)
        int KeyIndex(const KeyType &key, const KeyComparator &comparator) const;

    private:
        page_id_t next_page_id_; // The extra 4 bytes for the linked list
        MappingType array_[1];   // Flexible array of <Key, Value> (usually <Key, RID>)
    };
    
    

} // namespace francodb
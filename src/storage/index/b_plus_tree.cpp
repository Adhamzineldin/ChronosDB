#include <string>
#include "common/exception.h"
#include "common/rid.h"
#include "storage/index/b_plus_tree.h"

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

    // Check if tree is empty
    template<typename KeyType, typename ValueType, typename KeyComparator>
    bool BPlusTree<KeyType, ValueType, KeyComparator>::IsEmpty() const {
        return root_page_id_ == INVALID_PAGE_ID;
    }

    /*****************************************************************************
     * SEARCH
     *****************************************************************************/

    /**
     * GetValue: The Public API
     * 1. Find the leaf page that *should* contain the key.
     * 2. Ask the leaf page: "Do you have this key?"
     * 3. Return the result.
     */
    template<typename KeyType, typename ValueType, typename KeyComparator>
    bool BPlusTree<KeyType, ValueType, KeyComparator>::GetValue(const KeyType &key,
                                                                std::vector<ValueType> *result,
                                                                Transaction *transaction) {
        (void)transaction;
        
        // 1. Traverse the tree to find the leaf
        Page *page = FindLeafPage(key);
        if (page == nullptr) {
            return false;
        }

        // 2. Cast the raw bytes to a Leaf Page
        // (We treat the char array as a structured class)
        auto *leaf = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(page->GetData());

        ValueType val;
        bool found = leaf->Lookup(key, val, comparator_);

        
        
       
        
        
        // 3. Unpin the page (We are done reading it)
        // is_dirty = false (We didn't change anything)
        buffer_pool_manager_->UnpinPage(leaf->GetPageId(), false);

        if (found) {
            result->push_back(val);
            return true;
        }
        return false;
    }

    /**
     * FindLeafPage: The Private Helper
     * Traverses from Root -> Internal -> Internal -> Leaf.
     */
    template<typename KeyType, typename ValueType, typename KeyComparator>
    Page *BPlusTree<KeyType, ValueType, KeyComparator>::FindLeafPage(const KeyType &key, bool leftMost) {
        
        (void)leftMost;
        
        // 1. If tree is empty, return nullptr
        if (IsEmpty()) {
            return nullptr;
        }

        // 2. Fetch the Root Page from Buffer Pool
        Page *page = buffer_pool_manager_->FetchPage(root_page_id_);
        if (page == nullptr) {
            throw Exception(ExceptionType::OUT_OF_RANGE, "Out of memory: Could not fetch root page.");
        }

        // 3. Cast to Generic BPlusTreePage to check the type
        auto *node = reinterpret_cast<BPlusTreePage *>(page->GetData());

        // 4. Drill down until we hit a Leaf
        while (!node->IsLeafPage()) {
            // It's an Internal Page. Cast it so we can use Lookup().
            auto *internal = reinterpret_cast<BPlusTreeInternalPage<KeyType, ValueType, KeyComparator> *>(node);

            // Find which child page to go to
            page_id_t child_page_id = internal->Lookup(key, comparator_);

            // Unpin the current page (Parent) because we are moving down
            buffer_pool_manager_->UnpinPage(page->GetPageId(), false);

            // Fetch the child page
            page = buffer_pool_manager_->FetchPage(child_page_id);
            if (page == nullptr) {
                throw Exception(ExceptionType::OUT_OF_RANGE, "Out of memory: Could not fetch child page.");
            }

            // Update 'node' pointer for the next loop iteration
            node = reinterpret_cast<BPlusTreePage *>(page->GetData());
        }

        // 5. We found the leaf! Return the raw Page object (Pinned).
        return page;
    }


    /*****************************************************************************
 * INSERTION
 *****************************************************************************/

    /**
     * Insert: The Public API
     * Returns true if inserted, false if duplicate exists.
     */
    template<typename KeyType, typename ValueType, typename KeyComparator>
    bool BPlusTree<KeyType, ValueType, KeyComparator>::Insert(const KeyType &key, const ValueType &value,
                                                              Transaction *transaction) {
        // 1. If tree is empty, create the first page
        if (IsEmpty()) {
            StartNewTree(key, value);
            return true;
        }

        // 2. Insert into the correct leaf
        return InsertIntoLeaf(key, value, transaction);
    }

    /**
     * StartNewTree: Called when root_page_id_ == INVALID
     */
    template<typename KeyType, typename ValueType, typename KeyComparator>
    void BPlusTree<KeyType, ValueType, KeyComparator>::StartNewTree(const KeyType &key, const ValueType &value) {
        // Ask Buffer Pool for a new page
        page_id_t new_page_id;
        Page *page = buffer_pool_manager_->NewPage(&new_page_id);

        if (page == nullptr) {
            throw Exception(ExceptionType::OUT_OF_RANGE, "Out of memory: Cannot create new root.");
        }

        // Cast raw bytes to LeafPage
        auto *root = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(page->GetData());

        // Initialize: Type=LEAF, Parent=INVALID, MaxSize=leaf_max_size_
        root->Init(new_page_id, INVALID_PAGE_ID, leaf_max_size_);

        // Update the Tree's internal state
        root_page_id_ = new_page_id;
        
        
        

        // Insert the first key/value pair directly (Position 0)
        root->SetKeyAt(0, key);
        root->SetValueAt(0, value);
        root->SetSize(1);

        // Unpin (Dirty = true, because we wrote to it)
        buffer_pool_manager_->UnpinPage(new_page_id, true);
    }

    /**
     * InsertIntoLeaf: 
     * Finds the correct leaf. If it has space, insert. If full, Split.
     */
    template<typename KeyType, typename ValueType, typename KeyComparator>
    bool BPlusTree<KeyType, ValueType, KeyComparator>::InsertIntoLeaf(const KeyType &key, const ValueType &value,
                                                                      Transaction *transaction) {
        
        (void)transaction;
        
        // 1. Find the leaf that should hold this key
        Page *page = FindLeafPage(key);
        auto *leaf = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(page->GetData());

        // 2. Check for duplicates
        ValueType v;
        if (leaf->Lookup(key, v, comparator_)) {
            // Key already exists! Unpin and return failure.
            buffer_pool_manager_->UnpinPage(leaf->GetPageId(), false);
            return false;
        }

        // 3. Check if we have space
        int size = leaf->GetSize();
        int max_size = leaf->GetMaxSize();

        if (size < max_size) {
            // --- CASE A: HAS SPACE ---
            // Just insert it in sorted order

            // Find insertion index
            int index = leaf->KeyIndex(key, comparator_);

            // Shift existing items to the right to make a hole
            // Example: [1, 5, 9] -> Insert 3 -> [1, _, 5, 9]
            for (int i = size; i > index; i--) {
                leaf->SetKeyAt(i, leaf->KeyAt(i - 1));
                leaf->SetValueAt(i, leaf->ValueAt(i - 1));
            }

            // Fill the hole
            leaf->SetKeyAt(index, key);
            leaf->SetValueAt(index, value);
            leaf->SetSize(size + 1);
            

            // Unpin (Dirty = true)
            buffer_pool_manager_->UnpinPage(leaf->GetPageId(), true);
            return true;
        }

        // --- CASE B: LEAF IS FULL ---
        // This is Chunk 3 (Splitting). 
        // For now, let's just return false or throw exception to indicate "Not Implemented Yet"
        // so we can test the "Happy Path" first.

        buffer_pool_manager_->UnpinPage(leaf->GetPageId(), false);
        return false; // Temporary: Fail if full
    }
    
    template class BPlusTree<int, int, std::less<int>>;
} // namespace francodb
    
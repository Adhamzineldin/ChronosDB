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
        (void) transaction;

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
        (void) leftMost;

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
    template <typename KeyType, typename ValueType, typename KeyComparator>
bool BPlusTree<KeyType, ValueType, KeyComparator>::InsertIntoLeaf(const KeyType &key, const ValueType &value,
                                                                  Transaction *transaction) {
    (void)transaction;

    Page *page = FindLeafPage(key);
    auto *leaf = reinterpret_cast<BPlusTreeLeafPage<KeyType, ValueType, KeyComparator> *>(page->GetData());

    ValueType v;
    if (leaf->Lookup(key, v, comparator_)) {
        buffer_pool_manager_->UnpinPage(leaf->GetPageId(), false);
        return false; // Duplicate found
    }

    int size = leaf->GetSize();
    int max_size = leaf->GetMaxSize();

    if (size < max_size) {
        // Case A: Simple Insert (Has Space)
        int index = leaf->KeyIndex(key, comparator_);
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

    // Case B: Split Needed
    auto *new_leaf = Split(leaf);
    
    // DECISION: Where does the new key go?
    // We compare 'key' with the first key of the new (right) node.
    
    // FIX IS HERE:
    // If key < new_leaf->KeyAt(0), it implies it belongs to the LEFT (Old) Node.
    if (comparator_(key, new_leaf->KeyAt(0))) {
        // --- INSERT INTO OLD (LEFT) NODE ---
        int index = leaf->KeyIndex(key, comparator_);
        for (int i = leaf->GetSize(); i > index; i--) {
            leaf->SetKeyAt(i, leaf->KeyAt(i - 1));
            leaf->SetValueAt(i, leaf->ValueAt(i - 1));
        }
        leaf->SetKeyAt(index, key);
        leaf->SetValueAt(index, value);
        leaf->SetSize(leaf->GetSize() + 1);
    } else {
        // --- INSERT INTO NEW (RIGHT) NODE ---
        int index = new_leaf->KeyIndex(key, comparator_);
        for (int i = new_leaf->GetSize(); i > index; i--) {
            new_leaf->SetKeyAt(i, new_leaf->KeyAt(i - 1));
            new_leaf->SetValueAt(i, new_leaf->ValueAt(i - 1));
        }
        new_leaf->SetKeyAt(index, key);
        new_leaf->SetValueAt(index, value);
        new_leaf->SetSize(new_leaf->GetSize() + 1);
    }

    // 3. Insert the split key into the parent
    // The separator is ALWAYS the first key of the new (right) node.
    InsertIntoParent(leaf, new_leaf->KeyAt(0), new_leaf);

    // 4. Cleanup
    buffer_pool_manager_->UnpinPage(leaf->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(new_leaf->GetPageId(), true);

    return true;
}


    template<typename KeyType, typename ValueType, typename KeyComparator>
    template<typename N>
    N *BPlusTree<KeyType, ValueType, KeyComparator>::Split(N *node) {
        page_id_t new_page_id;
        Page *new_page = buffer_pool_manager_->NewPage(&new_page_id);
        if (new_page == nullptr) {
            throw Exception(ExceptionType::OUT_OF_RANGE, "Out of memory: Cannot split.");
        }

        auto *new_node = reinterpret_cast<N *>(new_page->GetData());

        // Init new node with same parent
        new_node->Init(new_page_id, node->GetParentPageId(), node->GetMaxSize());

        // Move top half of data to new node
        int total_size = node->GetSize();
        int split_index = (total_size + 1) / 2; // Split point (Left gets more if odd)
        int move_count = total_size - split_index;

        // Copy data
        for (int i = 0; i < move_count; i++) {
            new_node->SetKeyAt(i, node->KeyAt(split_index + i));
            new_node->SetValueAt(i, node->ValueAt(split_index + i));
        }

        node->SetSize(split_index);
        new_node->SetSize(move_count);

        // Link Leaves if necessary
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
        (void) transaction; // Silence compiler warning

        // Case 1: We are splitting the ROOT
        if (old_node->IsRootPage()) {
            page_id_t new_root_id;
            Page *page = buffer_pool_manager_->NewPage(&new_root_id);
            if (page == nullptr) {
                throw Exception(ExceptionType::OUT_OF_RANGE, "Out of memory: Cannot allocate new root.");
            }

            auto *new_root = reinterpret_cast<BPlusTreeInternalPage<KeyType, ValueType, KeyComparator> *>(page->
                GetData());

            // Init New Root (Internal Node)
            new_root->Init(new_root_id, INVALID_PAGE_ID, internal_max_size_);

            // Point New Root to Old Node (Left) and New Node (Right)
            // Internal Nodes: Value[0] = Left Child, Key[1] = Separator, Value[1] = Right Child
            new_root->SetValueAt(0, old_node->GetPageId());
            new_root->SetKeyAt(1, key);
            new_root->SetValueAt(1, new_node->GetPageId());
            new_root->SetSize(2); // We have 2 children now

            // Update Children's Parent Pointers
            old_node->SetParentPageId(new_root_id);
            new_node->SetParentPageId(new_root_id);

            // Update Tree Root Info
            root_page_id_ = new_root_id;

            // Log for debugging
            std::cout << "[INFO] Root Split! New Root ID: " << new_root_id << std::endl;

            buffer_pool_manager_->UnpinPage(new_root_id, true);
            return;
        }

        // Case 2: Standard Parent Insert
        page_id_t parent_id = old_node->GetParentPageId();
        Page *page = buffer_pool_manager_->FetchPage(parent_id);
        if (page == nullptr) {
            throw Exception(ExceptionType::OUT_OF_RANGE, "Out of memory: Cannot fetch parent.");
        }

        auto *parent = reinterpret_cast<BPlusTreeInternalPage<KeyType, ValueType, KeyComparator> *>(page->GetData());

        // Check if Parent has space
        if (parent->GetSize() < parent->GetMaxSize()) {
            // We need to insert (key, new_node) into the parent.
            // Since internal nodes are sorted, we must find where 'key' belongs.

            // Optimization: Since we just split 'old_node', and 'new_node' is its right sibling,
            // the new key usually goes exactly after the pointer to 'old_node'.

            int size = parent->GetSize();
            // Find the index of the pointer to the old node
            int index = -1;
            for (int i = 0; i < size; i++) {
                if (parent->ValueAt(i) == old_node->GetPageId()) {
                    index = i;
                    break;
                }
            }

            // We insert at index + 1
            // Shift data to make room
            for (int i = size; i > index + 1; i--) {
                parent->SetKeyAt(i, parent->KeyAt(i - 1));
                parent->SetValueAt(i, parent->ValueAt(i - 1));
            }

            parent->SetKeyAt(index + 1, key);
            parent->SetValueAt(index + 1, new_node->GetPageId());
            parent->SetSize(size + 1);

            // New sibling needs to know its parent is this node
            new_node->SetParentPageId(parent_id);

            buffer_pool_manager_->UnpinPage(parent_id, true);
            return;
        }

        // Case 3: Parent is FULL (Recursive Split)
        // For this checkpoint, we will stop here.
        // Implementing recursive internal splits requires a refactor of Split() to handle internal nodes better.
        // But Case 1 & 2 are enough to grow the tree from Height 1 to Height 2.
        buffer_pool_manager_->UnpinPage(parent_id, false);
    }


    template class BPlusTree<int, int, std::less<int> >;
} // namespace francodb

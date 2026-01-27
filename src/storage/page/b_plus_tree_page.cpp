#include "storage/page/b_plus_tree_page.h"

namespace chronosdb {

    /*
     * Helper methods to get/set page type
     * Page type enum class is defined in b_plus_tree_page.h
     */
    bool BPlusTreePage::IsLeafPage() const { 
        return page_type_ == IndexPageType::LEAF_PAGE; 
    }

    bool BPlusTreePage::IsRootPage() const { 
        return parent_page_id_ == INVALID_PAGE_ID; 
    }

    void BPlusTreePage::SetPageType(IndexPageType page_type) { 
        page_type_ = page_type; 
    }

    IndexPageType BPlusTreePage::GetPageType() const { 
        return page_type_; 
    }

    /*
     * Helper methods to get/set size (number of key/value pairs stored in that page)
     */
    void BPlusTreePage::SetSize(int size) { 
        size_ = size; 
    }

    int BPlusTreePage::GetSize() const { 
        return size_; 
    }

    void BPlusTreePage::SetMaxSize(int max_size) { 
        max_size_ = max_size; 
    }

    int BPlusTreePage::GetMaxSize() const { 
        return max_size_; 
    }

    /*
     * Helper method to get min page size
     * Generally, min size is max_size / 2 (Half full property of B+ Trees)
     */
    int BPlusTreePage::GetMinSize() const {
        if (IsRootPage()) {
            return IsLeafPage() ? 1 : 2; // Root rules are special
        }
        return max_size_ / 2;
    }

    /*
     * Helper methods to get/set parent page id
     */
    page_id_t BPlusTreePage::GetParentPageId() const { 
        return parent_page_id_; 
    }

    void BPlusTreePage::SetParentPageId(page_id_t parent_page_id) { 
        parent_page_id_ = parent_page_id; 
    }
    
    
    void BPlusTreePage::SetPageId(page_id_t page_id) {
        page_id_ = page_id;
    }

    page_id_t BPlusTreePage::GetPageId() const {
        return page_id_;
    }

} // namespace chronosdb
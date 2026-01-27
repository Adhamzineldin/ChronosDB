#pragma once

#include <cassert>
#include <climits>
#include <cstdlib>
#include <string>
#include "storage/page/page.h"
#include "common/config.h"

namespace chronosdb {

#define B_PLUS_TREE_PAGE_TYPE BPlusTreePage

    enum class IndexPageType { INVALID_INDEX_PAGE = 0, LEAF_PAGE, INTERNAL_PAGE };

    /**
     * Both Internal and Leaf pages inherit from this.
     *
     * UPDATED HEADER FORMAT (28 Bytes):
     * ---------------------------------------------------------------------------------------------------------
     * | Checksum (4) | PageType (4) | LSN (4) | CurrentSize (4) | MaxSize (4) | ParentPageId (4) | PageId (4) |
     * ---------------------------------------------------------------------------------------------------------
     */
    class BPlusTreePage {
    public:
        bool IsLeafPage() const;
        bool IsRootPage() const;
        void SetPageType(IndexPageType page_type);
        IndexPageType GetPageType() const;

        void SetSize(int size);
        int GetSize() const;
        void SetMaxSize(int max_size);
        int GetMaxSize() const;
        int GetMinSize() const;

        void SetParentPageId(page_id_t parent_page_id);
        page_id_t GetParentPageId() const;

        // --- NEW METHODS ---
        void SetPageId(page_id_t page_id);
        page_id_t GetPageId() const;

    private:
        uint32_t checksum_;      // Bytes 0-3
        IndexPageType page_type_; // Bytes 4-7
        int lsn_;                // Bytes 8-11
        int size_;               // Bytes 12-15
        int max_size_;           // Bytes 16-19
        page_id_t parent_page_id_; // Bytes 20-23
        page_id_t page_id_;      // Bytes 24-27
    };

} // namespace chronosdb
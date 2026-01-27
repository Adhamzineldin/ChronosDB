#pragma once

#include <vector>
#include <cstdint>
#include "common/config.h"
#include "storage/page/page.h"

namespace chronosdb {

    /**
     * Manages the allocation and deallocation of pages on disk.
     * Uses a Bitmap stored on a dedicated page (Page 2).
     */
    class FreePageManager {
    public:
        static constexpr page_id_t BITMAP_PAGE_ID = 2;

        // Returns a page ID that is free to use. 
        // If no recycled pages exist, returns the next highest ID (appending).
        static page_id_t AllocatePage(char *bitmap_data, page_id_t current_file_size) {
            // Search the bitmap for a '0' bit
            for (uint32_t i = 0; i < PAGE_SIZE; ++i) {
                if (static_cast<uint8_t>(bitmap_data[i]) != 0xFF) { // If byte is not all 1s
                    for (int bit = 0; bit < 8; ++bit) {
                        if (!(bitmap_data[i] & (1 << bit))) {
                            page_id_t found_id = i * 8 + bit;
                            // Mark as used
                            bitmap_data[i] |= (1 << bit);
                            return found_id;
                        }
                    }
                }
            }
            // If we reach here, no recycled pages. Return the end of file.
            return current_file_size; 
        }

        // Mark a page as free (Recycle it)
        static void DeallocatePage(char *bitmap_data, page_id_t page_id) {
            uint32_t byte_idx = page_id / 8;
            uint32_t bit_idx = page_id % 8;
            bitmap_data[byte_idx] &= ~(1 << bit_idx);
        }
    };

} // namespace chronosdb
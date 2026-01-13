#include <iostream>
#include <cstdio>
#include <cassert>
#include <filesystem>
#include <vector>
#include <algorithm>

#include "storage/index/b_plus_tree.h"
#include "buffer/buffer_pool_manager.h"

using namespace francodb;

void TestSplitTree() {
    std::string filename = "test_tree_split.francodb";
    if (std::filesystem::exists(filename)) {
        std::filesystem::remove(filename);
    }

    std::cout << "[TEST] Starting Split B+ Tree Test..." << std::endl;

    auto *disk_manager = new DiskManager(filename);
    // Increased pool size to 20 to accommodate splits comfortably
    auto *bpm = new BufferPoolManager(20, disk_manager); 
    
    // Max Size = 5 (Small size forces splits quickly!)
    BPlusTree<int, int, std::less<int>> tree("test_index", bpm, std::less<int>(), 5, 5);

    // 1. Insert 15 keys (Should create 3 Leaf Pages and 1 Root Page)
    int n = 15;
    for (int i = 1; i <= n; i++) {
        tree.Insert(i, i * 100);
        // std::cout << "Inserted " << i << std::endl;
    }

    std::cout << "[STEP 1] Inserted " << n << " keys." << std::endl;

    // 2. Read them all back to ensure no data was lost during split
    std::vector<int> result;
    for (int i = 1; i <= n; i++) {
        result.clear();
        bool found = tree.GetValue(i, &result);
        if (!found || result[0] != i * 100) {
            std::cout << "[FAIL] Lost Key " << i << " Value " << (found ? std::to_string(result[0]) : "NOT FOUND") << std::endl;
            assert(false);
        }
    }
    std::cout << "[STEP 2] All keys found! Splitting logic works." << std::endl;

    // Cleanup
    delete bpm;
    delete disk_manager;
    // std::filesystem::remove(filename);

    std::cout << "[SUCCESS] B+ Tree Split Test Passed!" << std::endl;
}

int main() {
    TestSplitTree();
    return 0;
}
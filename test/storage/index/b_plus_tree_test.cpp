#include <iostream>
#include <cstdio>
#include <cassert>
#include <filesystem>
#include <vector>
#include <algorithm> // For std::less

#include "storage/index/b_plus_tree.h"
#include "buffer/buffer_pool_manager.h"

using namespace francodb;

void TestSinglePageTree() {
    std::string filename = "test_tree_single.francodb";
    if (std::filesystem::exists(filename)) {
        std::filesystem::remove(filename);
    }

    std::cout << "[TEST] Starting Single Page B+ Tree Test..." << std::endl;

    // 1. Setup Environment
    auto *disk_manager = new DiskManager(filename);
    // Pool size 5 is enough for a 1-page tree
    auto *bpm = new BufferPoolManager(5, disk_manager); 
    
    // 2. Initialize Tree
    // Key=int, Value=int, Comparator=std::less<int>
    // Max Leaf Size = 10 (Plenty of space for our test)
    BPlusTree<int, int, std::less<int>> tree("test_index", bpm, std::less<int>(), 10, 10);

    // 3. Verify Empty
    assert(tree.IsEmpty());
    std::cout << "[STEP 1] Tree is initially empty. (Passed)" << std::endl;

    // 4. Insert Data (1, 2, 3, 4, 5)
    // Transaction is nullptr for now
    tree.Insert(1, 100); // Key 1 -> Value 100
    tree.Insert(2, 200);
    tree.Insert(3, 300);
    tree.Insert(4, 400);
    tree.Insert(5, 500);

    assert(!tree.IsEmpty());
    std::cout << "[STEP 2] Inserted 5 keys successfully." << std::endl;

    // 5. Read Data (GetValue)
    std::vector<int> result;
    
    // Search for Key 1
    bool found = tree.GetValue(1, &result);
    assert(found == true);
    assert(result[0] == 100);
    std::cout << "  -> Found Key 1: Value " << result[0] << " (Correct)" << std::endl;

    // Search for Key 3
    result.clear();
    found = tree.GetValue(3, &result);
    assert(found == true);
    assert(result[0] == 300);
    std::cout << "  -> Found Key 3: Value " << result[0] << " (Correct)" << std::endl;

    // Search for Key 5
    result.clear();
    found = tree.GetValue(5, &result);
    assert(found == true);
    assert(result[0] == 500);
    std::cout << "  -> Found Key 5: Value " << result[0] << " (Correct)" << std::endl;

    // 6. Negative Test (Search for missing key)
    result.clear();
    found = tree.GetValue(99, &result);
    assert(found == false);
    assert(result.empty());
    std::cout << "[STEP 3] Search for missing Key 99 returned false. (Passed)" << std::endl;

    // Cleanup
    delete bpm;
    delete disk_manager;
    // std::filesystem::remove(filename);

    std::cout << "[SUCCESS] Single Page Tree works!" << std::endl;
}

int main() {
    TestSinglePageTree();
    return 0;
}
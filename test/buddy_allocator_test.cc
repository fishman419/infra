#include <iostream>
#include <vector>
#include <random>
#include <cassert>
#include "buddy_allocator.h"

using namespace infra;

// Test 1: Basic allocation
bool test_basic_allocation() {
    std::cout << "Test 1: Basic allocation..." << std::endl;

    BuddyAllocator allocator(4096, 16);

    if (allocator.totalSize() != 4096) {
        std::cerr << "Expected total size 4096, got " << allocator.totalSize() << std::endl;
        return false;
    }

    if (allocator.usedSize() != 0) {
        std::cerr << "Expected used size 0, got " << allocator.usedSize() << std::endl;
        return false;
    }

    // Allocate 16 bytes
    void* p1 = allocator.allocate(16);
    if (!p1) {
        std::cerr << "Failed to allocate 16 bytes" << std::endl;
        return false;
    }

    if (allocator.usedSize() != 16) {
        std::cerr << "Expected used size 16, got " << allocator.usedSize() << std::endl;
        return false;
    }

    std::cout << "Test 1 passed" << std::endl;
    return true;
}

// Test 2: Deallocation and merging
bool test_deallocation_merging() {
    std::cout << "Test 2: Deallocation and merging..." << std::endl;

    BuddyAllocator allocator(4096, 16);

    // Allocate two 16-byte blocks
    void* p1 = allocator.allocate(16);
    void* p2 = allocator.allocate(16);

    size_t used_after_alloc = allocator.usedSize();
    if (used_after_alloc != 32) {
        std::cerr << "Expected used size 32, got " << used_after_alloc << std::endl;
        return false;
    }

    // Deallocate both - they should merge back to a 32-byte block
    allocator.deallocate(p1);
    allocator.deallocate(p2);

    if (allocator.usedSize() != 0) {
        std::cerr << "Expected used size 0 after deallocation, got " << allocator.usedSize() << std::endl;
        return false;
    }

    // Should be able to allocate a 32-byte block now
    void* p3 = allocator.allocate(32);
    if (!p3) {
        std::cerr << "Failed to allocate 32 bytes after merging" << std::endl;
        return false;
    }

    std::cout << "Test 2 passed" << std::endl;
    return true;
}

// Test 3: Different allocation sizes
bool test_different_sizes() {
    std::cout << "Test 3: Different allocation sizes..." << std::endl;

    BuddyAllocator allocator(1024 * 1024, 16);  // 1MB

    void* p1 = allocator.allocate(16);
    void* p2 = allocator.allocate(100);
    void* p3 = allocator.allocate(500);
    void* p4 = allocator.allocate(2000);

    if (!p1 || !p2 || !p3 || !p4) {
        std::cerr << "Failed to allocate one of the blocks" << std::endl;
        return false;
    }

    // Touch memory
    memset(p1, 0xAA, 16);
    memset(p2, 0xBB, 100);
    memset(p3, 0xCC, 500);
    memset(p4, 0xDD, 2000);

    // Deallocate middle one
    allocator.deallocate(p2);

    // Should still be able to allocate another 100 bytes
    void* p5 = allocator.allocate(100);
    if (!p5) {
        std::cerr << "Failed to allocate after deallocating middle block" << std::endl;
        return false;
    }

    memset(p5, 0xEE, 100);

    std::cout << "Test 3 passed" << std::endl;
    return true;
}

// Test 4: Out of memory handling
bool test_out_of_memory() {
    std::cout << "Test 4: Out of memory handling..." << std::endl;

    BuddyAllocator allocator(4096, 16);

    // Allocate many small blocks
    std::vector<void*> ptrs;
    while (true) {
        void* p = allocator.allocate(16);
        if (!p) break;
        ptrs.push_back(p);
    }

    if (allocator.freeSize() > 256) {
        std::cerr << "Expected very little free memory when full, got " << allocator.freeSize() << std::endl;
        return false;
    }

    // Deallocate all
    for (void* p : ptrs) {
        allocator.deallocate(p);
    }

    if (allocator.usedSize() != 0) {
        std::cerr << "Expected used size 0 after full deallocation, got "
                  << allocator.usedSize() << std::endl;
        return false;
    }

    std::cout << "  After full deallocation, free block stats:" << std::endl;
    allocator.printStats();

    // Should allocate again full size
    void* p = allocator.allocate(4096);
    if (!p) {
        std::cerr << "Failed to allocate full size after full deallocation" << std::endl;
        return false;
    }

    std::cout << "Test 4 passed" << std::endl;
    return true;
}

// Test 5: Random allocation and deallocation
bool test_random_allocations() {
    std::cout << "Test 5: Random allocation and deallocation..." << std::endl;

    BuddyAllocator allocator(4 * 1024 * 1024, 16);  // 4MB
    std::vector<std::pair<void*, size_t>> allocations;

    std::mt19937 gen(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<> size_dis(16, 10000);

    const int iterations = 500;
    for (int i = 0; i < iterations; ++i) {
        if (allocations.size() > 100 && (gen() % 2) == 0) {
            // Deallocate a random one
            size_t idx = gen() % allocations.size();
            allocator.deallocate(allocations[idx].first);
            allocations.erase(allocations.begin() + idx);
        } else {
            size_t size = size_dis(gen);
            void* p = allocator.allocate(size);
            if (p) {
                memset(p, 0xAA, size);
                allocations.emplace_back(p, size);
            }
        }
    }

    // Everything should still work
    void* p = allocator.allocate(256);
    if (!p) {
        std::cerr << "Failed to allocate after random operations" << std::endl;
        return false;
    }
    memset(p, 0x55, 256);
    allocator.deallocate(p);

    std::cout << "Test 5 passed (" << allocations.size() << " active allocations)" << std::endl;
    return true;
}

// Test 6: Invalid pointer deallocate handling
bool test_invalid_deallocate() {
    std::cout << "Test 6: Invalid pointer deallocate handling..." << std::endl;

    BuddyAllocator allocator(4096, 16);

    void* p = allocator.allocate(16);
    if (!p) {
        std::cerr << "First allocation failed" << std::endl;
        return false;
    }

    // Deallocate outside range - should just ignore
    int x = 42;
    allocator.deallocate(&x);

    // Double deallocate - should handle gracefully (already handled by bit tracking)
    allocator.deallocate(p);
    allocator.deallocate(p);

    std::cout << "Test 6 passed" << std::endl;
    return true;
}

// Test 7: Free block counting
bool test_free_block_counting() {
    std::cout << "Test 7: Free block counting..." << std::endl;

    BuddyAllocator allocator(4096, 16);

    // 4096 / 16 = 256 blocks => order 8 (2^8 = 256)
    int blocks_order8 = allocator.freeBlocks(8);
    if (blocks_order8 != 1) {
        std::cerr << "Expected 1 free block at order 8, got " << blocks_order8 << std::endl;
        return false;
    }

    // Allocate one 16-byte (order 0)
    allocator.allocate(16);

    // After splitting, we should have at least 1 free at order 0
    int blocks_order0 = allocator.freeBlocks(0);
    if (blocks_order0 < 1) {
        std::cerr << "Expected at least 1 free block at order 0" << std::endl;
        return false;
    }

    std::cout << "Test 7 passed" << std::endl;

    // Print stats for visual inspection
    allocator.printStats();

    return true;
}

int main() {
    std::cout << "Running BuddyAllocator tests..." << std::endl << std::endl;

    bool all_passed = true;

    all_passed &= test_basic_allocation();
    std::cout << std::endl;
    all_passed &= test_deallocation_merging();
    std::cout << std::endl;
    all_passed &= test_different_sizes();
    std::cout << std::endl;
    all_passed &= test_out_of_memory();
    std::cout << std::endl;
    all_passed &= test_random_allocations();
    std::cout << std::endl;
    all_passed &= test_invalid_deallocate();
    std::cout << std::endl;
    all_passed &= test_free_block_counting();
    std::cout << std::endl;

    if (all_passed) {
        std::cout << "All BuddyAllocator tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some tests failed!" << std::endl;
        return 1;
    }
}

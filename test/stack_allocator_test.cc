#include <iostream>
#include <vector>
#include <thread>
#include <cassert>
#include "stack_allocator.h"

using namespace infra;

// Test 1: Basic allocation
bool test_basic_allocation() {
    std::cout << "Test 1: Basic allocation..." << std::endl;

    StackAllocatorST allocator(4096);

    if (allocator.capacity() != 4096) {
        std::cerr << "Expected capacity 4096, got " << allocator.capacity() << std::endl;
        return false;
    }

    if (allocator.used() != 0) {
        std::cerr << "Expected used 0, got " << allocator.used() << std::endl;
        return false;
    }

    // Allocate int
    int* p1 = static_cast<int*>(allocator.allocate(sizeof(int)));
    if (!p1) {
        std::cerr << "Failed to allocate int" << std::endl;
        return false;
    }
    *p1 = 42;

    if (*p1 != 42) {
        std::cerr << "Value corrupted" << std::endl;
        return false;
    }

    if (allocator.used() > 0 && allocator.available() < 4096) {
        // OK
    } else {
        std::cerr << "Used/available tracking wrong" << std::endl;
        return false;
    }

    std::cout << "Test 1 passed" << std::endl;
    return true;
}

// Test 2: Alignment test
bool test_alignment() {
    std::cout << "Test 2: Alignment..." << std::endl;

    StackAllocatorST allocator(4096);

    // First allocation small
    allocator.allocate(1);

    // Next allocation should be aligned to 8
    void* p = allocator.allocate(sizeof(uint64_t), alignof(uint64_t));
    uintptr_t addr = reinterpret_cast<uintptr_t>(p);

    if ((addr % alignof(uint64_t)) != 0) {
        std::cerr << "Alignment wrong: address " << addr << " not divisible by "
                  << alignof(uint64_t) << std::endl;
        return false;
    }

    std::cout << "Test 2 passed" << std::endl;
    return true;
}

// Test 3: Marker rollback
bool test_marker_rollback() {
    std::cout << "Test 3: Marker rollback..." << std::endl;

    StackAllocatorST allocator(4096);

    allocator.allocate(100);
    size_t used_after_100 = allocator.used();
    auto marker = allocator.getMarker();

    allocator.allocate(200);
    allocator.allocate(300);

    if (allocator.used() <= used_after_100) {
        std::cerr << "Used should be larger after allocations" << std::endl;
        return false;
    }

    // Rollback
    allocator.deallocateToMarker(marker);

    if (allocator.used() != used_after_100) {
        std::cerr << "After rollback: expected used " << used_after_100
                  << ", got " << allocator.used() << std::endl;
        return false;
    }

    std::cout << "Test 3 passed" << std::endl;
    return true;
}

// Test 4: Full reset
bool test_reset() {
    std::cout << "Test 4: Full reset..." << std::endl;

    StackAllocatorST allocator(4096);

    allocator.allocate(100);
    allocator.allocate(200);
    allocator.allocate(300);

    allocator.reset();

    if (allocator.used() != 0) {
        std::cerr << "After reset: expected used 0, got " << allocator.used() << std::endl;
        return false;
    }

    if (allocator.available() != allocator.capacity()) {
        std::cerr << "After reset: available should equal capacity" << std::endl;
        return false;
    }

    // Should be able to allocate again
    void* p = allocator.allocate(500);
    if (!p) {
        std::cerr << "Cannot allocate after reset" << std::endl;
        return false;
    }

    std::cout << "Test 4 passed" << std::endl;
    return true;
}

// Test 5: Out of memory handling
bool test_out_of_memory() {
    std::cout << "Test 5: Out of memory handling..." << std::endl;

    StackAllocatorST allocator(1024);  // One page, 1024 rounded to 4096

    // Allocate most of it
    allocator.allocate(3000);

    // Try to allocate more than available
    void* p = allocator.allocate(2000);
    if (p != nullptr) {
        std::cerr << "Should return nullptr for out of memory" << std::endl;
        return false;
    }

    std::cout << "Test 5 passed" << std::endl;
    return true;
}

// Test 6: Thread safety
bool test_thread_safety() {
    std::cout << "Test 6: Thread safety..." << std::endl;

    const int num_threads = 4;
    const int allocs_per_thread = 100;

    StackAllocatorMT allocator(1024 * 1024);  // 1MB
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < allocs_per_thread; ++i) {
                auto marker = allocator.getMarker();
                int* arr = static_cast<int*>(allocator.allocate(100 * sizeof(int)));
                if (arr) {
                    for (int j = 0; j < 100; ++j) {
                        arr[j] = j;
                    }
                    bool ok = true;
                    for (int j = 0; j < 100; ++j) {
                        if (arr[j] != j) {
                            ok = false;
                            break;
                        }
                    }
                    if (ok) success_count++;
                }
                allocator.deallocateToMarker(marker);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    if (success_count != num_threads * allocs_per_thread) {
        std::cerr << "Expected " << (num_threads * allocs_per_thread)
                  << " successful allocations, got " << success_count << std::endl;
        return false;
    }

    // After all, should be back to start
    if (allocator.used() == 0 || allocator.used() < 1000) {
        // OK, most allocations were rolled back
    } else {
        std::cerr << "Too much memory in use after all rollbacks" << std::endl;
        return false;
    }

    std::cout << "Test 6 passed" << std::endl;
    return true;
}

// Test 7: Multiple allocations with different alignments
bool test_multiple_alignments() {
    std::cout << "Test 7: Multiple allocations with different alignments..." << std::endl;

    StackAllocatorST allocator(4096);

    struct alignas(16) Aligned16 {
        char data[16];
    };

    struct alignas(32) Aligned32 {
        char data[32];
    };

    Aligned16* p1 = static_cast<Aligned16*>(allocator.allocate(sizeof(Aligned16), alignof(Aligned16)));
    uintptr_t a1 = reinterpret_cast<uintptr_t>(p1);
    if ((a1 % 16) != 0) {
        std::cerr << "Alignment 16 failed: " << a1 << std::endl;
        return false;
    }

    char* p2 = static_cast<char*>(allocator.allocate(1, 1));
    if ((reinterpret_cast<uintptr_t>(p2) % 1) != 0) {
        std::cerr << "Alignment 1 failed" << std::endl;
        return false;
    }

    Aligned32* p3 = static_cast<Aligned32*>(allocator.allocate(sizeof(Aligned32), alignof(Aligned32)));
    uintptr_t a3 = reinterpret_cast<uintptr_t>(p3);
    if ((a3 % 32) != 0) {
        std::cerr << "Alignment 32 failed: " << a3 << std::endl;
        return false;
    }

    std::cout << "Test 7 passed" << std::endl;
    return true;
}

int main() {
    std::cout << "Running StackAllocator tests..." << std::endl << std::endl;

    bool all_passed = true;

    all_passed &= test_basic_allocation();
    std::cout << std::endl;
    all_passed &= test_alignment();
    std::cout << std::endl;
    all_passed &= test_marker_rollback();
    std::cout << std::endl;
    all_passed &= test_reset();
    std::cout << std::endl;
    all_passed &= test_out_of_memory();
    std::cout << std::endl;
    all_passed &= test_thread_safety();
    std::cout << std::endl;
    all_passed &= test_multiple_alignments();
    std::cout << std::endl;

    if (all_passed) {
        std::cout << "All StackAllocator tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some tests failed!" << std::endl;
        return 1;
    }
}

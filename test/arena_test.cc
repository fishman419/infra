#include "arena.h"
#include <iostream>
#include <vector>
#include <thread>
#include <cassert>

// Helper function to print test header
void print_test(const std::string& test_name) {
    std::cout << "\n=== " << test_name << " ===\n";
}

// Test 1: Basic allocation
void test_basic_allocation() {
    print_test("Basic Allocation");

    infra::ArenaST arena(1024 * 1024);  // 1MB arena

    // Allocate some memory
    void* ptr1 = arena.allocate(100);
    assert(ptr1 != nullptr);

    void* ptr2 = arena.allocate(200, 16);
    assert(ptr2 != nullptr);

    std::cout << "Allocated 100 bytes at " << ptr1 << "\n";
    std::cout << "Allocated 200 bytes (16-byte aligned) at " << ptr2 << "\n";
    std::cout << "Arena used: " << arena.used() << " bytes\n";

    arena.reset();
    std::cout << "After reset: " << arena.used() << " bytes\n";

    assert(arena.used() == 0);
}

// Test 2: Slab allocation
void test_slab_allocation() {
    print_test("Slab Allocation");

    infra::ArenaST arena(1024 * 1024);

    // Allocate small objects (should use slab)
    std::vector<void*> small_objects;
    for (int i = 0; i < 20; ++i) {
        void* ptr = arena.allocate(64);
        assert(ptr != nullptr);
        small_objects.push_back(ptr);
    }

    std::cout << "Allocated 20 small objects (64 bytes each)\n";
    std::cout << "Arena used: " << arena.used() << " bytes\n";

    // Deallocate some
    for (int i = 0; i < 10; ++i) {
        arena.deallocate(small_objects[i]);
    }
    small_objects.resize(10);

    std::cout << "Deallocated 10 objects\n";
    std::cout << "Arena used after deallocation: " << arena.used() << " bytes\n";

    arena.reset();
}

// Test 3: Large allocation
void test_large_allocation() {
    print_test("Large Allocation");

    infra::ArenaST arena(1024 * 1024);

    // Allocate large object (should use arena directly)
    void* large_ptr = arena.allocate(5000, 64);
    assert(large_ptr != nullptr);

    std::cout << "Allocated large object: 5000 bytes (64-byte aligned)\n";
    std::cout << "Arena used: " << arena.used() << " bytes\n";

    arena.reset();
}

// Test 4: Arena growth
void test_arena_growth() {
    print_test("Arena Growth");

    // Start with small arena
    infra::ArenaST arena(1024);

    // Allocate until we need more memory
    std::vector<void*> pointers;

    for (int i = 0; i < 5; ++i) {
        void* ptr = arena.allocate(1000);
        assert(ptr != nullptr);
        pointers.push_back(ptr);

        std::cout << "Allocation " << i << ": arena size = " << arena.size()
                  << ", used = " << arena.used() << "\n";
    }

    arena.reset();
}

// Test 5: Move semantics
void test_move_semantics() {
    print_test("Move Semantics");

    infra::ArenaST arena1(1024 * 1024);
    arena1.allocate(100);
    arena1.allocate(200);

    std::cout << "Original arena used: " << arena1.used() << "\n";

    // Move construct
    infra::ArenaST arena2 = std::move(arena1);
    std::cout << "After move construction, arena2 used: " << arena2.used() << "\n";

    // Move assign
    infra::ArenaST arena3(512);
    arena3 = std::move(arena2);
    std::cout << "After move assignment, arena3 used: " << arena3.used() << "\n";
}

// Test 6: Thread safety
void test_thread_safety() {
    print_test("Thread Safety");

    infra::ArenaMT arena(2 * 1024 * 1024);  // 2MB thread-safe arena

    const int num_threads = 2;
    const int allocations_per_thread = 20;
    (void)allocations_per_thread;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&arena, i]() {
            for (int j = 0; j < 20; ++j) {
                size_t size = 100 + (i * 20 + j) % 200; // Use literal 20 instead of variable
                void* ptr = arena.allocate(size);
                assert(ptr != nullptr);
                arena.deallocate(ptr);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "Thread-safe test completed\n";
    arena.print_memory_stats();
}

// Test 7: Stress test
void test_stress_test() {
    print_test("Stress Test");

    infra::ArenaST arena(1024 * 1024);
    arena.enable_leak_detection(true);

    const int iterations = 100;
    std::vector<void*> pointers;
    size_t total_allocated = 0;

    // Mix of small and large allocations
    for (int i = 0; i < iterations; ++i) {
        size_t size = (i % 5 == 0) ? 500 : 50;
        void* ptr = arena.allocate(size);
        assert(ptr != nullptr);
        pointers.push_back(ptr);
        total_allocated += size;

        // Deallocate some to test fragmentation
        if (i % 10 == 0 && !pointers.empty()) {
            arena.deallocate(pointers.back());
            pointers.pop_back();
        }
    }

    std::cout << "Stress test: " << iterations << " allocations\n";
    std::cout << "Total allocated: " << total_allocated << " bytes\n";
    std::cout << "Arena used: " << arena.used() << " bytes\n";
    std::cout << "Fragmentation: " << (arena.available() * 100.0 / arena.size()) << "%\n";

    // Clean up
    for (void* ptr : pointers) {
        arena.deallocate(ptr);
    }

    arena.reset();
}

// Test 8: Shrink to fit
void test_shrink_to_fit() {
    print_test("Shrink to Fit");

    infra::ArenaST arena(1024);

    // Allocate until we get multiple blocks
    std::vector<void*> pointers;
    for (int i = 0; i < 5; ++i) {
        void* ptr = arena.allocate(1000);
        assert(ptr != nullptr);
        pointers.push_back(ptr);
    }

    size_t initial_size = arena.size();
    std::cout << "Initial arena size: " << initial_size << " bytes\n";

    // Shrink to fit
    arena.shrink_to_fit();
    size_t shrunk_size = arena.size();
    std::cout << "After shrink_to_fit: " << shrunk_size << " bytes\n";

    assert(shrunk_size < initial_size);

    // Clean up
    for (void* ptr : pointers) {
        arena.deallocate(ptr);
    }

    arena.reset();
}

// Test 9: Leak detection
void test_leak_detection() {
    print_test("Leak Detection");

    infra::ArenaST arena(1024 * 1024);
    arena.enable_leak_detection(true);

    // Allocate some memory
    void* ptr1 = arena.allocate(100);
    void* ptr2 = arena.allocate(200);

    // Deallocate one
    arena.deallocate(ptr1);
    arena.deallocate(ptr2);

    // Reset should clear allocations
    arena.reset();

    // Now allocate again
    void* ptr3 = arena.allocate(50);
    assert(ptr3 != nullptr);

    arena.deallocate(ptr3);
    arena.enable_leak_detection(false);
}

// Test 10: Exception handling
void test_exception_handling() {
    print_test("Exception Handling");

    // Test zero size arena
    try {
        infra::ArenaST arena(0);
        assert(false); // Should throw
    } catch (const std::invalid_argument& e) {
        std::cout << "Caught expected exception: " << e.what() << "\n";
    }

    // Test zero size allocation
    infra::ArenaST arena(1024);
    void* ptr = arena.allocate(0);
    assert(ptr == nullptr);

    std::cout << "Zero size allocation returned nullptr as expected\n";
}

// Test 11: Memory statistics
void test_memory_statistics() {
    print_test("Memory Statistics");

    infra::ArenaST arena(1024 * 1024);

    // Allocate some memory
    arena.allocate(100);
    arena.allocate(200);
    arena.allocate(300);

    // Print statistics
    std::cout << "Printing memory statistics:\n";
    arena.print_memory_stats();

    arena.reset();
}

// Test 12: Slab operations
void test_slab_operations() {
    print_test("Slab Operations");

    infra::ArenaST arena(1024 * 1024);

    // Test different slab sizes
    std::vector<void*> small_ptrs;
    std::vector<void*> medium_ptrs;

    // Allocate small objects
    for (int i = 0; i < 10; ++i) {
        void* ptr = arena.allocate(8); // Smallest slab size
        assert(ptr != nullptr);
        small_ptrs.push_back(ptr);
    }

    // Allocate medium objects
    for (int i = 0; i < 10; ++i) {
        void* ptr = arena.allocate(64); // Medium slab size
        assert(ptr != nullptr);
        medium_ptrs.push_back(ptr);
    }

    std::cout << "Allocated 10 small (8-byte) and 10 medium (64-byte) objects\n";

    // Deallocate all
    for (void* ptr : small_ptrs) {
        arena.deallocate(ptr);
    }
    for (void* ptr : medium_ptrs) {
        arena.deallocate(ptr);
    }

    // Reset and verify
    arena.reset();
    assert(arena.used() == 0);
}

int main() {
    std::cout << "Starting Arena Tests...\n";

    try {
        test_basic_allocation();
        test_slab_allocation();
        test_large_allocation();
        test_arena_growth();
        test_move_semantics();
        test_thread_safety();
        test_stress_test();
        test_shrink_to_fit();
        test_leak_detection();
        test_exception_handling();
        test_memory_statistics();
        test_slab_operations();

        std::cout << "\n=== All Tests Passed! ===\n";

    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
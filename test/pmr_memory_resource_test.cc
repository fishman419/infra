#if __cplusplus >= 201703L

#include <iostream>
#include <vector>
#include <string>
#include <memory_resource>
#include <thread>
#include <atomic>
#include "pmr_memory_resource.h"
#include "arena.h"

using namespace infra;

// Test 1: Basic PMR vector allocation
bool test_pmr_vector() {
    std::cout << "Test 1: PMR vector allocation..." << std::endl;

    ArenaST arena(1024 * 1024);
    PMRMemoryResource<ArenaST> pmr(&arena);

    std::pmr::vector<int> vec(&pmr);

    for (int i = 0; i < 1000; ++i) {
        vec.push_back(i);
    }

    bool ok = true;
    for (int i = 0; i < 1000; ++i) {
        if (vec[i] != i) {
            ok = false;
            break;
        }
    }

    if (!ok) {
        std::cerr << "Vector data corrupted" << std::endl;
        return false;
    }

    std::cout << "Vector used " << arena.used() << " bytes of arena memory" << std::endl;
    std::cout << "Test 1 passed" << std::endl;
    return true;
}

// Test 2: PMR vector of strings
bool test_pmr_strings() {
    std::cout << "Test 2: PMR vector of strings..." << std::endl;

    ArenaMT arena(1024 * 1024);
    PMRMemoryResource<ArenaMT> pmr(&arena);

    std::pmr::vector<std::pmr::string> vec(&pmr);

    for (int i = 0; i < 100; ++i) {
        vec.emplace_back("String number " + std::to_string(i));
    }

    // Just verify we can build the vector without crashing
    if (vec.size() != 100) {
        std::cerr << "Expected 100 strings, got " << vec.size() << std::endl;
        return false;
    }

    std::cout << "Test 2 passed" << std::endl;
    return true;
}

// Test 3: Thread-safe PMR with ArenaMT
bool test_pmr_thread_safe() {
    std::cout << "Test 3: Thread-safe PMR..." << std::endl;

    ArenaMT arena(1024 * 1024);
    PMRMemoryResource<ArenaMT> pmr(&arena);
    std::atomic<int> counter(0);

    const int num_threads = 4;
    const int allocs_per_thread = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < allocs_per_thread; ++i) {
                std::pmr::vector<int> vec(&pmr);
                for (int j = 0; j < 10; ++j) {
                    vec.push_back(j);
                }
                counter++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    if (counter != num_threads * allocs_per_thread) {
        std::cerr << "Expected " << (num_threads * allocs_per_thread)
                  << " completed allocations, got " << counter << std::endl;
        return false;
    }

    std::cout << "Test 3 passed" << std::endl;
    return true;
}

int main() {
    std::cout << "Running PMRMemoryResource tests (C++17 required)..." << std::endl << std::endl;

    bool all_passed = true;

    all_passed &= test_pmr_vector();
    std::cout << std::endl;
    all_passed &= test_pmr_strings();
    std::cout << std::endl;
    all_passed &= test_pmr_thread_safe();
    std::cout << std::endl;

    if (all_passed) {
        std::cout << "All PMRMemoryResource tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some tests failed!" << std::endl;
        return 1;
    }
}

#else

#include <iostream>

int main() {
    std::cout << "PMR tests require C++17 or later, skipping..." << std::endl;
    return 0;
}

#endif

/*
 * Comprehensive Arena Allocator Examples
 */

#include "../memory/arena.h"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

// Example 1: Basic usage
void example_basic_usage() {
    std::cout << "=== Basic Arena Usage ===\n";

    // Create a 1MB arena
    infra::ArenaST arena(1024 * 1024);

    // Allocate some memory
    int* numbers = static_cast<int*>(arena.allocate(100 * sizeof(int)));
    double* values = static_cast<double*>(arena.allocate(50 * sizeof(double), 64));

    // Use the memory
    for (int i = 0; i < 100; ++i) {
        numbers[i] = i * 2;
    }

    std::cout << "Allocated and initialized 100 integers and 50 doubles\n";
    std::cout << "Arena used: " << arena.used() << " bytes\n";

    // Reset the arena (all memory is reclaimed)
    arena.reset();
    std::cout << "After reset: " << arena.used() << " bytes\n";
}

// Example 2: Object allocation with RAII
struct GameObject {
    int id;
    std::string name;
    std::vector<float> position;

    GameObject(int id, const std::string& name)
        : id(id), name(name), position{0.0f, 0.0f, 0.0f} {}

    void update_position(float x, float y, float z) {
        position[0] = x;
        position[1] = y;
        position[2] = z;
    }
};

void example_object_allocation() {
    std::cout << "\n=== Object Allocation Example ===\n";

    infra::ArenaST arena(2 * 1024 * 1024);  // 2MB arena
    arena.enable_leak_detection(true);

    // Allocate objects in arena
    std::vector<GameObject*> objects;

    for (int i = 0; i < 100; ++i) {
        // Use placement new to construct objects in arena memory
        void* memory = arena.allocate(sizeof(GameObject));
        GameObject* obj = new (memory) GameObject(i, "Object " + std::to_string(i));
        obj->update_position(i * 0.1f, i * 0.2f, i * 0.3f);
        objects.push_back(obj);
    }

    std::cout << "Created " << objects.size() << " game objects\n";
    std::cout << "Arena used: " << arena.used() << " bytes\n";

    // Destroy objects
    for (auto* obj : objects) {
        obj->~GameObject();
    }

    arena.reset();
    std::cout << "After cleanup: " << arena.used() << " bytes\n";
}

// Example 3: String pooling
void example_string_pooling() {
    std::cout << "\n=== String Pooling Example ===\n";

    infra::ArenaST arena(1024 * 1024);

    // Arena is great for temporary strings
    std::vector<const char*> strings;

    for (int i = 0; i < 1000; ++i) {
        std::string str = "String number " + std::to_string(i);
        void* memory = arena.allocate(str.size() + 1);
        char* arena_str = static_cast<char*>(memory);
        std::strcpy(arena_str, str.c_str());
        strings.push_back(arena_str);
    }

    std::cout << "Pooled " << strings.size() << " strings\n";
    std::cout << "Total string size: " << arena.used() << " bytes\n";
    std::cout << "Sample string: " << strings[0] << "\n";

    arena.reset();
}

// Example 4: Multi-threaded usage
void example_multithreaded() {
    std::cout << "\n=== Multi-threaded Example ===\n";

    infra::ArenaMT arena(10 * 1024 * 1024);  // Thread-safe arena

    const int num_threads = 4;
    std::vector<std::thread> threads;
    std::vector<std::vector<int*>> thread_results(num_threads);

    auto worker = [&](int thread_id) {
        for (int i = 0; i < 1000; ++i) {
            // Allocate a batch of numbers
            int* batch = static_cast<int*>(arena.allocate(100 * sizeof(int)));
            thread_results[thread_id].push_back(batch);

            // Initialize
            for (int j = 0; j < 100; ++j) {
                batch[j] = thread_id * 10000 + i * 100 + j;
            }
        }
    };

    // Start threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i);
    }

    // Wait for completion
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "All threads completed allocations\n";
    std::cout << "Arena used: " << arena.used() << " bytes\n";

    arena.reset();
}

// Example 5: Performance benchmark
void example_performance_benchmark() {
    std::cout << "\n=== Performance Benchmark ===\n";

    const size_t iterations = 100000;
    const size_t object_size = sizeof(int);

    // Test arena allocator
    infra::ArenaST arena(10 * 1024 * 1024);
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iterations; ++i) {
        void* ptr = arena.allocate(object_size);
        *static_cast<int*>(ptr) = static_cast<int>(i);
        arena.deallocate(ptr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto arena_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Test standard allocator
    start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iterations; ++i) {
        void* ptr = malloc(object_size);
        *static_cast<int*>(ptr) = static_cast<int>(i);
        free(ptr);
    }

    end = std::chrono::high_resolution_clock::now();
    auto malloc_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Arena allocator: " << arena_duration.count() << " μs\n";
    std::cout << "malloc/free:     " << malloc_duration.count() << " μs\n";

    if (malloc_duration.count() > 0) {
        std::cout << "Arena speedup: " <<
            (static_cast<double>(malloc_duration.count()) / arena_duration.count()) << "x\n";
    }
}

// Example 6: Custom alignment
struct AlignedData {
    char data[128];
    // This struct needs 16-byte alignment for SSE
};

void example_custom_alignment() {
    std::cout << "\n=== Custom Alignment Example ===\n";

    infra::ArenaST arena(1024 * 1024);

    // Allocate with custom alignment
    AlignedData* data1 = static_cast<AlignedData*>(arena.allocate(sizeof(AlignedData), 16));
    AlignedData* data2 = static_cast<AlignedData*>(arena.allocate(sizeof(AlignedData), 32));

    std::cout << "Allocated with 16-byte alignment at: " << data1 << "\n";
    std::cout << "Allocated with 32-byte alignment at: " << data2 << "\n";
    std::cout << "Alignment checks: "
              << ((reinterpret_cast<uintptr_t>(data1) & 0xF) == 0 ? "✓" : "✗") << " "
              << ((reinterpret_cast<uintptr_t>(data2) & 0x1F) == 0 ? "✓" : "✗") << "\n";

    arena.reset();
}

int main() {
    std::cout << "Arena Allocator Examples\n\n";

    try {
        example_basic_usage();
        example_object_allocation();
        example_string_pooling();
        example_multithreaded();
        example_performance_benchmark();
        example_custom_alignment();

        std::cout << "\nAll examples completed successfully!\n";

    } catch (const std::exception& e) {
        std::cerr << "Example failed: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
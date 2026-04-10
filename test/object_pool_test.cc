#include <iostream>
#include <vector>
#include <thread>
#include <random>
#include "object_pool.h"

using namespace infra;

// Test object with constructor/destructor tracking
struct TestObject {
    int value;
    static int constructor_count;
    static int destructor_count;

    TestObject() : value(0) {
        constructor_count++;
    }

    explicit TestObject(int v) : value(v) {
        constructor_count++;
    }

    ~TestObject() {
        destructor_count++;
    }

    static void resetCounters() {
        constructor_count = 0;
        destructor_count = 0;
    }
};

int TestObject::constructor_count = 0;
int TestObject::destructor_count = 0;

// Test 1: Basic allocation and release
bool test_basic() {
    std::cout << "Test 1: Basic allocation and release..." << std::endl;

    TestObject::resetCounters();
    ObjectPool<TestObject> pool(4);

    if (pool.available() != 4) {
        std::cerr << "Expected 4 available, got " << pool.available() << std::endl;
        return false;
    }

    if (pool.allocated() != 4) {
        std::cerr << "Expected 4 allocated, got " << pool.allocated() << std::endl;
        return false;
    }

    if (pool.inUse() != 0) {
        std::cerr << "Expected 0 in use, got " << pool.inUse() << std::endl;
        return false;
    }

    // Acquire one
    TestObject* obj1 = pool.acquire(42);
    if (!obj1) {
        std::cerr << "Failed to acquire object" << std::endl;
        return false;
    }

    if (obj1->value != 42) {
        std::cerr << "Expected value 42, got " << obj1->value << std::endl;
        return false;
    }

    if (pool.available() != 3) {
        std::cerr << "Expected 3 available after acquire, got " << pool.available() << std::endl;
        return false;
    }

    if (pool.inUse() != 1) {
        std::cerr << "Expected 1 in use after acquire, got " << pool.inUse() << std::endl;
        return false;
    }

    if (TestObject::constructor_count != 1) {
        std::cerr << "Expected 1 constructor call, got " << TestObject::constructor_count << std::endl;
        return false;
    }

    // Release it back
    pool.release(obj1);

    if (pool.available() != 4) {
        std::cerr << "Expected 4 available after release, got " << pool.available() << std::endl;
        return false;
    }

    if (pool.inUse() != 0) {
        std::cerr << "Expected 0 in use after release, got " << pool.inUse() << std::endl;
        return false;
    }

    if (TestObject::destructor_count != 1) {
        std::cerr << "Expected 1 destructor call, got " << TestObject::destructor_count << std::endl;
        return false;
    }

    std::cout << "Test 1 passed" << std::endl;
    return true;
}

// Test 2: Multiple acquire and dynamic growth
bool test_growth() {
    std::cout << "Test 2: Multiple acquire and dynamic growth..." << std::endl;

    ObjectPool<TestObject> pool(2);
    std::vector<TestObject*> objects;

    // Acquire more than pre-allocated
    for (int i = 0; i < 5; ++i) {
        TestObject* obj = pool.acquire(i);
        if (!obj) {
            std::cerr << "Failed to acquire object " << i << std::endl;
            return false;
        }
        if (obj->value != i) {
            std::cerr << "Expected value " << i << ", got " << obj->value << std::endl;
            return false;
        }
        objects.push_back(obj);
    }

    // 2 initial + 2 on first growth + 2 on second growth = 6 total
    if (pool.allocated() != 6) {
        std::cerr << "Expected 6 allocated after growth, got " << pool.allocated() << std::endl;
        return false;
    }

    if (pool.inUse() != 5) {
        std::cerr << "Expected 5 in use, got " << pool.inUse() << std::endl;
        return false;
    }

    // Release all
    for (auto obj : objects) {
        pool.release(obj);
    }

    if (pool.available() != 6) {
        std::cerr << "Expected 6 available after release all, got " << pool.available() << std::endl;
        return false;
    }

    std::cout << "Test 2 passed" << std::endl;
    return true;
}

// Test 3: Move semantics
bool test_move() {
    std::cout << "Test 3: Move semantics..." << std::endl;

    ObjectPool<TestObject> pool1(4);
    TestObject* obj = pool1.acquire(123);

    ObjectPool<TestObject> pool2(std::move(pool1));

    if (pool1.allocated() != 0) {
        std::cerr << "Expected pool1 to be empty after move" << std::endl;
        return false;
    }

    if (pool2.allocated() != 4) {
        std::cerr << "Expected pool2 to have 4 allocated after move" << std::endl;
        return false;
    }

    if (pool2.inUse() != 1) {
        std::cerr << "Expected pool2 to have 1 in use after move" << std::endl;
        return false;
    }

    pool2.release(obj);

    std::cout << "Test 3 passed" << std::endl;
    return true;
}

// Test 4: Thread safety
bool test_thread_safety() {
    std::cout << "Test 4: Thread safety..." << std::endl;

    const int num_threads = 8;
    const int acquires_per_thread = 1000;

    ObjectPoolMT<TestObject> pool(16);
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 100);

            for (int i = 0; i < acquires_per_thread; ++i) {
                int val = dis(gen);
                TestObject* obj = pool.acquire(val);
                if (!obj) continue;

                if (obj->value != val) {
                    continue;
                }

                // Simulate some work
                for (volatile int j = 0; j < 10; ++j) {}

                pool.release(obj);
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    if (success_count != num_threads * acquires_per_thread) {
        std::cerr << "Expected " << (num_threads * acquires_per_thread)
                  << " successful acquires, got " << success_count << std::endl;
        return false;
    }

    // All should be available
    if (pool.available() != pool.allocated()) {
        std::cerr << "Expected all available after threads complete" << std::endl;
        return false;
    }

    std::cout << "Test 4 passed (" << success_count << " acquires)" << std::endl;
    return true;
}

// Test 5: Non-trivial object
struct ComplexObject {
    int* data;
    int size;

    ComplexObject(int s) : size(s) {
        data = new int[s];
        for (int i = 0; i < s; ++i) {
            data[i] = i;
        }
    }

    ~ComplexObject() {
        delete[] data;
    }
};

bool test_complex_object() {
    std::cout << "Test 5: Non-trivial object with dynamic allocation..." << std::endl;

    ObjectPool<ComplexObject> pool(4);

    ComplexObject* obj1 = pool.acquire(100);
    if (!obj1) {
        std::cerr << "Failed to acquire complex object" << std::endl;
        return false;
    }

    for (int i = 0; i < 100; ++i) {
        if (obj1->data[i] != i) {
            std::cerr << "Data corrupted at index " << i << std::endl;
            return false;
        }
    }

    pool.release(obj1);

    ComplexObject* obj2 = pool.acquire(200);
    if (!obj2) {
        std::cerr << "Failed to acquire second complex object" << std::endl;
        return false;
    }

    pool.release(obj2);

    std::cout << "Test 5 passed" << std::endl;
    return true;
}

int main() {
    std::cout << "Running ObjectPool tests..." << std::endl << std::endl;

    bool all_passed = true;

    all_passed &= test_basic();
    std::cout << std::endl;
    all_passed &= test_growth();
    std::cout << std::endl;
    all_passed &= test_move();
    std::cout << std::endl;
    all_passed &= test_thread_safety();
    std::cout << std::endl;
    all_passed &= test_complex_object();
    std::cout << std::endl;

    if (all_passed) {
        std::cout << "All ObjectPool tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some tests failed!" << std::endl;
        return 1;
    }
}

/*
 * Basic usage example for the Infra slab allocator
 */

#include "../memory/slab.h"
#include <iostream>

int main() {
    // Example 1: Basic slab for integers
    std::cout << "=== Basic Integer Slab ===" << std::endl;
    infra::Slab<int> int_slab(10);

    if (int_slab.Init() == 0) {
        // Get some integers
        int* a = int_slab.Get();
        int* b = int_slab.Get();
        int* c = int_slab.Get();

        if (a && b && c) {
            *a = 100;
            *b = 200;
            *c = 300;

            std::cout << "Allocated integers: " << *a << ", " << *b << ", " << *c << std::endl;

            // Return them to the slab
            int_slab.Put(a);
            int_slab.Put(b);
            int_slab.Put(c);

            std::cout << "Returned integers to slab" << std::endl;
        }
    }

    // Example 2: Slab for custom objects
    std::cout << "\n=== Custom Object Slab ===" << std::endl;

    struct Point {
        int x;
        int y;
        Point(int x, int y) : x(x), y(y) {}
    };

    infra::Slab<Point> point_slab(5);

    if (point_slab.Init() == 0) {
        // Get Point objects
        Point* p1 = point_slab.Get();
        Point* p2 = point_slab.Get();

        if (p1 && p2) {
            // Use placement new to construct objects
            new (p1) Point(10, 20);
            new (p2) Point(30, 40);

            std::cout << "Point 1: (" << p1->x << ", " << p1->y << ")" << std::endl;
            std::cout << "Point 2: (" << p2->x << ", " << p2->y << ")" << std::endl;

            // Destroy objects before returning
            p1->~Point();
            p2->~Point();

            // Return to slab
            point_slab.Put(p1);
            point_slab.Put(p2);
        }
    }

    // Example 3: Stress test
    std::cout << "\n=== Stress Test ===" << std::endl;
    infra::Slab<uint64_t> stress_slab(1000);

    if (stress_slab.Init() == 0) {
        // Allocate and deallocate many objects
        const int iterations = 100;
        uint64_t* objects[iterations];

        // Allocate
        for (int i = 0; i < iterations; ++i) {
            objects[i] = stress_slab.Get();
            if (objects[i]) {
                *objects[i] = i;
            }
        }

        // Verify
        for (int i = 0; i < iterations; ++i) {
            if (objects[i] && *objects[i] == static_cast<uint64_t>(i)) {
                std::cout << "Object " << i << ": OK" << std::endl;
            }
        }

        // Deallocate
        for (int i = 0; i < iterations; ++i) {
            if (objects[i]) {
                stress_slab.Put(objects[i]);
            }
        }

        std::cout << "Stress test completed" << std::endl;
    }

    return 0;
}
# Infra

Infrastructure components for high-performance memory management.

## Overview

The Infra project provides a slab allocator implementation optimized for high-performance systems. The slab allocator is designed to efficiently manage and reuse fixed-size memory objects, reducing allocation overhead and improving memory locality.

## Components

### Slab Allocator (`memory/slab.h`)

A generic template-based slab allocator that:

- Allocates memory in pages-aligned chunks for optimal performance
- Maintains free lists for fast object reuse
- Uses `mmap/munmap` for memory management
- Supports configurable slab counts per region
- Implements basic error handling

#### Features

- **Memory Efficiency**: Allocates memory in page-sized regions to minimize external fragmentation
- **Fast Allocation/Deallocation**: O(1) operations using free lists
- **Type-safe**: C++ template-based implementation
- **Scalable**: Supports multiple memory regions for large allocation requests

#### Usage

```cpp
#include "memory/slab.h"

// Create a slab for uint32_t objects with 1024 items
infra::Slab<uint32_t> slab(1024);

// Initialize the slab
slab.Init();

// Get an object from the slab
uint32_t* obj = slab.Get();

// Return the object to the slab
slab.Put(obj);
```

## Building and Testing

### Prerequisites

- C++ compiler (supporting C++11 or later)
- Standard libraries: `iostream`, `stdint.h`, `sys/mman.h`

### Compilation

```bash
# Compile the test
g++ memory/slab_test.cc -o slab_test

# Run the test
./slab_test
```

## Memory Layout

The slab allocator organizes memory into regions, each containing:

- A contiguous memory block allocated via `mmap`
- A free list tracking available objects
- Metadata about the region (size, count)

Each region's size is page-aligned to ensure efficient memory usage:

```
Memory Region
┌─────────────────────────────────────┐
│ Page 1: Objects 0-1023              │
├─────────────────────────────────────┤
│ Page 2: (if needed)                 │
└─────────────────────────────────────┘
```

## Performance Characteristics

- **Allocation Time**: O(1) - constant time for object acquisition
- **Deallocation Time**: O(1) - constant time for object release
- **Memory Overhead**: Minimal overhead per object (only the free list)
- **Memory Locality**: Objects of the same type are stored contiguously

## Limitations

1. Single region implementation (can be extended to multiple regions)
2. No thread safety (mutex locks would need to be added for concurrent use)
3. Fixed object size per slab instance
4. Basic error reporting (prints to stderr)

## Future Enhancements

- Multi-region support for larger allocation sizes
- Thread-safe implementation with mutex locks
- Custom memory allocators (replacing mmap)
- Statistics tracking (allocations, deallocations, hits/misses)
- Memory usage reporting
- Support for variable-sized objects

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Author

fishman, 2022
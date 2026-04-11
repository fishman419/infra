# Infra

Infrastructure components for high-performance memory management in C++.

All components are header-only, require C++11, and follow consistent coding conventions.

## Overview

The Infra project provides a collection of memory management components optimized for high-performance systems:

- **Arena/Linear Allocator** - fast bump allocation with optional deallocation support
- **Object Pool** - pre-allocated object pool for fixed-size objects with optional thread safety
- **Stack Allocator** - stack-based allocation with marker-based rollback
- **Slab Allocator** - efficient fixed-size object allocation
- **Buddy Allocator** - binary buddy memory allocation for variable-size blocks with minimal fragmentation
- **PMR Memory Resource** - C++17 `std::pmr::memory_resource` adapter for all Infra allocators

## Components

### Arena (`memory/arena.h`)

Fast bump-pointer (linear) allocator that supports:
- Single-shot reset deallocation
- Compile-time thread safety selection (single-threaded `ArenaST` / multi-threaded `ArenaMT`)
- Automatic growth when out of space
- Optional embedded SLAB allocator for small allocations to reduce fragmentation
- `shrink_to_fit` support to release unused memory

**Features:**
- **Ultra-fast allocation**: O(1) bump pointer allocation
- **Alignment support**: arbitrary alignment for all allocations
- **Memory efficiency**: only minimal metadata overhead

```cpp
#include "memory/arena.h"

infra::ArenaST arena(4096);
void* ptr = arena.allocate(256);
// ... use ptr ...
arena.reset(); // Reset entire arena
```

### Object Pool (`memory/object_pool.h`)

Pre-allocated pool of fixed-size objects:
- Compile-time thread safety selection
- Automatic dynamic growth when pool is exhausted
- Type-safe templated design
- O(1) acquire/release

```cpp
#include "memory/object_pool.h"

infra::ObjectPool<MyObject, true> pool; // thread-safe
MyObject* obj = pool.acquire();
// ... use obj ...
pool.release(obj);
```

### Stack Allocator (`memory/stack_allocator.h`)

Stack-based allocator with marker-based rollback:
- Perfect for short-lived allocations that can be rolled back in bulk
- Marker-based deallocation allows partial stack clearing
- Compile-time thread safety selection
- Alignment support

```cpp
#include "memory/stack_allocator.h"

infra::StackAllocatorST allocator(1024 * 1024);
auto marker = allocator.getMarker();
void* temp1 = allocator.allocate(128);
void* temp2 = allocator.rollback(marker); // Free temp1 and temp2
```

### Slab Allocator (`memory/slab.h`)

Generic template-based slab allocator optimized for fixed-size objects:
- Allocates memory in page-aligned chunks
- Maintains free lists for fast object reuse
- Uses `mmap/munmap` for direct OS memory management

```cpp
#include "memory/slab.h"

infra::Slab<uint32_t> slab(1024);
slab.Init();
uint32_t* obj = slab.Get();
slab.Put(obj);
```

### Buddy Allocator (`memory/buddy_allocator.h`)

Classic binary buddy memory allocation for variable-size allocations within a fixed contiguous memory region:
- O(log n) allocation and deallocation
- Excellent coalescing behavior minimizes fragmentation
- Direct OS memory management via mmap
- Free block statistics available for debugging

Perfect for fixed-size memory pools where you need variable-sized allocations and minimal fragmentation.

```cpp
#include "memory/buddy_allocator.h"

infra::BuddyAllocator allocator(4 * 1024 * 1024, 16); // 4MB total, 16-byte min block
void* p1 = allocator.allocate(256);
void* p2 = allocator.allocate(1024);
allocator.deallocate(p1);
allocator.deallocate(p2);
```

### PMRMemoryResource (`memory/pmr_memory_resource.h`)

Adapter to convert any Infra allocator to C++17 `std::pmr::memory_resource` for use with polymorphic memory allocator containers:
- Works with all existing Infra allocators
- Zero overhead wrapper
- Enables use of `std::pmr::vector`, `std::pmr::string`, etc. with custom allocators

```cpp
#if __cplusplus >= 201703L
#include "memory/pmr_memory_resource.h"
#include "memory/arena.h"

infra::ArenaMT arena(1024 * 1024);
infra::ArenaPMRMT pmr(&arena);
std::pmr::vector<int> vec(&pmr);
vec.push_back(42); // Allocated from arena
#endif
```

## Building and Testing

### Prerequisites

- C++ compiler (supporting C++11 or later)
- Standard libraries: `iostream`, `stdint.h`, `sys/mman.h`
- pthreads for multi-threaded tests

### Compilation

```bash
make all            # Build all tests
make test-all        # Run all tests
make test-buddy-allocator  # Run just buddy allocator tests
```

### Output

All tests print pass/fail status to stdout. If all tests pass, you're good to go!

## Performance Characteristics

| Component          | Allocation | Deallocation | Thread Safety | Best For |
|--------------------|------------|--------------|-------------|----------|
| Arena              | O(1)       | Bulk reset   | Optional    | Short-lived allocations, arenas |
| Object Pool        | O(1)       | O(1)        | Optional    | Reuse of fixed-size objects |
| Stack Allocator    | O(1)       | O(1) rollback | Optional | Temporary allocations that can be rolled back |
| Slab              | O(1)       | O(1)        | No         | Fixed-size kernel-style object allocation |
| Buddy Allocator   | O(log n)    | O(log n)     | No         | Variable-size allocations in fixed memory pool |

## Project Structure

```
infra/
├── memory/          # Core header-only memory management components
├── test/            # Test files (one per component)
├── examples/        # Usage examples
├── docs/            # Documentation
└── Makefile         # Build system
```

## Coding Guidelines

See [CLAUDE.md](CLAUDE.md) for project coding conventions.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Author

fishman
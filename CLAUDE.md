# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Infra is a collection of high-performance memory management components for C++ (C++11 required). All components are header-only implementations.

## Quick Commands

```bash
make all              # Build all tests and examples
make test-all         # Run all tests
make test-<component> # Run single component test (e.g., test-arena, test-buddy-allocator)
make clean            # Remove build artifacts
clang-format -i memory/*.h test/*.cc  # Format all code
```

Component test commands:
- `test-arena` - Arena allocator tests
- `test-object-pool` - Object pool tests
- `test-stack-allocator` - Stack allocator tests
- `test-buddy-allocator` - Buddy allocator tests
- `test-pmr` - PMR memory resource tests (requires C++17)
- `test` - Slab allocator tests

## High-Level Architecture

### Memory Component Design Patterns

All allocators follow consistent design principles:

1. **Header-only templates** - No .cpp files, all implementation in headers
2. **Compile-time configuration** - Thread safety and features selected via template parameters
3. **Direct OS memory** - Uses `mmap`/`munmap` for page allocation (avoid malloc)
4. **Zero-overhead abstractions** - Empty base optimization for disabled features

### Component Hierarchy & Relationships

```
common.h (foundation)
├── EmptyLock / NoopLockGuard
├── LockTypeSelector<ThreadSafe>
└── Utility functions (roundUpToPage, alignUp, etc.)

Memory Components (all depend on common.h):
├── arena.h           - Bump-pointer allocator with optional embedded slab
├── object_pool.h     - Fixed-size object pool
├── stack_allocator.h - Marker-based stack allocation
├── slab.h            - Page-based fixed-size allocator
├── buddy_allocator.h - Binary buddy variable-size allocator
└── pmr_memory_resource.h - C++17 std::pmr adapter (wraps any allocator)
```

### Key Design Decisions

- **Thread safety**: Optional via `bool ThreadSafe` template parameter. When false, uses empty locks with zero overhead.
- **Memory ownership**: Allocators that grow own their memory (Arena, ObjectPool); allocators working on fixed regions (BuddyAllocator) do not.
- **Alignment**: All allocators support arbitrary alignment requests.
- **Error handling**: Constructors throw on failure; allocation functions return nullptr.

### Directory Structure

```
infra/
├── memory/          # Core header-only memory management components
├── test/            # Test files (one file per component, *_test.cc)
├── examples/        # Usage examples
├── docs/            # Documentation
├── Makefile         # Build system
└── CLAUDE.md        # This file
```

## Coding Guidelines

### Naming

- **Namespace:** All code inside `infra` namespace
- **Classes/Structs:** PascalCase (`ObjectPool`, `StackAllocator`, `LockTypeSelector`)
- **Member variables:** snake_case with trailing underscore (`object_size_`, `used_`, `lock_`)
- **Static constants:** `kConstantName` (`kPageSize`)
- **Member functions:** camelCase (`allocate()`, `release()`, `getMarker()`)
- **Typedefs:** PascalCase with suffix indicating specialization (`ObjectPoolMT`, `StackAllocatorST`)

### Formatting

- 4-space indentation (no tabs)
- Line length ~80 columns
- Opening brace on same line for functions/methods
- Doxygen-style comments for public APIs
- Header guards: `#ifndef INFRA_COMPONENT_NAME`, `#define INFRA_COMPONENT_NAME`, `#endif` at end

### Design Principles

1. **Header-only:** All components implemented in `.h` files (no `.cpp`)
2. **Compile-time configuration:** Use template parameters for optional features (zero overhead when disabled):
   ```cpp
   template <typename T, bool ThreadSafe = false>
   class ObjectPool { ... };
   ```
3. **Direct OS memory management:** Use `mmap`/`munmap` directly for page allocation instead of `malloc` when possible
4. **Empty base optimization:** For non-thread-safe configuration, use empty lock/lock guard classes to get zero size overhead
5. **Move semantics only:** Disable copy constructor/assignment, provide move support when appropriate
6. **User responsibility:** Stack/arena allocation don't call destructors automatically - user is responsible
7. **No external dependencies:** Only depend on standard library and system calls (`sys/mman.h`)

### Thread Safety

- Thread safety is optional and selected at compile-time via template parameter
- When `ThreadSafe = false`, no locking is done (zero overhead)
- When `ThreadSafe = true`, use `std::mutex`
- Common lock selection pattern (already in `memory/common.h`):
  ```cpp
  using Lock = typename LockTypeSelector<ThreadSafe>::Lock;
  using LockGuard = typename LockTypeSelector<ThreadSafe>::LockGuard;
  ```

### Memory Management

- Always check `mmap` return value, throw `std::bad_alloc` on failure for constructors
- Destructors must clean up all allocated memory (munmap any mapped regions)
- Use `MAP_PRIVATE | MAP_ANONYMOUS` for anonymous memory mappings

### Error Handling

- Return `nullptr` for allocation failures when exception can't be thrown
- Throw exceptions for constructor failures (can't return error)
- Basic error messages to stdout/stderr are acceptable

### Testing

- One test file per component, named `component_test.cc` in `test/` directory
- Test all major functionality including edge cases
- Thread safety tests with multiple concurrent threads
- All tests must pass before commit
- `Makefile` updated to include new test targets

### Common Utilities

Reuse these from `memory/common.h` instead of redefining:

- `EmptyLock` - empty lock for non-thread-safe case
- `NoopLockGuard` - no-op lock guard for non-thread-safe case
- `LockTypeSelector<ThreadSafe>` - selects the right lock type
- `kPageSize` - 4096 constant
- `roundUpToPage(size)` - round size up to page alignment
- `alignUp(addr, alignment)` - align address upward
- `alignmentPadding(addr, alignment)` - calculate required padding

## Makefile Conventions

- Add new test targets when adding new components
- Keep the existing pattern:
  - `test-component` - build and run the test
  - `test-all` - run all tests
- Output binaries go to `build/` directory
- Use `-pthread` for tests that use std::thread
- Keep CXXFLAGS: `-std=c++11 -Wall -Wextra -O2`

## Git Workflow

- Commit after each complete, working feature
- All tests must pass before commit
- Commit message format: `type: short description`
  - `feat:` new feature
  - `fix:` bug fix
  - `refactor:` code refactoring
  - `docs:` documentation update

## Review Checklist Before Adding New Code

- [ ] Follows naming conventions
- [ ] Uses common utilities from `common.h` where possible
- [ ] Header guards correctly named
- [ ] Compiles without warnings
- [ ] All existing tests still pass
- [ ] New tests added for new component
- [ ] `Makefile` updated if adding new component
- [ ] Directory structure respected (headers in `memory/`, tests in `test/`)

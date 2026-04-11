# Coding Guidelines for Infra Project

This document captures the coding conventions and requirements for this project, established through iterative development.

## Project Overview

Infra is a collection of high-performance memory management components for C++ (C++11 required). All components are header-only implementations.

## Directory Structure

```
infra/
├── memory/          # Core header-only memory management components
├── test/            # Test files (one file per component)
├── examples/        # Usage examples
├── docs/            # Documentation
└── CLAUDE.md        # This file - coding guidelines
```

## Coding Conventions

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

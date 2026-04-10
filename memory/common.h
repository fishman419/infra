#ifndef INFRA_COMMON
#define INFRA_COMMON

#include <mutex>
#include <type_traits>

namespace infra {

// Empty lock for non-thread-safe case (enables empty base optimization)
// Has zero size overhead when used with EBO
struct EmptyLock {
    void lock() noexcept {}
    void unlock() noexcept {}
};

// No-op lock guard for non-thread-safe case
struct NoopLockGuard {
    template <typename Lock>
    explicit NoopLockGuard(Lock&) noexcept {}
};

// Common page size constant
constexpr size_t kPageSize = 4096;

// Helper to select lock type based on ThreadSafe template parameter
template <bool ThreadSafe>
struct LockTypeSelector {
    using Lock = typename std::conditional<ThreadSafe, std::mutex, EmptyLock>::type;
    using LockGuard = typename std::conditional<ThreadSafe, std::lock_guard<std::mutex>, NoopLockGuard>::type;
};

// Round up size to page alignment
inline size_t roundUpToPage(size_t size) {
    return ((size + kPageSize - 1) / kPageSize) * kPageSize;
}

// Align address up to the specified alignment
inline uintptr_t alignUp(uintptr_t addr, size_t alignment) {
    return (addr + alignment - 1) & ~(alignment - 1);
}

// Calculate padding needed for alignment from current address
inline size_t alignmentPadding(uintptr_t addr, size_t alignment) {
    return ((addr + alignment - 1) & ~(alignment - 1)) - addr;
}

}  // namespace infra

#endif  // INFRA_COMMON

#ifndef INFRA_STACK_ALLOCATOR
#define INFRA_STACK_ALLOCATOR

#include <iostream>
#include <cstdint>
#include <sys/mman.h>
#include <mutex>
#include <cstring>
#include <new>
#include <algorithm>
#include "common.h"

namespace infra {

// Lightweight stack-based allocator with marker-based rollback
// Supports efficient linear allocation and bulk deallocation via markers
template <bool ThreadSafe = false>
class StackAllocator {
public:
    // Marker type for rollback
    typedef size_t Marker;

    // Constructor with total capacity
    explicit StackAllocator(size_t total_size)
        : total_size_(roundUpToPage(total_size))
        , used_(0)
        , base_(nullptr) {

        void* ptr = mmap(nullptr, total_size_, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
            throw std::bad_alloc();
        }

        base_ = static_cast<uint8_t*>(ptr);
    }

    // Destructor
    ~StackAllocator() {
        if (base_) {
            munmap(base_, total_size_);
        }
    }

    // Disable copy
    StackAllocator(const StackAllocator&) = delete;
    StackAllocator& operator=(const StackAllocator&) = delete;

    // Allocate memory with specified alignment
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        if (size == 0) {
            throw std::invalid_argument("Cannot allocate zero size");
        }

        LockGuard guard(lock_);

        // We need one byte before the aligned address to store padding
        // Start from current + 1 to guarantee space for padding byte
        uintptr_t current_addr = reinterpret_cast<uintptr_t>(base_ + used_ + 1);
        uintptr_t aligned_addr = alignUp(current_addr, alignment);
        size_t padding = aligned_addr - current_addr;

        // Total size: includes the padding byte we reserved at the beginning
        size_t total_needed = (aligned_addr - reinterpret_cast<uintptr_t>(base_ + used_)) + size;

        if (used_ + total_needed > total_size_) {
            // Out of memory
            return nullptr;
        }

        // Store padding in the byte before aligned address
        // padding + 1 because we already added 1 byte offset
        *(reinterpret_cast<uint8_t*>(aligned_addr) - 1) = static_cast<uint8_t>(padding + 1);

        used_ += total_needed;
        return reinterpret_cast<void*>(aligned_addr);
    }

    // Get current marker for rollback
    Marker getMarker() const {
        return used_;
    }

    // Deallocate all memory back to the given marker
    void deallocateToMarker(Marker marker) {
        LockGuard guard(lock_);

        if (marker > total_size_) {
            return;
        }

        // Doesn't call destructors - user is responsible
        used_ = marker;
    }

    // Reset entire stack to empty
    void reset() {
        LockGuard guard(lock_);
        used_ = 0;
    }

    // Get current used bytes
    size_t used() const {
        return used_;
    }

    // Get available bytes
    size_t available() const {
        return total_size_ - used_;
    }

    // Get total capacity
    size_t capacity() const {
        return total_size_;
    }

    // Get the high water mark (same as used)
    size_t highWaterMark() const {
        return used_;
    }

private:
    // Lock type selection
    using Lock = typename LockTypeSelector<ThreadSafe>::Lock;
    using LockGuard = typename LockTypeSelector<ThreadSafe>::LockGuard;

    size_t total_size_;
    size_t used_;
    uint8_t* base_;
    mutable Lock lock_;
};

// Convenience typedefs
typedef StackAllocator<false> StackAllocatorST;
typedef StackAllocator<true> StackAllocatorMT;

}  // namespace infra

#endif  // INFRA_STACK_ALLOCATOR

#ifndef INFRA_OBJECT_POOL
#define INFRA_OBJECT_POOL

#include <iostream>
#include <cstdint>
#include <sys/mman.h>
#include <mutex>
#include <new>
#include <cstring>
#include "common.h"

namespace infra {

// Type-safe object pool for efficient object reuse
// Automatically handles construction and destruction
// Thread-safe if ThreadSafe template parameter is true
template <typename T, bool ThreadSafe = false>
class ObjectPool {
public:
    // Constructor with pre-allocation
    // prealloc_count: number of objects to pre-allocate
    explicit ObjectPool(size_t prealloc_count = 64)
        : object_size_(std::max(sizeof(T), sizeof(T*)))
        , prealloc_count_(prealloc_count)
        , allocated_count_(0)
        , available_count_(0)
        , head_(nullptr)
        , memory_blocks_(nullptr) {

        if (prealloc_count > 0) {
            allocateBlock(prealloc_count);
        }
    }

    // Destructor - cleanup all memory
    // Objects still acquired by user are not destructed since memory is unmapped
    // This is fine as pool destruction implies all outstanding allocations are abandoned
    ~ObjectPool() {
        Block* block = memory_blocks_;
        while (block) {
            Block* next = block->next;
            munmap(block->base, block->total_size);
            delete block;
            block = next;
        }
    }

    // Disable copy
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    // Enable move
    ObjectPool(ObjectPool&& other) noexcept
        : object_size_(other.object_size_)
        , prealloc_count_(other.prealloc_count_)
        , allocated_count_(other.allocated_count_)
        , available_count_(other.available_count_)
        , head_(other.head_)
        , memory_blocks_(other.memory_blocks_)
        , lock_() {
        other.allocated_count_ = 0;
        other.available_count_ = 0;
        other.head_ = nullptr;
        other.memory_blocks_ = nullptr;
    }

    ObjectPool& operator=(ObjectPool&& other) noexcept {
        if (this != &other) {
            this->~ObjectPool();
            new (this) ObjectPool(std::move(other));
        }
        return *this;
    }

    // Acquire an object from the pool
    // Constructs it with the given arguments
    template <typename... Args>
    T* acquire(Args&&... args) {
        LockGuard guard(lock_);

        T* obj = nullptr;

        if (head_ != nullptr) {
            // Get from free list
            obj = head_;
            head_ = *getNext(obj);
            available_count_--;
        } else {
            // Need to allocate new block
            if (allocateBlock(prealloc_count_) != 0) {
                // Allocation failed
                return nullptr;
            }
            // Try again after allocation
            if (head_ != nullptr) {
                obj = head_;
                head_ = *getNext(obj);
                available_count_--;
            }
        }

        if (obj != nullptr) {
            // Construct the object
            new (obj) T(std::forward<Args>(args)...);
        }

        return obj;
    }

    // Release an object back to the pool
    // Calls destructor before putting back
    void release(T* obj) {
        if (!obj) return;

        LockGuard guard(lock_);

        // Call destructor
        obj->~T();

        // Put back to free list
        setNext(obj, head_);
        head_ = obj;
        available_count_++;
    }

    // Get number of available objects in pool
    size_t available() const {
        return available_count_;
    }

    // Get total number of objects allocated by pool
    size_t allocated() const {
        return allocated_count_;
    }

    // Get number of objects currently in use
    size_t inUse() const {
        return allocated_count_ - available_count_;
    }

private:
    // Memory block structure
    struct Block {
        uint8_t* base;
        size_t object_count;
        size_t total_size;
        Block* next;
    };

    // Lock type selection
    using Lock = typename LockTypeSelector<ThreadSafe>::Lock;
    using LockGuard = typename LockTypeSelector<ThreadSafe>::LockGuard;

    size_t object_size_;
    size_t prealloc_count_;
    size_t allocated_count_;
    size_t available_count_;
    T* head_;      // Free list head
    Block* memory_blocks_;  // List of allocated memory blocks
    mutable Lock lock_;

    // Allocate a new block of objects
    int allocateBlock(size_t count) {
        // Calculate total size, round up to page boundary
        size_t total_bytes = count * object_size_;
        total_bytes = roundUpToPage(total_bytes);

        // Allocate via mmap
        void* ptr = mmap(nullptr, total_bytes, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
            std::cerr << "ObjectPool: mmap failed for " << total_bytes << " bytes" << std::endl;
            return -1;
        }

        // Create block header
        Block* block = new Block();
        block->base = static_cast<uint8_t*>(ptr);
        block->object_count = count;
        block->total_size = total_bytes;
        block->next = memory_blocks_;
        memory_blocks_ = block;

        // Build free list in this block
        for (size_t i = 0; i < count; ++i) {
            T* obj = reinterpret_cast<T*>(block->base + i * object_size_);
            setNext(obj, head_);
            head_ = obj;
        }

        allocated_count_ += count;
        available_count_ += count;

        return 0;
    }

    // Get next pointer from free list node
    // We store the next pointer in the object's memory itself
    T** getNext(T* obj) {
        return reinterpret_cast<T**>(obj);
    }

    // Set next pointer
    void setNext(T* obj, T* next) {
        *getNext(obj) = next;
    }
};

// Convenience typedefs
template <typename T>
using ObjectPoolMT = ObjectPool<T, true>;

}  // namespace infra

#endif  // INFRA_OBJECT_POOL

#ifndef INFRA_ARENA
#define INFRA_ARENA

#include <iostream>
#include <cstdint>
#include <sys/mman.h>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <new>

namespace infra {

// Forward declarations
template <typename T, bool ThreadSafe = false>
class Arena;

// Memory block structure
struct MemoryBlock {
    void* base;
    size_t size;
    size_t used;
    MemoryBlock* next;
    bool is_arena_owner;

    MemoryBlock(void* base, size_t size, bool is_owner = false)
        : base(base), size(size), used(0), next(nullptr), is_arena_owner(is_owner) {}

    size_t available() const { return size - used; }
};

// Simplified slab for small objects - no longer template to avoid bloat
class Slab {
public:
    explicit Slab(size_t object_size, size_t object_count)
        : object_size_(object_size), object_count_(object_count) {
        memory_size_ = ((object_size_ * object_count_) + (kPageSize + 1)) / kPageSize * kPageSize;
        void* ptr = mmap(nullptr, memory_size_, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
            throw std::bad_alloc();
        }

        base_ = static_cast<uint8_t*>(ptr);
        free_count_ = object_count_;
        free_offset_ = 0;

        // Instead of free list, use bit tracking for faster allocation
        free_bits_ = new uint64_t[(object_count_ + 63) / 64];
        memset(free_bits_, 0xFF, (object_count_ + 63) / 64 * sizeof(uint64_t));
    }

    ~Slab() {
        if (base_) {
            munmap(base_, memory_size_);
            delete[] free_bits_;
        }
    }

    void* allocate() {
        if (free_count_ == 0) {
            return nullptr;
        }

        // Find first free slot
        for (size_t i = 0; i < (object_count_ + 63) / 64; ++i) {
            if (free_bits_[i] != 0) {
                // Find set bit
                size_t bit = __builtin_ctzll(free_bits_[i]);
                size_t index = i * 64 + bit;
                if (index < object_count_) {
                    free_bits_[i] &= ~(1ULL << bit);
                    free_count_--;
                    return base_ + index * object_size_;
                }
            }
        }
        return nullptr;
    }

    void deallocate(void* ptr) {
        if (!ptr || !base_) return;

        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t base_addr = reinterpret_cast<uintptr_t>(base_);
        if (addr < base_addr || addr >= base_addr + memory_size_) {
            return;
        }

        size_t index = (addr - base_addr) / object_size_;
        if (index >= object_count_) {
            return;
        }

        size_t word = index / 64;
        size_t bit = index % 64;
        uint64_t mask = 1ULL << bit;

        if (!(free_bits_[word] & mask)) {
            free_bits_[word] |= mask;
            free_count_++;
        }
    }

    void clear() {
        free_count_ = object_count_;
        memset(free_bits_, 0xFF, (object_count_ + 63) / 64 * sizeof(uint64_t));
    }

    bool contains(void* ptr) const {
        if (!ptr || !base_) return false;
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t base_addr = reinterpret_cast<uintptr_t>(base_);
        return addr >= base_addr && addr < base_addr + memory_size_ &&
               (addr - base_addr) % object_size_ == 0;
    }

    bool full() const { return free_count_ == 0; }
    bool empty() const { return free_count_ == object_count_; }
    size_t capacity() const { return object_count_; }
    size_t free() const { return free_count_; }
    size_t allocated() const { return object_count_ - free_count_; }
    size_t object_size() const { return object_size_; }

private:
    static const size_t kPageSize = 4096;
    uint8_t* base_ = nullptr;
    size_t memory_size_ = 0;
    size_t object_size_ = 0;
    size_t object_count_ = 0;
    uint64_t* free_bits_ = nullptr;
    size_t free_count_ = 0;
    size_t free_offset_ = 0;
};

// Arena implementation
template <typename T, bool ThreadSafe>
class Arena {
public:
    static constexpr size_t kSlabSizeThreshold = 128;
    static constexpr size_t kDefaultInitialSize = 1024 * 1024;  // 1MB
    static constexpr size_t kSlabSizes[5] = {8, 16, 32, 64, 128};
    static constexpr size_t kMaxSlabsPerSize = 4;  // Limit number of slabs

    explicit Arena(size_t initial_size = kDefaultInitialSize)
        : initial_size_(initial_size), leak_detection_enabled_(false) {
        if (initial_size_ == 0) {
            throw std::invalid_argument("Arena size must be positive");
        }

        allocate_block(initial_size_);

        // Initialize slabs
        for (size_t size : kSlabSizes) {
            slabs_[size] = new SlabList(size);
        }
    }

    ~Arena() {
        // Deallocate all memory blocks
        MemoryBlock* current = blocks_;
        while (current) {
            MemoryBlock* next = current->next;
            if (current->is_arena_owner) {
                munmap(current->base, current->size);
            }
            delete current;
            current = next;
        }

        // Delete slabs
        for (auto& pair : slabs_) {
            delete pair.second;
        }
    }

    // Disable copying
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    // Enable moving
    Arena(Arena&& other) noexcept {
        lock_guard_type lock(other.mutex_);
        std::swap(blocks_, other.blocks_);
        std::swap(initial_size_, other.initial_size_);
        total_used_ = other.total_used_;
        other.total_used_ = 0;
        std::swap(leak_detection_enabled_, other.leak_detection_enabled_);

        // Move slabs
        slabs_ = std::move(other.slabs_);

        // Clear other's blocks
        other.blocks_ = nullptr;
        other.initial_size_ = kDefaultInitialSize;
        other.total_used_ = 0;
    }

    Arena& operator=(Arena&& other) noexcept {
        if (this != &other) {
            lock_guard_type lock(other.mutex_);
            std::swap(blocks_, other.blocks_);
            std::swap(initial_size_, other.initial_size_);
            total_used_ = other.total_used_;
            other.total_used_ = 0;
            std::swap(leak_detection_enabled_, other.leak_detection_enabled_);

            slabs_ = std::move(other.slabs_);

            other.blocks_ = nullptr;
            other.initial_size_ = kDefaultInitialSize;
            other.total_used_ = 0;
        }
        return *this;
    }

    // Allocate memory
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        if (size == 0) {
            return nullptr;
        }

        // Use slab for small objects
        if (size <= kSlabSizeThreshold) {
            return allocate_from_slab(size);
        }

        // Use arena for large objects
        return allocate_from_arena(size, alignment);
    }

    // Deallocate memory
    void deallocate(void* ptr) {
        if (!ptr) {
            return;
        }

        // Check slabs first
        for (const auto& pair : slabs_) {
            if (pair.second->contains(ptr)) {
                pair.second->deallocate(ptr);
                return;
            }
        }

        // Otherwise, it's a large allocation - no tracking needed
        // Large allocations are deallocated when the arena resets
    }

    // Reset arena (clear all allocations)
    void reset() {
        lock_guard_type lock(mutex_);

        // Reset all blocks
        MemoryBlock* current = blocks_;
        while (current) {
            current->used = 0;
            current = current->next;
        }

        // Clear slab allocations
        for (auto& pair : slabs_) {
            pair.second->clear();
        }

        total_used_ = 0;

        if (leak_detection_enabled_) {
            allocations_.clear();
        }
    }

    // Shrink arena to fit actual usage
    void shrink_to_fit() {
        lock_guard_type lock(mutex_);

        // For now, just release all blocks except the first one
        if (blocks_ && blocks_->next) {
            MemoryBlock* current = blocks_->next;
            while (current) {
                MemoryBlock* next = current->next;
                munmap(current->base, current->size);
                delete current;
                current = next;
            }
            blocks_->next = nullptr;
        }
    }

    // Get arena size
    size_t size() const {
        lock_guard_type lock(mutex_);
        size_t total = 0;
        MemoryBlock* current = blocks_;
        while (current) {
            total += current->size;
            current = current->next;
        }
        return total;
    }

    // Get used memory
    size_t used() const {
        return total_used_;
    }

    // Get available memory
    size_t available() const {
        return size() - used();
    }

    // Enable leak detection
    void enable_leak_detection(bool enable = true) {
        lock_guard_type lock(mutex_);
        leak_detection_enabled_ = enable;
        if (!enable) {
            allocations_.clear();
        }
    }

    // Print memory statistics
    void print_memory_stats() const {
        lock_guard_type lock(mutex_);

        // Calculate total size directly
        size_t total_size = 0;
        MemoryBlock* current = blocks_;
        while (current) {
            total_size += current->size;
            current = current->next;
        }

        std::cout << "Arena Memory Statistics:\n";
        std::cout << "  Total size: " << total_size << " bytes\n";
        std::cout << "  Used: " << total_used_ << " bytes\n";
        std::cout << "  Available: " << (total_size - total_used_) << " bytes\n";
        std::cout << "  Fragmentation: " << (total_size > 0 ? (static_cast<double>(total_size - total_used_) / total_size) * 100.0 : 0.0) << "%\n";

        std::cout << "\nSlab Usage:\n";
        for (const auto& pair : slabs_) {
            const auto& slab = pair.second;
            std::cout << "  " << pair.first << " bytes: "
                      << slab->allocated() << " allocated, "
                      << slab->free() << " free\n";
        }

        if (leak_detection_enabled_) {
            std::cout << "\nActive allocations: " << allocations_.size() << "\n";
        }
    }

private:
    // Empty lock for non-thread-safe version
    class EmptyLock {
    public:
        template <typename U>
        explicit EmptyLock(U&) {}
    };

    using lock_guard_type = typename std::conditional<ThreadSafe,
                                                      std::lock_guard<std::mutex>,
                                                      EmptyLock>::type;

    // Slab list to manage multiple slabs of same size
    class SlabList {
    public:
        explicit SlabList(size_t object_size) : object_size_(object_size) {}

        void add_slab(Slab* slab) {
            if (slabs_.size() < kMaxSlabsPerSize) {
                slabs_.push_back(slab);
            }
        }

        void* allocate() {
            // Try each slab
            for (auto* slab : slabs_) {
                void* ptr = slab->allocate();
                if (ptr) {
                    return ptr;
                }
            }

            // Need new slab
            if (slabs_.size() < kMaxSlabsPerSize) {
                Slab* new_slab = new Slab(object_size_, 256);
                slabs_.push_back(new_slab);
                return new_slab->allocate();
            }

            return nullptr;
        }

        void deallocate(void* ptr) {
            for (auto* slab : slabs_) {
                if (slab->contains(ptr)) {
                    slab->deallocate(ptr);
                    return;
                }
            }
        }

        bool contains(void* ptr) {
            for (auto* slab : slabs_) {
                if (slab->contains(ptr)) {
                    return true;
                }
            }
            return false;
        }

        void clear() {
            for (auto* slab : slabs_) {
                slab->clear();
            }
        }

        size_t allocated() const {
            size_t total = 0;
            for (const auto& slab : slabs_) {
                total += slab->allocated();
            }
            return total;
        }

        size_t free() const {
            size_t total = 0;
            for (const auto& slab : slabs_) {
                total += slab->free();
            }
            return total;
        }

    private:
        std::vector<Slab*> slabs_;
        size_t object_size_;
    };

    // Allocate from slab
    void* allocate_from_slab(size_t size) {
        lock_guard_type lock(mutex_);

        // Find appropriate slab size
        size_t slab_size = size;
        for (size_t s : kSlabSizes) {
            if (size <= s) {
                slab_size = s;
                break;
            }
        }

        void* ptr = slabs_[slab_size]->allocate();
        if (ptr) {
            total_used_ += slab_size;
            if (leak_detection_enabled_) {
                allocations_[ptr] = {size, slab_size};
            }
        }
        return ptr;
    }

    // Allocate from arena
    void* allocate_from_arena(size_t size, size_t alignment) {
        lock_guard_type lock(mutex_);

        // Align size
        size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);

        // Find a block with enough space
        MemoryBlock* block = find_block_with_space(aligned_size);
        if (!block) {
            // Allocate new block
            size_t new_block_size = std::max(initial_size_, aligned_size);
            allocate_block(new_block_size);
            block = blocks_;  // New block is at the front
        }

        // Allocate from the block
        void* ptr = align_pointer(static_cast<uint8_t*>(block->base) + block->used, alignment);
        if (static_cast<uint8_t*>(ptr) + size > static_cast<uint8_t*>(block->base) + block->size) {
            return nullptr;  // Shouldn't happen due to find_block_with_space
        }

        block->used += aligned_size;
        total_used_ += aligned_size;

        if (leak_detection_enabled_) {
            allocations_[ptr] = {size, aligned_size};
        }

        return ptr;
    }

    // Allocate a new memory block
    void allocate_block(size_t size) {
        void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
            throw std::bad_alloc();
        }

        MemoryBlock* new_block = new MemoryBlock(ptr, size, true);
        new_block->next = blocks_;
        blocks_ = new_block;
    }

    // Find block with enough space
    MemoryBlock* find_block_with_space(size_t size) {
        MemoryBlock* current = blocks_;
        while (current) {
            if (current->available() >= size) {
                return current;
            }
            current = current->next;
        }
        return nullptr;
    }

    // Align pointer
    void* align_pointer(void* ptr, size_t alignment) {
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        addr = (addr + alignment - 1) & ~(alignment - 1);
        return reinterpret_cast<void*>(addr);
    }

    // Calculate fragmentation percentage
    double calculate_fragmentation() const {
        if (size() == 0) return 0.0;
        return (static_cast<double>(available()) / size()) * 100.0;
    }

    // Member variables
    MemoryBlock* blocks_ = nullptr;
    size_t initial_size_;
    mutable typename std::conditional<ThreadSafe, std::mutex, char>::type mutex_;
    size_t total_used_ = 0;
    bool leak_detection_enabled_;

    // Slab management
    std::unordered_map<size_t, SlabList*> slabs_;

    // Leak detection
    struct AllocationInfo {
        size_t requested_size;
        size_t actual_size;
    };
    std::unordered_map<void*, AllocationInfo> allocations_;
};

// Common typedefs
using ArenaMT = Arena<char, true>;    // Thread-safe arena
using ArenaST = Arena<char, false>;   // Single-threaded arena

} // namespace infra

#endif // INFRA_ARENA
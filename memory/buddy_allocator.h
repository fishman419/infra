#ifndef INFRA_BUDDY_ALLOCATOR
#define INFRA_BUDDY_ALLOCATOR

#include <iostream>
#include <cstdint>
#include <sys/mman.h>
#include <cstring>
#include <algorithm>
#include "common.h"

namespace infra {

// Buddy memory allocator
// Manages variable-sized allocations within a fixed contiguous memory region
// Uses classic binary buddy algorithm: O(log n) allocation and deallocation
// Best for fixed-size memory pools where fragmentation must be minimized
class BuddyAllocator {
public:
    // Constructor with total memory size and minimum block size
    // total_size: total bytes to manage
    // min_block_size: smallest allocation unit (default 16 bytes)
    explicit BuddyAllocator(size_t total_size, size_t min_block_size = 16)
        : total_size_(roundUpToPage(total_size))
        , min_block_size_(std::max(min_block_size, sizeof(uintptr_t))) {

        // Calculate maximum order
        // max_order = log2(total_size / min_block_size)
        size_t size = total_size_ / min_block_size_;
        max_order_ = 0;
        while (size > 1) {
            size >>= 1;
            max_order_++;
        }

        // Calculate level 0 (largest block) order
        total_order_ = max_order_;

        // Allocate memory
        void* ptr = mmap(nullptr, total_size_, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
            throw std::bad_alloc();
        }
        base_ = static_cast<uint8_t*>(ptr);

        // Allocate free lists and bitmap
        free_lists_ = new uintptr_t[max_order_ + 1];
        // Allocated bits: one bit per min block index
        size_t num_min_blocks = total_size_ / min_block_size_;
        size_t allocated_bitmap_blocks = (num_min_blocks + 63) / 64;
        allocated_bits_ = new uint64_t[allocated_bitmap_blocks];
        // Split bits: one bit per (order, block) - each order has blocks aligned to order
        split_bitmap_offsets_ = new size_t[max_order_ + 1];
        size_t offset = 0;
        for (size_t o = 0; o <= max_order_; ++o) {
            split_bitmap_offsets_[o] = offset;
            size_t num_blocks = num_min_blocks >> o;  // each block has 1<<o min blocks
            size_t num_words = (num_blocks + 63) / 64;
            offset += num_words;
        }
        split_bits_ = new uint64_t[offset];
        // Store order for each allocated block's starting index
        // 0xFF means not allocated
        block_order_ = new uint8_t[num_min_blocks];
        memset(block_order_, 0xFF, sizeof(uint8_t) * num_min_blocks);

        // Initialize all as free
        memset(free_lists_, 0, sizeof(uintptr_t) * (max_order_ + 1));
        memset(allocated_bits_, 0, sizeof(uint64_t) * allocated_bitmap_blocks);
        memset(split_bits_, 0, sizeof(uint64_t) * offset);

        // Add the entire memory as one free block at max_order
        freeListAdd(max_order_, 0);

        used_size_ = 0;
    }

    // Destructor
    ~BuddyAllocator() {
        if (base_) {
            munmap(base_, total_size_);
        }
        delete[] free_lists_;
        delete[] allocated_bits_;
        delete[] split_bits_;
        delete[] split_bitmap_offsets_;
        delete[] block_order_;
    }

    // Disable copy
    BuddyAllocator(const BuddyAllocator&) = delete;
    BuddyAllocator& operator=(const BuddyAllocator&) = delete;

    // Allocate memory of given size
    // Returns nullptr if out of memory
    void* allocate(size_t size) {
        if (size == 0) {
            return nullptr;
        }

        // Calculate required order
        size_t order = getOrderForSize(size);
        if (order > max_order_) {
            return nullptr;
        }

        // Find a free block starting from required order upwards
        size_t current_order = order;
        while (current_order <= max_order_ && free_lists_[current_order] == 0) {
            current_order++;
        }

        if (current_order > max_order_) {
            // No available blocks
            return nullptr;
        }

        // Split blocks until we get to the required order
        while (current_order > order) {
            // Split a block from current_order into two blocks at current_order - 1
            uintptr_t block_index = freeListRemove(current_order);
            splitBitSet(current_order, block_index);  // Mark parent as split
            // Left child starts at block_index (order current_order -1) - it wasn't split, already 0 because we initialize all to 0
            // No need to clear, it's already 0 from initialization
            freeListAdd(current_order - 1, getBuddyIndex(block_index, current_order - 1));
            freeListAdd(current_order - 1, block_index);
            current_order--;
        }

        // Allocate the block
        uintptr_t block_index = freeListRemove(order);
        allocatedBitSet(block_index);
        block_order_[block_index] = static_cast<uint8_t>(order);
        size_t block_size = blockSizeForOrder(order);
        used_size_ += block_size;

        // Calculate address from index
        uint8_t* addr = base_ + block_index * min_block_size_;
        return static_cast<void*>(addr);
    }

    // Deallocate memory previously allocated
    void deallocate(void* ptr) {
        if (!ptr) return;

        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t base_addr = reinterpret_cast<uintptr_t>(base_);

        if (addr < base_addr || addr >= base_addr + total_size_) {
            // Out of range - ignore
            return;
        }

        // Calculate block index
        size_t offset = addr - base_addr;
        uintptr_t block_index = offset / min_block_size_;

        // Find order of the block - we need to find the highest order that contains this index
        int order = getOrderOfBlock(block_index);
        if (order < 0 || !isAllocated(block_index)) {
            // Not allocated - ignore
            return;
        }

        size_t block_size = blockSizeForOrder(static_cast<size_t>(order));
        allocatedBitClear(block_index);
        block_order_[block_index] = 0xFF;
        if (used_size_ >= block_size) {
            used_size_ -= block_size;
        } else {
            used_size_ = 0;
        }

        // Merge with buddy if possible
        while (static_cast<size_t>(order) < max_order_) {
            uintptr_t buddy_index = getBuddyIndex(block_index, static_cast<size_t>(order));
            if (isAllocated(buddy_index) || isSplit(static_cast<size_t>(order), buddy_index)) {
                // Buddy is either allocated or split - can't merge
                break;
            }

            // Remove buddy from free list
            if (!removeFromFreeList(static_cast<int>(order), buddy_index)) {
                // Buddy not found in free list - already merged or allocated, can't merge
                break;
            }

            // Merge to parent
            block_index = block_index & ~(1ULL << order);
            splitBitClear(static_cast<size_t>(order + 1), block_index);
            order++;
        }

        freeListAdd(static_cast<size_t>(order), block_index);
    }

    // Get total size
    size_t totalSize() const {
        return total_size_;
    }

    // Get used size
    size_t usedSize() const {
        return used_size_;
    }

    // Get free size
    size_t freeSize() const {
        return total_size_ - used_size_;
    }

    // Get number of free blocks at given order
    int freeBlocks(int order) const {
        if (order < 0 || static_cast<size_t>(order) > max_order_) {
            return 0;
        }
        int count = 0;
        uintptr_t curr = free_lists_[order];
        while (curr != 0) {
            count++;
            uintptr_t index = curr - 1;
            curr = getNext(index);
        }
        return count;
    }

    // Print statistics
    void printStats() const {
        std::cout << "Buddy Allocator Statistics:\n";
        std::cout << "  Total size:    " << total_size_ << " bytes\n";
        std::cout << "  Used size:     " << used_size_ << " bytes\n";
        std::cout << "  Free size:     " << (total_size_ - used_size_) << " bytes\n";
        std::cout << "  Min block:     " << min_block_size_ << " bytes\n";
        std::cout << "  Max order:     " << max_order_ << " (block size " << (1ULL << max_order_) * min_block_size_ << " bytes)\n";
        std::cout << "  Free blocks by order:\n";
        for (size_t o = 0; o <= max_order_; ++o) {
            int cnt = freeBlocks(static_cast<int>(o));
            if (cnt > 0) {
                std::cout << "    order " << o << " (size " << (1ULL << o) * min_block_size_
                          << "): " << cnt << " blocks\n";
            }
        }
    }

private:
    // Calculate block size for a given order
    size_t blockSizeForOrder(int order) const {
        return (1ULL << order) * min_block_size_;
    }

    // Get required order for a given allocation size
    size_t getOrderForSize(size_t size) const {
        if (size <= min_block_size_) {
            return 0;
        }
        size_t blocks = (size + min_block_size_ - 1) / min_block_size_;
        size_t order = 0;
        while ((1ULL << order) < blocks) {
            order++;
        }
        return order;
    }

    // Get buddy index of a block
    uintptr_t getBuddyIndex(uintptr_t index, int order) const {
        return index ^ (1ULL << order);
    }

    // Get the parent block index
    uintptr_t getParentIndex(uintptr_t index, int order) const {
        return index & ~(1ULL << order);
    }

    // Check if block index is allocated
    bool isAllocated(uintptr_t index) const {
        size_t word = index / 64;
        size_t bit = index % 64;
        return (allocated_bits_[word] & (1ULL << bit)) != 0;
    }

    // Mark block as allocated
    void allocatedBitSet(uintptr_t index) {
        size_t word = index / 64;
        size_t bit = index % 64;
        allocated_bits_[word] |= (1ULL << bit);
    }

    // Mark block as free
    void allocatedBitClear(uintptr_t index) {
        size_t word = index / 64;
        size_t bit = index % 64;
        allocated_bits_[word] &= ~(1ULL << bit);
    }

    // Check if block at (order, index) is split
    bool isSplit(size_t order, uintptr_t index) const {
        // index is the starting min block index
        size_t block_k = index >> order;  // block number within this order
        size_t word_offset = split_bitmap_offsets_[order] + (block_k / 64);
        size_t bit = block_k % 64;
        return (split_bits_[word_offset] & (1ULL << bit)) != 0;
    }

    // Mark block at (order, index) as split
    void splitBitSet(size_t order, uintptr_t index) {
        size_t block_k = index >> order;
        size_t word_offset = split_bitmap_offsets_[order] + (block_k / 64);
        size_t bit = block_k % 64;
        split_bits_[word_offset] |= (1ULL << bit);
    }

    // Mark block at (order, index) as not split
    void splitBitClear(size_t order, uintptr_t index) {
        size_t block_k = index >> order;
        size_t word_offset = split_bitmap_offsets_[order] + (block_k / 64);
        size_t bit = block_k % 64;
        split_bits_[word_offset] &= ~(1ULL << bit);
    }

    // Get next pointer from free list node
    // We store (index + 1), 0 means end of list (since 0 is a valid index)
    uintptr_t getNext(uintptr_t index) const {
        uint8_t* addr = base_ + index * min_block_size_;
        uintptr_t stored = *reinterpret_cast<uintptr_t*>(addr);
        return stored == 0 ? 0 : stored - 1;
    }

    // Set next pointer - store (index + 1), 0 means end
    void setNext(uintptr_t index, uintptr_t next) {
        uint8_t* addr = base_ + index * min_block_size_;
        *reinterpret_cast<uintptr_t*>(addr) = next == 0 ? 0 : next + 1;
    }

    // Add block to free list at given order (add to head)
    // We use 0 as invalid (empty list), so block indices are stored +1 in free_lists_
    void freeListAdd(int order, uintptr_t index) {
        if (free_lists_[order] != 0) {
            // free_lists_[order] stores (index + 1), need to decode to get next index
            uintptr_t next_index = free_lists_[order] - 1;
            setNext(index, next_index);
        } else {
            setNext(index, 0);
        }
        free_lists_[order] = index + 1;
    }

    // Remove and return head from free list
    uintptr_t freeListRemove(int order) {
        uintptr_t index_plus_1 = free_lists_[order];
        uintptr_t index = index_plus_1 - 1;
        uintptr_t next_index = getNext(index);
        free_lists_[order] = next_index == 0 ? 0 : (next_index + 1);
        return index;
    }

    // Remove a specific index from free list
    // Returns true if successfully removed, false if not found
    bool removeFromFreeList(int order, uintptr_t index) {
        if (free_lists_[order] == 0) {
            return false;
        }

        uintptr_t first = free_lists_[order] - 1;
        if (first == index) {
            uintptr_t next_index = getNext(first);
            free_lists_[order] = next_index == 0 ? 0 : (next_index + 1);
            return true;
        }

        uintptr_t prev = first;
        uintptr_t curr = getNext(prev);
        while (curr != 0) {
            if (curr == index) {
                setNext(prev, getNext(curr));
                return true;
            }
            prev = curr;
            curr = getNext(prev);
        }
        // If we get here, didn't find the index
        return false;
    }

    // Find the order of an already allocated block
    int getOrderOfBlock(uintptr_t index) {
        // We store the order directly when we allocate
        if (block_order_[index] == 0xFF) {
            return -1; // Not allocated
        }
        return static_cast<int>(block_order_[index]);
    }

    uint8_t* base_;
    size_t total_size_;
    size_t min_block_size_;
    size_t max_order_;
    size_t total_order_;
    size_t used_size_;
    uintptr_t* free_lists_;
    uint64_t* allocated_bits_;
    uint64_t* split_bits_;
    size_t* split_bitmap_offsets_;
    uint8_t* block_order_;
};

}  // namespace infra

#endif  // INFRA_BUDDY_ALLOCATOR

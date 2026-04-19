#ifndef INFRA_SLAB
#define INFRA_SLAB

#include <iostream>
#include <cstdint>
#include <sys/mman.h>
#include <cstdlib>
#include "common.h"

namespace infra {

template <typename T>
class Slab {
public:
    explicit Slab(uint64_t slab_count) : slab_count_(slab_count) {}

    int init() {
        uint64_t memory_size =
            ((sizeof(T) * slab_count_) + (infra::kPageSize + 1)) /
            infra::kPageSize * infra::kPageSize;
        void* ptr = mmap(nullptr, memory_size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
            std::cout << "mmap failed" << std::endl;
            return -1;
        }
        regions_[0].memory_start = ptr;
        regions_[0].memory_size = memory_size;
        regions_[0].slab_count = slab_count_;
        regions_[0].free_list = (T**)malloc(slab_count_ * sizeof(T*));
        regions_[0].free_count = slab_count_;
        for (uint64_t i = 0; i < slab_count_; ++i) {
            regions_[0].free_list[i] = ((T*)ptr) + i;
        }

        num_regions_ = 1;
        return 0;
    }

    ~Slab() {
        for (uint64_t i = 0; i < num_regions_; ++i) {
            if (regions_[i].memory_start && regions_[i].memory_size) {
                int ret = munmap(regions_[i].memory_start, regions_[i].memory_size);
                if (ret) {
                    std::cout << "munmap failed, ret: " << ret << std::endl;
                }
                free(regions_[i].free_list);
            }
        }
    }

    T* get() {
        T* slab = nullptr;
        auto& region = regions_[0];
        if (region.free_count > 0) {
            slab = region.free_list[region.free_count - 1];
            region.free_list[region.free_count--] = nullptr;
        }
        return slab;
    }

    void put(T* slab) {
        if (slab) {
            auto& region = regions_[0];
            region.free_list[region.free_count++] = slab;
        }
    }

private:
    static const uint64_t kMaxRegion = 8;

    struct MemoryRegion {
        void* memory_start = nullptr;
        uint64_t memory_size = 0;
        uint64_t slab_count = 0;
        T** free_list = nullptr;
        uint64_t free_count = 0;
    };

    MemoryRegion regions_[kMaxRegion];
    uint64_t num_regions_ = 0;
    uint64_t slab_count_ = 0;
};

}  // namespace infra

#endif  // INFRA_SLAB

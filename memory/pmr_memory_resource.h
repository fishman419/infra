#ifndef INFRA_PMR_MEMORY_RESOURCE
#define INFRA_PMR_MEMORY_RESOURCE

#if __cplusplus >= 201703L

#include <memory_resource>
#include "common.h"
#include "arena.h"

namespace infra {

// Adapter to convert any infra allocator to std::pmr::memory_resource
// Allows using infra allocators with C++17 polymorphic allocator (std::pmr) containers
//
// Usage:
//   infra::ArenaST arena;
//   infra::PMRMemoryResource<infra::ArenaST> pmr_arena(&arena);
//   std::pmr::vector<int> vec(&pmr_arena);
//   vec.push_back(42); // Allocates from arena
template <typename Allocator>
class PMRMemoryResource : public std::pmr::memory_resource {
public:
    // Construct with pointer to the underlying allocator
    // Allocator must outlive the memory resource
    explicit PMRMemoryResource(Allocator* allocator)
        : allocator_(allocator) {}

protected:
    // Allocate memory from underlying allocator
    void* do_allocate(size_t bytes, size_t alignment) override {
        return allocator_->allocate(bytes, alignment);
    }

    // Deallocate memory to underlying allocator
    void do_deallocate(void* p, size_t /*bytes*/, size_t /*alignment*/) override {
        allocator_->deallocate(p);
    }

    // Compare for equality - two instances are equal if they point to same allocator
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        const PMRMemoryResource* other_typed = dynamic_cast<const PMRMemoryResource*>(&other);
        if (!other_typed) {
            return false;
        }
        return allocator_ == other_typed->allocator_;
    }

private:
    Allocator* allocator_;
};

// Convenience typedefs for common allocators
typedef PMRMemoryResource<ArenaST> ArenaPMR;
typedef PMRMemoryResource<ArenaMT> ArenaPMRMT;

}  // namespace infra

#endif  // __cplusplus >= 201703L

#endif  // INFRA_PMR_MEMORY_RESOURCE

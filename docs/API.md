# API Documentation - Slab Allocator

## Class: infra::Slab<T>

A template class that provides a slab allocator for objects of type `T`.

### Constructor

```cpp
explicit Slab(uint64_t slab_count);
```

**Parameters:**
- `slab_count`: Number of objects to allocate in the slab

**Description:**
Creates a slab allocator that can manage `slab_count` objects of type `T`.

### Methods

#### `int Init()`

**Returns:**
- `0` on success
- `-1` on failure (e.g., mmap failed)

**Description:**
Initializes the slab allocator by allocating memory and setting up the free list.

#### `T* Get()`

**Returns:**
- Pointer to an object of type `T` from the slab
- `nullptr` if no objects are available

**Description:**
Acquires an object from the slab's free list. The object is uninitialized and must be constructed by the caller if needed.

#### `void Put(T* slab)`

**Parameters:**
- `slab`: Pointer to an object to return to the slab

**Description:**
Returns an object to the slab's free list for reuse. The object should be destroyed by the caller before being returned if necessary.

#### `~Slab()`

**Description:**
Destructor that cleans up all allocated memory using `munmap`.

### Private Members

#### Constants

- `kPageSize`: 4096 bytes (system page size)
- `kMaxRegion`: 8 (maximum number of memory regions)

#### Struct: MemoryRegion

```cpp
struct MemoryRegion {
    void* memory_start = nullptr;     // Start of allocated memory
    uint64_t memory_size = 0;         // Total size of the region
    uint64_t slab_count = 0;          // Number of objects in this region
    T** free_list = nullptr;         // Array of free object pointers
    uint64_t free_count = 0;          // Current count of free objects
};
```

#### Variables

- `regions_[kMaxRegion]`: Array of memory regions
- `num_regions_`: Number of active regions
- `slab_count_`: Total number of objects per region

## Usage Examples

### Basic Usage

```cpp
#include "memory/slab.h"

int main() {
    // Create a slab for 1024 uint32_t objects
    infra::Slab<uint32_t> slab(1024);
    
    // Initialize
    if (slab.Init() == 0) {
        // Get an object
        uint32_t* obj = slab.Get();
        if (obj) {
            // Use the object
            *obj = 42;
            
            // Return it to the slab
            slab.Put(obj);
        }
    }
    return 0;
}
```

### Error Handling

```cpp
infra::Slab<uint32_t> slab(1024);
if (slab.Init() != 0) {
    std::cerr << "Failed to initialize slab" << std::endl;
    return -1;
}

// Get object
uint32_t* obj = slab.Get();
if (!obj) {
    std::cerr << "No available objects in slab" << std::endl;
    return -1;
}
```

## Memory Management

The slab allocator uses `mmap` with `MAP_PRIVATE | MAP_ANONYMOUS` flags to allocate memory. This provides:

- Private memory mappings
- Zero-initialized memory
- Operating system page-level management

All allocated memory is properly freed in the destructor using `munmap`.

## Performance Notes

- Objects are allocated contiguously within each region
- Free list operations are O(1)
- Memory is allocated in page-aligned chunks for efficiency
- No internal fragmentation within a region
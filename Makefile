# Makefile for Infra project

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
SRCDIR = memory
TESTDIR = test
BUILDDIR = build
TARGETS = $(TARGET) $(ARENA_TARGET) $(OBJECT_POOL_TARGET) $(STACK_ALLOCATOR_TARGET) $(BUDDY_ALLOCATOR_TARGET) $(PMR_MEMORY_RESOURCE_TARGET) $(FILE_IO_TARGET) $(EXAMPLES_TARGET)
TARGET = $(BUILDDIR)/slab_test
ARENA_TARGET = $(BUILDDIR)/arena_test
OBJECT_POOL_TARGET = $(BUILDDIR)/object_pool_test
STACK_ALLOCATOR_TARGET = $(BUILDDIR)/stack_allocator_test
BUDDY_ALLOCATOR_TARGET = $(BUILDDIR)/buddy_allocator_test
PMR_MEMORY_RESOURCE_TARGET = $(BUILDDIR)/pmr_memory_resource_test
FILE_IO_TARGET = $(BUILDDIR)/file_io_test
EXAMPLES_TARGET = $(BUILDDIR)/arena_examples
PERF_TARGET = $(BUILDDIR)/arena_performance
SOURCES = $(TESTDIR)/slab_test.cc
ARENA_SOURCES = $(TESTDIR)/arena_test.cc
OBJECT_POOL_SOURCES = $(TESTDIR)/object_pool_test.cc
STACK_ALLOCATOR_SOURCES = $(TESTDIR)/stack_allocator_test.cc
BUDDY_ALLOCATOR_SOURCES = $(TESTDIR)/buddy_allocator_test.cc
PMR_MEMORY_RESOURCE_SOURCES = $(TESTDIR)/pmr_memory_resource_test.cc
FILE_IO_SOURCES = $(TESTDIR)/file_io_test.cc
INCLUDES = -I$(SRCDIR)
EXAMPLES_DIR = examples

.PHONY: all clean run test test-arena test-all

all: $(TARGETS)

# Slab test
$(TARGET): $(SRCDIR)/slab.h $(SOURCES)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SOURCES)

# Arena test
$(ARENA_TARGET): $(SRCDIR)/arena.h $(ARENA_SOURCES)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(ARENA_SOURCES)

# ObjectPool test
$(OBJECT_POOL_TARGET): $(SRCDIR)/object_pool.h $(OBJECT_POOL_SOURCES)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -pthread -o $@ $(OBJECT_POOL_SOURCES)

# StackAllocator test
$(STACK_ALLOCATOR_TARGET): $(SRCDIR)/stack_allocator.h $(STACK_ALLOCATOR_SOURCES)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -pthread -o $@ $(STACK_ALLOCATOR_SOURCES)

# BuddyAllocator test
$(BUDDY_ALLOCATOR_TARGET): $(SRCDIR)/buddy_allocator.h $(BUDDY_ALLOCATOR_SOURCES)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(BUDDY_ALLOCATOR_SOURCES)

# PMRMemoryResource test (requires C++17, will skip silently if C++17 not available)
$(PMR_MEMORY_RESOURCE_TARGET): $(SRCDIR)/pmr_memory_resource.h $(SRCDIR)/arena.h $(PMR_MEMORY_RESOURCE_SOURCES)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -std=c++17 $(INCLUDES) -o $@ $(PMR_MEMORY_RESOURCE_SOURCES)

# FileIO test
$(FILE_IO_TARGET): $(SRCDIR)/file_io.h $(FILE_IO_SOURCES)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(FILE_IO_SOURCES)

# Examples
EXAMPLES_TARGET = $(BUILDDIR)/arena_examples
$(EXAMPLES_TARGET): $(EXAMPLES_DIR)/arena_examples.cpp $(SRCDIR)/arena.h $(SRCDIR)/slab.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -I$(EXAMPLES_DIR) -o $@ $(EXAMPLES_DIR)/arena_examples.cpp

# Performance test
PERF_TARGET = $(BUILDDIR)/arena_performance
$(PERF_TARGET): $(EXAMPLES_DIR)/arena_performance.cpp $(SRCDIR)/arena.h $(SRCDIR)/slab.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(SRCDIR) -I$(EXAMPLES_DIR) -o $@ $(EXAMPLES_DIR)/arena_performance.cpp

clean:
	rm -rf $(BUILDDIR)

# Run individual tests
run: $(TARGET)
	$(TARGET)

run-arena: $(ARENA_TARGET)
	$(ARENA_TARGET)

run-examples: $(EXAMPLES_TARGET)
	$(EXAMPLES_TARGET)

run-performance: $(PERF_TARGET)
	$(PERF_TARGET)

# Test commands
test: $(TARGET)
	$(TARGET) && echo "Slab test completed successfully"

test-arena: $(ARENA_TARGET)
	$(ARENA_TARGET) && echo "Arena test completed successfully"

test-object-pool: $(OBJECT_POOL_TARGET)
	$(OBJECT_POOL_TARGET) && echo "ObjectPool test completed successfully"

test-stack-allocator: $(STACK_ALLOCATOR_TARGET)
	$(STACK_ALLOCATOR_TARGET) && echo "StackAllocator test completed successfully"

test-buddy-allocator: $(BUDDY_ALLOCATOR_TARGET)
	$(BUDDY_ALLOCATOR_TARGET) && echo "BuddyAllocator test completed successfully"

test-pmr: $(PMR_MEMORY_RESOURCE_TARGET)
	$(PMR_MEMORY_RESOURCE_TARGET) && echo "PMRMemoryResource test completed successfully"

test-file-io: $(FILE_IO_TARGET)
	$(FILE_IO_TARGET) && echo "FileIO test completed successfully"

test-performance: $(PERF_TARGET)
	$(PERF_TARGET) && echo "Performance test completed successfully"

test-all: test test-arena test-object-pool test-stack-allocator test-buddy-allocator test-pmr test-file-io run-examples
	echo "All tests completed successfully"
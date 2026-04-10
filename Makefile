# Makefile for Infra project

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
SRCDIR = memory
TESTDIR = test
BUILDDIR = build
TARGETS = $(TARGET) $(ARENA_TARGET) $(OBJECT_POOL_TARGET) $(STACK_ALLOCATOR_TARGET) $(EXAMPLES_TARGET)
TARGET = $(BUILDDIR)/slab_test
ARENA_TARGET = $(BUILDDIR)/arena_test
OBJECT_POOL_TARGET = $(BUILDDIR)/object_pool_test
STACK_ALLOCATOR_TARGET = $(BUILDDIR)/stack_allocator_test
EXAMPLES_TARGET = $(BUILDDIR)/arena_examples
PERF_TARGET = $(BUILDDIR)/arena_performance
SOURCES = $(TESTDIR)/slab_test.cc
ARENA_SOURCES = $(TESTDIR)/arena_test.cc
OBJECT_POOL_SOURCES = $(TESTDIR)/object_pool_test.cc
STACK_ALLOCATOR_SOURCES = $(TESTDIR)/stack_allocator_test.cc
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

test-performance: $(PERF_TARGET)
	$(PERF_TARGET) && echo "Performance test completed successfully"

test-all: test test-arena test-object-pool test-stack-allocator run-examples
	echo "All tests completed successfully"
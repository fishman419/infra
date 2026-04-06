# Makefile for Infra project

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
SRCDIR = memory
BUILDDIR = build
TARGETS = slab_test arena_test examples
TARGET = $(BUILDDIR)/slab_test
ARENA_TARGET = $(BUILDDIR)/arena_test
EXAMPLES_TARGET = $(BUILDDIR)/examples
SOURCES = $(SRCDIR)/slab_test.cc
ARENA_SOURCES = $(SRCDIR)/arena_test.cc
EXAMPLE_SOURCES = $(SRCDIR)/slab.h $(SRCDIR)/arena.h $(EXAMPLES_DIR)/arena_usage.cpp
INCLUDES = -I$(SRCDIR)
EXAMPLES_DIR = examples

.PHONY: all clean run test test-arena test-all

all: $(TARGETS)

# Slab test
$(TARGET): $(SRCDIR)/slab.h $(SOURCES)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SOURCES)

# Arena test
$(ARENA_TARGET): $(SRCDIR)/arena.h $(SRCDIR)/arena_test.cc
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(ARENA_SOURCES)

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

test-quick-arena: $(QUICK_ARENA_TARGET)
	$(QUICK_ARENA_TARGET) && echo "Quick arena test completed successfully"

test-simple-arena: $(SIMPLE_ARENA_TARGET)
	$(SIMPLE_ARENA_TARGET) && echo "Simple arena test completed successfully"

test-performance: $(PERF_TARGET)
	$(PERF_TARGET) && echo "Performance test completed successfully"

test-all: test test-arena test-performance run-examples
	echo "All tests completed successfully"
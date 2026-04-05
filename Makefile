# Makefile for Infra project

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2
SRCDIR = memory
BUILDDIR = build
TARGET = $(BUILDDIR)/slab_test
SOURCES = $(SRCDIR)/slab_test.cc
INCLUDES = -I$(SRCDIR)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCDIR)/slab.h $(SOURCES)
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SOURCES)

clean:
	rm -rf $(BUILDDIR)

run: $(TARGET)
	$(TARGET)

test: $(TARGET)
	$(TARGET) && echo "Test completed successfully"
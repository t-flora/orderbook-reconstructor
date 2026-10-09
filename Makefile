CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -Isource -MMD -MP
TARGET = reconstructor_TiagoFlora
SRCDIR = source
TESTDIR = tests
BUILDDIR = build
BINARY = $(BUILDDIR)/$(TARGET)
TEST_BINARY = $(BUILDDIR)/test_examples
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SOURCES))
# Exclude main.cpp for library objects used in tests
LIB_SOURCES = $(filter-out $(SRCDIR)/main.cpp,$(SOURCES))
LIB_OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(LIB_SOURCES))

all: $(BINARY)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BINARY): $(OBJECTS) | $(BUILDDIR)
	$(CXX) $(OBJECTS) -o $(BINARY)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Test compilation
$(TEST_BINARY): $(LIB_OBJECTS) $(BUILDDIR)/test_examples.o | $(BUILDDIR)
	$(CXX) $(LIB_OBJECTS) $(BUILDDIR)/test_examples.o -o $(TEST_BINARY)

$(BUILDDIR)/test_examples.o: $(TESTDIR)/test_examples.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(BINARY) $(TEST_BINARY)
	./$(TEST_BINARY)

test_examples: $(TEST_BINARY)
	./$(TEST_BINARY)

# Rebuild objects when included headers change
-include $(OBJECTS:.o=.d) $(BUILDDIR)/test_examples.d

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean test tests test_examples

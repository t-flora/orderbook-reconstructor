CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -Isource
TARGET = reconstructor_TiagoFlora
SRCDIR = source
BUILDDIR = build
BINARY = $(BUILDDIR)/$(TARGET)
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SOURCES))

all: $(BINARY)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BINARY): $(OBJECTS) | $(BUILDDIR)
	$(CXX) $(OBJECTS) -o $(BINARY)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean
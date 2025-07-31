CXX = g++
CXXFLAGS = -std=c++14 -O3 -Wall -Wextra -Isource
TARGET = reconstructor_TiagoFlora
SRCDIR = source
BUILDDIR = build
BINARY = $(BUILDDIR)/$(TARGET)
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BINARY): $(OBJECTS) | $(BUILDDIR)
	$(CXX) $(OBJECTS) -o $(BINARY)

%.o: %.cpp # Target rule for object files
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(BINARY)

.PHONY: clean
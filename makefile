# General compiler arguments
CXX = g++
 #CXXFLAGS = -g -fstandalone-debug -O0 -Wall -Wextra -pedantic-errors -std=c++20 -stdlib=libc++
CXXFLAGS = -O3 -Wall -Wextra -pedantic-errors -std=c++20

# Files to compile
sources := $(wildcard src/*.cpp)
headers := $(wildcard src/*.hpp)
install_dir := /usr/bin

# Files to create
objects := $(sources:src/%.cpp=obj/%.o)
binary := ./bin/quickbuild

# Main target
quickbuild: setup $(objects) $(headers)
	$(CXX) $(CXXFLAGS) -o $(binary) $(objects)

# Object files
obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Setup the build directories
setup:
	mkdir -p bin obj

install:
	install -m 755 $(binary) $(install_dir)

# Run
run: quickbuild
	$(binary)

# Clean
clean:
	rm --force $(objects)
	rm --force $(binary)
	rm --force --recursive bin
	rm --force --recursive obj

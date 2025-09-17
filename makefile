# General compiler arguments
CXX = g++
# CXXFLAGS = -g -O0 -Wall -Wextra -pedantic-errors -std=c++20
CXXFLAGS = -O3 -Wall -Wextra -pedantic-errors -std=c++20

# Files to compile
sources := $(wildcard src/*.cpp) $(wildcard src/*/*.cpp)
headers := $(wildcard src/*.hpp)
install_dir := /usr/bin

# Files to create
objects := $(sources:src/%.cpp=obj/release/%.o)
binary := ./bin/qvickbuild

# Main target
qvickbuild: setup $(objects) $(headers)
	$(CXX) $(CXXFLAGS) -o $(binary) $(objects)

# Object files
obj/release/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Setup the build directories
setup:
	mkdir -p bin obj/release obj/debug
	cd ./src && find . -type d -exec mkdir -p -- ../obj/release/{} \;

install:
	install -m 755 $(binary) $(install_dir)

# Run
run: qvickbuild
	$(binary)

# Clean
clean:
	rm --force $(objects)
	rm --force $(binary)
	rm --force --recursive bin
	rm --force --recursive obj

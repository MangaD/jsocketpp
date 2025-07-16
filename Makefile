# Platform-specific configuration
ifeq ($(OS),Windows_NT)
    CMD = cmd /C
    FIND = powershell -Command "Get-ChildItem -Recurse -Include *.cpp,*.hpp src, include\jsocketpp, test | ForEach-Object { $_.FullName }"
else
    CMD =
    FIND = find src include/jsocketpp test -name '*.cpp' -o -name '*.hpp'
endif

# Compiler and tools
CXX          ?= g++
AR           = ar
ARFLAGS      = rcvs
CLANG_FORMAT ?= clang-format

# Directories
SRC_FOLDER   = src
BUILD        = release
ARCH         ?= x64
GTEST_DIR    ?= /usr/local

# Default architecture flags
ifeq ($(ARCH),x64)
    ARCHITECTURE = -m64
else ifeq ($(ARCH),x86)
    ARCHITECTURE = -m32
else
    $(error Unsupported architecture $(ARCH))
endif

# Source files (using platform-specific find command)
SRC := $(shell $(FIND))

# Preprocessor defines
DEFINES =

# Compiler flags
ifeq ($(BUILD),release)
    OPTIMIZE = -O3 -s -DNDEBUG
else
    OPTIMIZE = -O0 -g -DDEBUG -fsanitize=undefined -fsanitize=thread
    WARNINGS = -Wall -Wextra -pedantic -Wmain -Weffc++ -Wswitch-default -Wswitch-enum \
               -Wmissing-include-dirs -Wmissing-declarations -Wunreachable-code -Winline \
               -Wfloat-equal -Wundef -Wcast-align -Wredundant-decls -Winit-self -Wshadow \
               -Wnon-virtual-dtor -Wconversion
    ifeq ($(CXX),g++)
        WARNINGS += -Wzero-as-null-pointer-constant
    endif
endif

# Combine all flags
CXXFLAGS = $(ARCHITECTURE) -std=c++20 $(DEFINES) $(WARNINGS) $(OPTIMIZE)

# GoogleTest flags (requires GTest installed or built separately)
GTEST_FLAGS = -I$(GTEST_DIR)/include -L$(GTEST_DIR)/lib -lgtest -lgtest_main -lpthread

.PHONY: all clean debug release release32 release64 debug32 debug64 tests format

# Default target (build everything)
all: src tests

# Build the source directory
src:
	$(MAKE) -C src

# Build the test directory
tests:
	$(MAKE) -C tests

# Clean the build
clean:
	$(MAKE) -C src clean
	$(MAKE) -C tests clean

# Build in debug mode
debug:
	$(MAKE) BUILD=debug

# Build in release mode
release:
	$(MAKE) BUILD=release

# Build for 32-bit release
release32:
	$(MAKE) BUILD=release ARCH=x86

# Build for 64-bit release
release64:
	$(MAKE) BUILD=release ARCH=x64

# Build for 32-bit debug
debug32:
	$(MAKE) BUILD=debug ARCH=x86

# Build for 64-bit debug
debug64:
	$(MAKE) BUILD=debug ARCH=x64

# Run clang-format on the source files
format:
	$(CLANG_FORMAT) -i --style=file $(SRC)

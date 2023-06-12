# Library: Socket
# Makefile created by MangaD

# Notes:
#  - If 'cmd /C' does not work, replace with 'cmd //C'

CMD = cmd /C

CXX    ?= g++
AR      = ar
ARFLAGS = rcvs
SRC_FOLDER = src
# By default, we build for release
BUILD=release

DEFINES=

ifneq ($(BUILD),release)
WARNINGS = -Wall -Wextra -pedantic -Wmain -Weffc++ -Wswitch-default \
	-Wswitch-enum -Wmissing-include-dirs -Wmissing-declarations -Wunreachable-code -Winline \
	-Wfloat-equal -Wundef -Wcast-align -Wredundant-decls -Winit-self -Wshadow -Wnon-virtual-dtor \
	-Wconversion
	
ifeq ($(CXX),g++)
WARNINGS += -Wzero-as-null-pointer-constant
endif
endif

ifeq ($(BUILD),release)
OPTIMIZE = -O3 -s -DNDEBUG
else
OPTIMIZE = -O0 -g -DDEBUG -fsanitize=undefined -fsanitize=thread
endif

# x64 or x86
ifeq ($(ARCH),x64)
ARCHITECTURE = -m64
else ifeq ($(ARCH),x86)
ARCHITECTURE = -m32
endif

CXXFLAGS = $(ARCHITECTURE) -std=c++11 $(DEFINES) $(WARNINGS) $(OPTIMIZE)

.PHONY: all clean debug release release32 release64 debug32 debug64

all: libsocket.a

libsocket.a: socket.o
	$(AR) $(ARFLAGS) $@ $?
#$(AR) $(ARFLAGS) libsocket.a socket.o
#ranlib libsocket.a

socket.o: $(SRC_FOLDER)/socket.cpp $(SRC_FOLDER)/socket.hpp
	$(CXX) -c $< -o $@ $(CXXFLAGS)
#$(CXX) -c socket.cpp -o socket.o $(CXXFLAGS)

debug:
	$(MAKE) BUILD=debug
release:
	$(MAKE) BUILD=release
release32:
	$(MAKE) BUILD=release ARCH=x64
release64:
	$(MAKE) BUILD=release ARCH=x64
debug32:
	$(MAKE) BUILD=debug ARCH=x86
debug64:
	$(MAKE) BUILD=debug ARCH=x64

clean:
ifeq ($(OS),Windows_NT)
	$(CMD) "del *.o *.a" 2>nul
else
	rm -f *.o *.a
endif
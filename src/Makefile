# src/Makefile - Build the jsocketpp library (static and shared)

CXX ?= g++
AR = ar
ARFLAGS = rcvs
CXXFLAGS ?= -std=c++17 -O2 -Wall -I.
SRC = socket.cpp
LIB_STATIC = jsocketpp.a
LIB_SHARED = jsocketpp.so

ifeq ($(OS),Windows_NT)
    LIB_SHARED = jsocketpp.dll
    LDFLAGS = -lws2_32 -liphlpapi
else
    LDFLAGS =
endif

# Default target is to build the static library
all: $(LIB_STATIC)

# Build static library
$(LIB_STATIC): $(SRC)
	$(CXX) $(CXXFLAGS) -c $(SRC) -o socket.o
	$(AR) $(ARFLAGS) $@ socket.o

# Build shared library
shared: $(SRC)
	$(CXX) $(CXXFLAGS) -fPIC -shared -o $(LIB_SHARED) $(SRC) $(LDFLAGS)

# Clean up object files, static/shared libraries
clean:
	rm -f *.o $(LIB_STATIC) $(LIB_SHARED)

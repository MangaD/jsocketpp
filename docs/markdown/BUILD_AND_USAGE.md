# jsocketpp: Build, Test, and Usage Guide

<!--!
\defgroup BUILD_AND_USAGE jsocketpp: Build, Test, and Usage Guide
\ingroup docs
\hidegroupgraph
[TOC]
-->

This document provides comprehensive instructions for building, testing, and using the `jsocketpp` C++20 cross-platform
socket library on Windows, Linux, and macOS. It covers CMake, Makefile, and manual build approaches, as well as usage
examples and troubleshooting tips.

---

## Table of Contents

- [Prerequisites](#prerequisites)
- [Building the Library](#building-the-library)
    - [Using CMake Presets (Recommended)](#using-cmake-presets-recommended)
    - [Manual CMake Build](#manual-cmake-build)
    - [Using the Makefile](#using-the-makefile)
    - [Manual Compilation](#manual-compilation)
- [Building and Running Tests](#building-and-running-tests)
- [Linking the Library](#linking-the-library)
- [Usage Example](#usage-example)
- [CMake Integration in Your Project](#cmake-integration-in-your-project)
- [Troubleshooting](#troubleshooting)
- [Platform Notes](#platform-notes)
- [License](#license)

---

## Prerequisites

- **C++20 compiler** (MSVC, GCC, or Clang)
- **CMake 3.19+** (for presets support)
- **Ninja** (recommended for fast builds, or use Visual Studio/MSBuild on Windows)
- **GoogleTest** (automatically fetched for unit tests)
- **Git** (for fetching dependencies)

---

## Building the Library

### Using CMake Presets (Recommended)

This project provides a `CMakePresets.json` file for easy, reproducible builds across platforms and configurations.

1. **Configure the project:**
   ```sh
   cmake --preset=default
   # or for a shared library:
   cmake --preset=release-shared
   # or for MSVC:
   cmake --preset=msvc-release
   # or for Clang/GCC:
   cmake --preset=clang-release
   ```

2. **Build the library and all targets:**
   ```sh
   cmake --build --preset=default
   # or use the same preset as above
   ```

3. **Install (optional):**
   ```sh
   cmake --install --preset=default
   ```

### Manual CMake Build

```sh
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

To build a shared library:

```sh
cmake .. -DBUILD_SHARED_LIBS=ON
```

### Using the Makefile

A GNU Makefile is provided for quick builds (mainly for Linux/macOS):

```sh
make           # Builds static lib, client, server
make test      # Builds and runs GoogleTest unit tests
make clean     # Cleans build artifacts
```

### Manual Compilation

You can also build the library manually:

```sh
g++ -std=c++20 -O2 -Wall -I./src -c src/socket.cpp -o socket.o
ar rcs jsocketpp.a socket.o
```

---

## Building and Running Tests

- **With CMake:**
  ```sh
  cmake --build --preset=default
  ctest --preset=default
  # Or run the test executable directly:
  ./build/default-release/socket_gtest
  ```
- **With Makefile:**
  ```sh
  make test
  ```
- **Test executables:**
    - `socket_gtest` (unit tests)
    - `client` and `server` (feature tests)

---

## Linking the Library

- **Static library:** Link against `jsocketpp.a` (or `jsocketpp.lib` on Windows)
- **Shared library:** Link against `jsocketpp.so`/`jsocketpp.dll`/`jsocketpp.dylib` as appropriate
- **Windows:** Also link against `ws2_32.lib` and `iphlpapi.lib`
- **Linux/POSIX:** No extra libraries needed

---

## Usage Example

````cpp
#include "socket.hpp"

int main() {
    jsocketpp::SocketInitializer init; // RAII for WSAStartup/WSACleanup on Windows
    jsocketpp::Socket client("example.com", 80);
    client.connect();
    client.write("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n");
    std::string response = client.read<std::string>();
    std::cout << response << std::endl;
}
````

See the `tests/` directory for more complete TCP, UDP, and Unix domain socket client/server examples.

---

## CMake Integration in Your Project

If you want to use `jsocketpp` in your own CMake project after installing:

```cmake
find_package(jsocketpp REQUIRED)
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE jsocketpp::jsocketpp)
```

Or, if using as a subdirectory:

```cmake
add_subdirectory(path/to/jsocketpp)
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE jsocketpp)
```

---

## Troubleshooting

- **Cannot find `<sys/un.h>` on Windows:** This is expected; Unix domain sockets are only available on POSIX or Windows
  10+ with AF_UNIX support.
- **Link errors for `ws2_32` or `iphlpapi`:** Ensure you link these libraries on Windows.
- **CMake version errors:** Use CMake 3.19 or newer for preset support.
- **Build in-source error:** Always build in a separate directory (out-of-source build).
- **GoogleTest not found:** The build will fetch and build GoogleTest automatically.

---

## Platform Notes

- **Windows:**
    - Requires linking with `ws2_32` and `iphlpapi`.
    - Unix domain sockets require Windows 10 (1803+) and Visual Studio 2019+.
- **Linux/macOS:**
    - Full support for TCP, UDP, and Unix domain sockets.
- **Cross-compiling:**
    - Use the appropriate CMake preset or set toolchain variables manually.

---

## License

MIT License. See [LICENSE](../LICENSE) for details.

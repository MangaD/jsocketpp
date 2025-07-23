# jsocketpp: Java-Style Cross-Platform Socket Library for Modern C++20

<!--! [TOC] -->
<!--
- TOC should be placed after the header
- https://www.doxygen.nl/manual/markdown.html#md_toc
- https://www.doxygen.nl/manual/htmlcmds.html#htmltagcmds
-->

[![Build Status](https://img.shields.io/github/actions/workflow/status/MangaD/jsocketpp/ci.yml)](https://github.com/MangaD/jsocketpp/actions)
[![GH Doxygen](https://github.com/MangaD/jsocketpp/actions/workflows/doxygen-gh-pages.yml/badge.svg)](https://github.com/MangaD/jsocketpp/actions/workflows/doxygen-gh-pages.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](./LICENSE)
[![Conan](https://img.shields.io/badge/conan-available-brightgreen)](https://conan.io/center/jsocketpp)
[![vcpkg](https://img.shields.io/badge/vcpkg-available-blue)](https://vcpkg.io/en/packages.html#jsocketpp)

> **A modern, cross-platform socket library for C++20 — modeled after Java's networking API, but written with clean,
idiomatic, exception-safe C++.**

<div align="center">
  <img src="docs/doxygen/logo.png" width="180" alt="jsocketpp logo">
</div>

---

## 📘 Overview

**jsocketpp** brings the simplicity of Java's `Socket`, `ServerSocket`, and `DatagramSocket` APIs to modern C++20 — with
full support for TCP, UDP, multicast, and UNIX domain sockets, on both Windows and Unix-like systems.

It abstracts away:

- platform-specific quirks (Winsock vs BSD)
- low-level socket boilerplate
- `select()` and `fcntl()` logic
- raw buffer juggling

... and replaces them with a safe, portable, **object-oriented** API that’s easy to use and hard to misuse.

### 🧰 Typical Use Cases

- Minimalist web servers or microservices (HTTP/TCP)
- CLI tools for socket testing or diagnostics
- Inter-process communication (IPC) with UNIX domain sockets
- UDP-based discovery and messaging protocols
- Multicast streaming or group communication
- Socket wrappers for game servers or simulation engines

---

## 💡 Why jsocketpp?

C++ networking is traditionally low-level and error-prone. `jsocketpp` is designed to:

- Save you from boilerplate (`socket`, `bind`, `listen`, `accept`, `recv`...)
- Eliminate raw pointer management and platform-specific checks
- Encourage RAII, strong exception safety, and readable code
- Support modern build tools (CMake, Conan, vcpkg)
- Let you write servers and clients in **under 10 lines**

---

## ✨ Features at a Glance

- 📦 **TCP, UDP, Multicast, and UNIX socket support**
- 📶 **IPv4, IPv6, and dual-stack** support (automatic when possible)
- 🔁 Blocking, non-blocking, and **timeout-enabled I/O**
- 📬 Buffered and typed `read<T>()` methods
- 🔄 `acceptAsync()` and `tryAccept()` for non-blocking server loops
- ✅ `Socket::isConnected()` to check peer connection state
- 🎯 Java-inspired classes:
    - `Socket`, `ServerSocket`, `DatagramSocket`, `MulticastSocket`, `UnixSocket`
- 🔒 **Exception-safe** by design (no manual cleanup needed)
- 🧪 Unit tested with GoogleTest
- ⚙️ Packaged for **Conan**, **vcpkg**, and **CMake** consumers
- 🪟🧬 **Cross-platform**: Windows, Linux, macOS

---

## 🔧 Platform & Toolchain Support

| Feature          | Support                          |
|------------------|----------------------------------|
| C++ Standard     | C++20                            |
| OS Support       | Linux, Windows (Win10+), macOS   |
| Compilers        | GCC ≥ 10, Clang ≥ 11, MSVC 2019+ |
| Package Managers | Conan, vcpkg                     |
| Build System     | CMake ≥ 3.19 (presets.json)      |
| Documentation    | Doxygen + markdown               |

---

## 🔁 jsocketpp vs Boost.Asio

| Feature           | jsocketpp                           | Boost.Asio                            |
|-------------------|-------------------------------------|---------------------------------------|
| API style         | Java-style OOP                      | Callback-driven / coroutine / reactor |
| Dependencies      | None (header + source only)         | Requires Boost (or standalone Asio)   |
| Asynchronous I/O  | Non-blocking, timeout, async accept | Full async/reactor model              |
| Learning curve    | Low                                 | Moderate to steep                     |
| Coroutine support | Planned                             | ✅ Full support (C++20 coroutines)     |
| Readability       | ✅ Very high (simple methods)        | Can be verbose                        |
| Performance       | Near-native for blocking I/O        | Optimal with async + threads          |

**Use `jsocketpp`** when you want:

- Clean and portable blocking I/O
- Simple, readable code
- Great for CLI tools, daemons, test utilities, game servers, educational use

---

## 🚀 Performance & Footprint

- No runtime overhead — it's just modern C++ around system calls
- All socket I/O maps directly to `recv`, `send`, `recvfrom`, etc.
- Internal buffering is efficient (default: 4 KB)
- No heap allocations during read/write (unless resizing)

For high-throughput async workloads, Boost.Asio or io_uring may outperform it. For most real-world usage, **jsocketpp is
more than fast enough**.

---

## ⚠️ Limitations

- **Not thread-safe**: Each socket instance must be used from a single thread
- No coroutine API (`co_await`) — async support is currently based on `select()` with callbacks/futures

- No SSL/TLS (consider wrapping with OpenSSL or mbedTLS externally)
- No epoll/kqueue/io_uring integration (yet)
- Does not auto-retry on `EINTR` or transient errors (manual retry loop if needed)
- Limited to host-native socket support (`AF_UNIX` on Win10+ only)

---

## 📦 Installation

jsocketpp can be installed via [vcpkg](https://vcpkg.io), [Conan](https://conan.io/center), or manually as a CMake
subproject.

### ✅ vcpkg

```sh
vcpkg install jsocketpp
```

Then link via `find_package(jsocketpp CONFIG REQUIRED)` in CMake.

---

### ✅ Conan

```sh
conan install jsocketpp/[latest]@
```

You may also add `jsocketpp` to your `conanfile.txt` or `conanfile.py`.

---

### 🔧 Manual Integration

```sh
git clone https://github.com/MangaD/jsocketpp.git
cd jsocketpp
mkdir build && cd build
cmake ..
make
sudo make install
```

Or add the `src/` folder to your CMake project:

```cmake
add_subdirectory(jsocketpp)
target_link_libraries(myapp PRIVATE jsocketpp)
```

---

## 🚀 Quick Start Examples

### 🧵 TCP Echo Server

```cpp
#include <jsocketpp/ServerSocket.hpp>
#include <jsocketpp/Socket.hpp>

int main() {
    jsocketpp::ServerSocket server(8080); // binds and listens

    while (true) {
        auto client = server.accept(); // blocks (or times out)
        std::string msg = client.read<std::string>();
        client.write("Echo: " + msg);
    }
}
```

---

### 📡 UDP Echo Server

```cpp
#include <jsocketpp/DatagramSocket.hpp>
#include <jsocketpp/DatagramPacket.hpp>

int main() {
    jsocketpp::DatagramSocket socket(9000);
    jsocketpp::DatagramPacket packet(1024);

    while (true) {
        socket.read(packet);        // fills sender info
        socket.write(packet);       // echoes back to sender
    }
}
```

---

### 🌐 Multicast Receiver

```cpp
#include <jsocketpp/MulticastSocket.hpp>
#include <iostream>

int main() {
    jsocketpp::MulticastSocket sock(5000);
    sock.joinGroup("239.255.0.1");

    jsocketpp::DatagramPacket pkt(2048);
    sock.read(pkt);
    std::cout << "Got multicast: " << std::string(pkt.buffer.begin(), pkt.buffer.end()) << std::endl;
}
```

---

### 🧷 UNIX Domain Socket (IPC)

```cpp
#include <jsocketpp/UnixSocket.hpp>

int main() {
    jsocketpp::UnixSocket server("/tmp/echo.sock");
    server.bind();
    server.listen();

    auto client = server.accept();
    std::string msg = client.read<std::string>();
    client.write("Echo from UNIX socket: " + msg);
}
```

> On Windows, AF\_UNIX requires Windows 10 build 1803 or later.

---

## 🔄 Java ↔ jsocketpp Class Mapping

| Java Class               | jsocketpp Equivalent                |
|--------------------------|-------------------------------------|
| `Socket`                 | `jsocketpp::Socket`                 |
| `ServerSocket`           | `jsocketpp::ServerSocket`           |
| `DatagramSocket`         | `jsocketpp::DatagramSocket`         |
| `DatagramPacket`         | `jsocketpp::DatagramPacket`         |
| `MulticastSocket`        | `jsocketpp::MulticastSocket`        |
| `SocketException`        | `jsocketpp::SocketException`        |
| `SocketTimeoutException` | `jsocketpp::SocketTimeoutException` |

---

## 📚 Documentation

Full API documentation is available as Doxygen-generated HTML:

👉 [📖 jsocketpp API Reference (GitHub Pages)](https://github.com/MangaD/jsocketpp/wiki)

You can also generate it locally:

```sh
cmake -S . -B build -DBUILD_DOCS=ON
cmake --build build --target docs
````

The generated HTML will appear in `docs/doxygen/html/index.html`.

---

## 🧪 Building and Testing

The project uses **CMake presets** for consistent and cross-platform builds:

```sh
cmake --preset=gcc-debug-x64     # or clang-debug-x64 / msvc-x64-static
cmake --build --preset=gcc-debug-x64
ctest --preset=gcc-debug-x64
```

Presets are defined in [CMakePresets.json](./CMakePresets.json) and include configurations for:

* GCC, Clang, MSVC, MinGW
* x64 and ARM64
* Debug/Release/RelWithDebInfo
* Shared and static builds
* CI workflows for GitHub Actions

Tests are written with **GoogleTest**. To run them:

```sh
ctest --preset=gcc-debug-x64
```

---

## 🛠 Developer Tooling

### Pre-commit hooks

The repository supports pre-commit checks:

* `clang-format` (via `.clang-format`)
* `cmake-format` (via `.cmake-format.yaml`)
* Header guards, whitespace, and YAML checks

Set up with:

```sh
pip install -r requirements.txt
pre-commit install
```

---

## 🤝 Contributing

We welcome PRs, issues, and ideas! Please read the following before contributing:

* 📋 [CONTRIBUTING.md](./CONTRIBUTING.md)
* 📜 [CODE\_OF\_CONDUCT.md](./CODE_OF_CONDUCT.md)
* 🛡️ [SECURITY.md](./SECURITY.md)

To work on the library:

```sh
git clone https://github.com/MangaD/jsocketpp.git
cd jsocketpp
cmake --preset=clang-debug-x64
cmake --build --preset=clang-debug-x64
```

Remember to format your code with:

```sh
cmake --build . --target clang-format
```

And write tests in `tests/`!

---

## ⚖️ License

This project is licensed under the MIT License.
See [LICENSE](./LICENSE) for details.

---

## 🙏 Acknowledgements

* The Java Networking API — for its excellent abstractions
* GoogleTest — for testing
* CLion, VS Code, Ninja, and CMake — for development tooling
* Everyone who contributes, uses, or discusses jsocketpp ❤️

---

## ❓ FAQ

### Is jsocketpp async?

Yes — jsocketpp provides non-blocking and timeout-based I/O, including:

- `Socket::connect(timeout)`
- `ServerSocket::accept(timeout)` and `tryAccept(timeout)`
- `ServerSocket::acceptAsync()` (via `std::future`)
- `ServerSocket::acceptAsync(callback)` (via background thread)

These are based on `select()` internally and return in bounded time.

**Coroutine support** is a potential future feature via `co_await` + platform-specific polling (e.g., `epoll`, `kqueue`,
`IOCP`).

---

### How does error handling work?

All functions throw either:

- `SocketException` — for platform-level errors (`connect()`, `recv()` failure, etc.)
- `SocketTimeoutException` — if an operation times out (e.g., in `accept()` or `read()` with timeout set)

All errors include the native error code and message.

---

### Can I use jsocketpp in production?

Yes — `jsocketpp` is robust, portable, and suitable for production use in many real-world scenarios:

- Lightweight HTTP and WebSocket servers
- Game servers using TCP/UDP/multicast
- Local IPC using UNIX domain sockets
- Network tools, daemons, test harnesses

It supports non-blocking and timeout-driven I/O, including async `accept()` and `connect()` via `select()` and futures.

However, keep in mind:

- No TLS yet (use OpenSSL or mbedTLS externally)
- No coroutine (`co_await`) support yet
- Not thread-safe by default — use one socket per thread
- No high-performance event loop (e.g., epoll/io_uring)

---

### Does it work on Windows?

Yes. It fully supports Winsock, and even supports AF_UNIX on Windows 10+ (1803 or later).

On Windows:

- `WSAStartup` is handled internally
- Platform-specific errors are translated to readable messages

---

### What is the default buffer size?

Most read/write operations use a default internal buffer size of **4096 bytes**. You can override this in constructors
for `Socket`, `DatagramSocket`, etc.

---

## 🛣️ Roadmap

- [x] TCP (client + server)
- [x] UDP + Datagram abstraction
- [x] Multicast (IPv4 + IPv6)
- [x] UNIX domain sockets (Linux, macOS, Win10+)
- [x] Timeout + non-blocking support
- [x] Exception handling and message formatting
- [x] CMake + Conan + vcpkg packaging
- [x] CI with full CMake preset matrix
- [x] Doxygen-rich documentation
- [ ] TLS/SSL (via OpenSSL wrapper or interface)
- [ ] Coroutine-based async API (`co_await` I/O)
- [ ] epoll/kqueue/io_uring abstraction layer
- [ ] Thread-safe wrappers
- [ ] IPv6 advanced options (flow label, hop limit)
- [ ] Proxy socket types (SOCKS5, HTTP CONNECT)

---

## 🧭 See Also

- [Boost.Asio](https://think-async.com/) — heavyweight async networking
- [libuv](https://libuv.org/) — C event loop abstraction
- [asio (standalone)](https://github.com/chriskohlhoff/asio)
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) — a nice C++ HTTP server/client lib

# Publishing `libsocket` to the Conan Center Index (CCI)

This guide explains, step by step, how to prepare, test, and submit your C++ library `libsocket` to the Conan Center Index (CCI) so it can be easily consumed by the C++ community.

---

## 1. Prerequisites

- **Conan**: Install the latest Conan (2.x) from https://conan.io/downloads.html
- **CMake**: Ensure CMake 3.19+ is installed.
- **Git**: Required for CCI submission.
- **A public repository**: Your library must be hosted on a public VCS (e.g., GitHub).

---

## 2. Conan Recipe (`conanfile.py`)

A Conan recipe describes how to build, package, and consume your library. Your `conanfile.py` should:

- Inherit from `ConanFile`.
- Specify metadata (name, version, license, url, description, topics).
- Export sources (CMakeLists.txt, src/, include/).
- Use `CMakeToolchain` and `CMakeDeps` generators.
- Implement `layout`, `build`, `package`, and `package_info` methods.

**Example:**
```python
from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout

class LibsocketConan(ConanFile):
    name = "libsocket"
    version = "0.1.0"
    license = "MIT"
    author = "Your Name <your@email.com>"
    url = "https://github.com/yourusername/libsocket"
    description = "A modern C++ socket library."
    topics = ("socket", "network", "tcp", "udp", "c++")
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"
    exports_sources = "CMakeLists.txt", "src/*", "include/*"

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["socket"]
```

---

## 3. Make Your CMake Project Conan-Friendly

- Ensure your `CMakeLists.txt` supports installation (`install(TARGETS ...)`, `install(FILES ...)`).
- Support `CMAKE_INSTALL_PREFIX` and use standard CMake variables for headers and libraries.
- Optionally, add `find_package` support for consumers.

---

## 4. Test the Recipe Locally

1. Open a terminal in your project root.
2. Run:
   ```pwsh
   conan create .
   ```
   This will build and package your library using Conan. Fix any errors.

---

## 5. Prepare for Conan Center Index (CCI) Submission

CCI requires a specific structure and additional files:

- Fork the [conan-center-index](https://github.com/conan-io/conan-center-index) repository on GitHub.
- In your fork, add your recipe under `recipes/libsocket/all/`:
  - `conanfile.py` (the recipe)
  - `CMakeLists.txt` (if needed for test_package)
  - `test_package/` (a minimal consumer project to verify the package)

**Example structure:**
```
recipes/
  libsocket/
    all/
      conanfile.py
      test_package/
        conanfile.py
        CMakeLists.txt
        test_package.cpp
```

### test_package
- This is a minimal project that uses your library as a Conan dependency.
- It should build and run a simple test (e.g., create a socket, call a function).

---

## 6. Submit a Pull Request to CCI

1. Push your branch to your fork.
2. Open a pull request from your fork to the main CCI repository.
3. Follow the CCI PR template and guidelines:
   - https://github.com/conan-io/conan-center-index/wiki
   - https://github.com/conan-io/conan-center-index/blob/master/docs/how_to_add_packages.md
4. Address any feedback from the CCI team and automated CI.

---

## 7. After Acceptance

- Your library will be available to all Conan users via:
  ```pwsh
  conan search libsocket -r=conancenter
  conan install libsocket/0.1.0@
  ```
- Keep your recipe up to date as you release new versions.

---

## 8. Tips and Best Practices

- Use semantic versioning for your library.
- Keep your recipe and test_package minimal and clean.
- Document usage in your README and CMake config files.
- Respond promptly to CCI review comments.

---

## References
- [Conan Center Index Wiki](https://github.com/conan-io/conan-center-index/wiki)
- [How to add packages to CCI](https://github.com/conan-io/conan-center-index/blob/master/docs/how_to_add_packages.md)
- [Conan Documentation](https://docs.conan.io/)

---

If you need help with any step, consult the Conan documentation or ask the CCI community!

# Publishing `jsocketpp` to the vcpkg Registry

This guide explains, step by step, how to prepare, test, and submit your C++ library `jsocketpp` to the vcpkg registry
so it can be easily consumed by the C++ community.

---

## 1. Prerequisites

- **vcpkg**: Install the latest vcpkg from https://github.com/microsoft/vcpkg
- **CMake**: Ensure CMake 3.19+ is installed.
- **Git**: Required for registry submission.
- **A public repository**: Your library must be hosted on a public VCS (e.g., GitHub).

---

## 2. Prepare Your Library for vcpkg

- Ensure your `CMakeLists.txt` supports installation (`install(TARGETS ...)`, `install(FILES ...)`).
- Use standard CMake variables for headers and libraries.
- Support `find_package` for consumers (recommended).
- Use semantic versioning and tag releases in your VCS.

---

## 3. Create a vcpkg Portfile

A vcpkg port consists of:

- A `portfile.cmake` (build instructions)
- A `vcpkg.json` (metadata)
- Optionally, patches and usage files

### Example `vcpkg.json`

```json
{
    "name": "jsocketpp",
    "version-string": "0.1.0",
    "description": "A modern C++ socket library.",
    "homepage": "https://github.com/yourusername/jsocketpp",
    "license": "MIT"
}
```

### Example `portfile.cmake`

```cmake
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO yourusername/jsocketpp
    REF v0.1.0 # or the latest tag
    SHA512 <fill-in-sha512>
)

vcpkg_cmake_configure(
    SOURCE_PATH ${SOURCE_PATH}
    PREFER_NINJA
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup()

file(INSTALL ${SOURCE_PATH}/LICENSE DESTINATION ${CURRENT_PACKAGES_DIR}/share/jsocketpp RENAME copyright)
```

- Replace `<fill-in-sha512>` with the actual SHA512 hash of the release archive (see below).

---

## 4. Test the Port Locally

1. Clone the vcpkg repository and bootstrap it:
   ```pwsh
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   ```
2. Create a new port directory:
   ```pwsh
   mkdir ports\jsocketpp
   # Copy your portfile.cmake and vcpkg.json into this directory
   ```
3. Download your release archive and compute its SHA512:
   ```pwsh
   # Download the release tarball from GitHub
   curl -L -o jsocketpp-0.1.0.tar.gz https://github.com/yourusername/jsocketpp/archive/refs/tags/v0.1.0.tar.gz
   # Compute SHA512
   certutil -hashfile jsocketpp-0.1.0.tar.gz SHA512
   ```
   Copy the hash into your `portfile.cmake`.
4. Build and test the port:
   ```pwsh
   .\vcpkg install jsocketpp --overlay-ports=ports
   ```
   Fix any errors and ensure the library installs and can be found by consumers.

---

## 5. Submit to the vcpkg Registry

1. Fork the [vcpkg](https://github.com/microsoft/vcpkg) repository on GitHub.
2. Add your port to the `ports/` directory in your fork.
3. Open a pull request to the main vcpkg repository.
4. Follow the vcpkg PR template and guidelines:
    - https://github.com/microsoft/vcpkg/blob/master/docs/maintainers/adding-a-port.md
    - https://github.com/microsoft/vcpkg/blob/master/docs/maintainers/port-file-guidelines.md
5. Address any feedback from the vcpkg team and automated CI.

---

## 6. After Acceptance

- Your library will be available to all vcpkg users via:
  ```pwsh
  vcpkg install jsocketpp
  ```
- Keep your port up to date as you release new versions.

---

## 7. Tips and Best Practices

- Use semantic versioning and tag releases.
- Keep your portfile minimal and clean.
- Document usage in your README and CMake config files.
- Respond promptly to vcpkg review comments.

---

## References

- [vcpkg Documentation](https://github.com/microsoft/vcpkg/tree/master/docs)
- [Adding a port](https://github.com/microsoft/vcpkg/blob/master/docs/maintainers/adding-a-port.md)
- [Port file guidelines](https://github.com/microsoft/vcpkg/blob/master/docs/maintainers/port-file-guidelines.md)

---

If you need help with any step, consult the vcpkg documentation or ask the vcpkg community!

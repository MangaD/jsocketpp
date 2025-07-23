# ───────────────────────────────────────────────
# 🧱 CMake Project Configuration
# ───────────────────────────────────────────────

cmake_minimum_required(VERSION 3.16)

# Prevent in-source builds — ensures a clean build environment
if(CMAKE_CURRENT_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "In-source builds are not supported.\n" "Please use a separate build directory:\n"
                        "  mkdir build && cd build && cmake ..")
endif()

# ───────────────────────────────────────────────
# 🧾 Build Options
# ───────────────────────────────────────────────
option(BUILD_SOURCE "Build primary libraries and executables" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(ENABLE_EXAMPLES "Build example programs" ON)
option(ENABLE_TESTS "Build unit tests" ON)
option(BUILD_DOCS "Build documentation (requires Doxygen)" ON)

if(BUILD_SOURCE)
    set(PROJECT_LANGUAGES CXX)
endif()

# ───────────────────────────────────────────────
# 📦 Project Declaration
# ───────────────────────────────────────────────
project(
    jsocketpp
    VERSION 1.0
    DESCRIPTION "A cross-platform C++20 socket library."
    HOMEPAGE_URL "https://github.com/MangaD/jsocketpp"
    LANGUAGES ${PROJECT_LANGUAGES})

# ───────────────────────────────────────────────
# 📚 Optional Documentation
# ───────────────────────────────────────────────
if(BUILD_DOCS)
    find_package(Doxygen QUIET)
    if(DOXYGEN_FOUND)
        include(cmake/doxygen.cmake)
    else()
        message(WARNING "Doxygen not found — skipping documentation generation.")
    endif()
endif()

# Stop early if only docs are being built
if(NOT BUILD_SOURCE)
    return()
endif()

# ───────────────────────────────────────────────
# ⚙️ Compiler Configuration
# ───────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ───────────────────────────────────────────────
# 🧱 Subdirectories (Core Modules)
# ───────────────────────────────────────────────
add_subdirectory(src)

if(ENABLE_TESTS)
    add_subdirectory(tests)
endif()

if(ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()

# ───────────────────────────────────────────────
# 📤 Install + Export Targets
# ───────────────────────────────────────────────
include(cmake/install.cmake)

# ───────────────────────────────────────────────
# 🧹 clang-format / clang-tidy Integration
# ───────────────────────────────────────────────
include(cmake/clang-tools.cmake)

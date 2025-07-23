# Use latest CMake version features and modern CMake best practices
cmake_minimum_required(VERSION 3.16)

# Prevent in-source builds (recommended CMake practice)
if(CMAKE_CURRENT_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Please create a build directory and remove CMakeCache.txt.")
endif()

# Options for building
option(BUILD_SOURCE "Build primary libraries and executables." ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(ENABLE_EXAMPLES "Build example programs" ON)
option(BUILD_DOCS "Build documentation" ON)

if(${BUILD_SOURCE})
    set(PROJECT_LANGUAGES CXX)
endif()

# Project declaration
project(
    jsocketpp
    VERSION 1.0
    DESCRIPTION "A cross-platform C++20 socket library."
    HOMEPAGE_URL "https://github.com/MangaD/jsocketpp"
    LANGUAGES ${PROJECT_LANGUAGES})

# Optional: Generate Doxygen documentation if Doxygen is found
if(${BUILD_DOCS})
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        include(cmake/doxygen.cmake)
    else()
        message(WARNING "Doxygen not found, skipping documentation generation.")
    endif()
endif()

# Source will be built only if BUILD_SOURCE is ON
if(NOT BUILD_SOURCE)
    return()
endif()

# Set C++ standard requirement (C++20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add subdirectories
add_subdirectory(src)
add_subdirectory(tests)

if(ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install and packaging logic
install(
    EXPORT jsocketppTargets
    FILE jsocketppTargets.cmake
    NAMESPACE jsocketpp::
    DESTINATION lib/cmake/jsocketpp)

# Create and install version file for CMake package compatibility
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/jsocketppConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/jsocketppConfigVersion.cmake" DESTINATION lib/cmake/jsocketpp)

# ───────────────────────────────────────────────
# Clang-Tidy & Clang-Format Integration
# ───────────────────────────────────────────────

# Find clang-tidy if available and enable it conditionally
find_program(CLANG_TIDY_EXE NAMES clang-tidy)

# Enable clang-tidy only if the environment or cmake variable CLANG_TIDY_ENABLE is set This avoids forcing it on all builds and
# makes it opt-in for developers or CI.
if(CLANG_TIDY_EXE AND (DEFINED CLANG_TIDY_ENABLE OR DEFINED ENV{CLANG_TIDY_ENABLE}))
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}" "--warnings-as-errors=*")
    message(STATUS "Clang-Tidy enabled: ${CLANG_TIDY_EXE}")
else()
    message(STATUS "Clang-Tidy not enabled (pass -DCLANG_TIDY_ENABLE=ON to enable it)")
endif()

# Find clang-format and register format-related targets
find_program(CLANG_FORMAT_EXE NAMES clang-format)

if(CLANG_FORMAT_EXE)
    # Collect all formatable source files
    file(
        GLOB_RECURSE
        JSOCKETPP_FORMAT_FILES
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.hpp"
        "${CMAKE_SOURCE_DIR}/tests/*.cpp"
        "${CMAKE_SOURCE_DIR}/tests/*.hpp"
        "${CMAKE_SOURCE_DIR}/examples/*.cpp")

    # Format files in-place
    add_custom_target(
        clang-format
        COMMAND ${CLANG_FORMAT_EXE} -i --style=file ${JSOCKETPP_FORMAT_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting source code with clang-format"
        VERBATIM)

    # Check-only (dry-run) format target — ideal for CI enforcement
    add_custom_target(
        clang-format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror --style=file ${JSOCKETPP_FORMAT_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking clang-format compliance"
        VERBATIM)
else()
    message(STATUS "Clang-Format not found — clang-format targets will not be available")
endif()

# Export targets for use with find_package()
export(EXPORT jsocketppTargets FILE "${CMAKE_CURRENT_BINARY_DIR}/jsocketppTargets.cmake")

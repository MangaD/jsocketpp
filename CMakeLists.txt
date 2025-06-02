# Use latest CMake version features and modern CMake best practices
cmake_minimum_required(VERSION 3.16)

# Prevent in-source builds (recommended CMake practice)
if (CMAKE_CURRENT_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Please create a build directory and remove CMakeCache.txt.")
endif ()

# Options for building
option(BUILD_SOURCE "Build primary libraries and executables." ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(ENABLE_EXAMPLES "Build example programs" ON)
option(BUILD_DOCS "Build documentation" ON)

if (${BUILD_SOURCE})
    set(PROJECT_LANGUAGES CXX)
endif ()

# Project declaration
project(jsocketpp
        VERSION 1.0
        DESCRIPTION "A cross-platform C++17 socket library."
        HOMEPAGE_URL "https://github.com/MangaD/jsocketpp"
        LANGUAGES ${PROJECT_LANGUAGES})

# Optional: Generate Doxygen documentation if Doxygen is found
if (${BUILD_DOCS})
    find_package(Doxygen)
    if (DOXYGEN_FOUND)
        include(cmake/doxygen.cmake)
    else ()
        message(WARNING "Doxygen not found, skipping documentation generation.")
    endif ()
endif ()

# Source will be built only if BUILD_SOURCE is ON
if (NOT BUILD_SOURCE)
    return()
endif ()

# Set C++ standard requirement (C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add subdirectories
add_subdirectory(src)
add_subdirectory(tests)

if (ENABLE_EXAMPLES)
    add_subdirectory(examples)
endif ()

# Install and packaging logic
install(EXPORT jsocketppTargets
        FILE jsocketppTargets.cmake
        NAMESPACE jsocketpp::
        DESTINATION lib/cmake/jsocketpp
)

# Create and install version file for CMake package compatibility
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/jsocketppConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/jsocketppConfigVersion.cmake" DESTINATION lib/cmake/jsocketpp)

# clang-format integration (if found)
if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    find_program(CLANG_FORMAT_EXE NAMES clang-format)
    if (CLANG_FORMAT_EXE)
        file(GLOB_RECURSE ALL_SOURCE_FILES
                "${CMAKE_SOURCE_DIR}/src/*.cpp"
                "${CMAKE_SOURCE_DIR}/src/*.hpp"
                "${CMAKE_SOURCE_DIR}/tests/*.cpp"
                "${CMAKE_SOURCE_DIR}/tests/*.hpp"
        )
        add_custom_target(
                clang-format
                COMMAND ${CLANG_FORMAT_EXE} -i --style=file ${ALL_SOURCE_FILES}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                COMMENT "Running clang-format on all source files"
        )
    endif ()
endif ()

# Export targets for use with find_package()
export(EXPORT jsocketppTargets FILE "${CMAKE_CURRENT_BINARY_DIR}/jsocketppTargets.cmake")

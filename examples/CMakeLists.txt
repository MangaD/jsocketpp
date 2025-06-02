# CMakeLists.txt for jsocketpp/examples

cmake_minimum_required(VERSION 3.14)

# This CMakeLists.txt should be included from the top-level CMakeLists.txt,
# or you can add it as a subdirectory: add_subdirectory(examples)

# All examples will be built only if ENABLE_EXAMPLES is ON
if (NOT ENABLE_EXAMPLES)
    return()
endif ()

project(jsocketpp_examples LANGUAGES CXX)

# Find the jsocketpp library target (assumed built in parent project)
# Adjust the target name/path if needed.
# If jsocketpp is an installed package, you can use find_package(jsocketpp REQUIRED)

# Include the main project's include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Gather all example source files (*.cpp)
file(GLOB EXAMPLE_SOURCES "*.cpp")

foreach (EXAMPLE_SRC ${EXAMPLE_SOURCES})
    # Get the file name without extension
    get_filename_component(EXAMPLE_NAME ${EXAMPLE_SRC} NAME_WE)
    add_executable(${EXAMPLE_NAME} ${EXAMPLE_SRC})
    target_link_libraries(${EXAMPLE_NAME} PRIVATE jsocketpp)

    # Set C++17 standard
    target_compile_features(${EXAMPLE_NAME} PRIVATE cxx_std_17)

    # On Unix, link pthread if necessary (some socket APIs require it)
    if (UNIX AND NOT APPLE)
        target_link_libraries(${EXAMPLE_NAME} PRIVATE pthread)
    endif ()

    # On Windows, link ws2_32 for sockets
    if (WIN32)
        target_link_libraries(${EXAMPLE_NAME} PRIVATE ws2_32)
    endif ()

    # If you have additional include dirs or definitions, set them here
    # target_include_directories(${EXAMPLE_NAME} PRIVATE ...)
    # target_compile_definitions(${EXAMPLE_NAME} PRIVATE ...)
endforeach ()

# Optionally install example binaries
install(TARGETS ${EXAMPLE_NAME}
        RUNTIME DESTINATION bin/examples)

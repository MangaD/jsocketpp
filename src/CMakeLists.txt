# src/CMakeLists.txt - Build the jsocketpp library

# Create the jsocketpp library (STATIC or SHARED depending on your needs)
add_library(jsocketpp common.cpp DatagramSocket.cpp MulticastSocket.cpp ServerSocket.cpp Socket.cpp UnixSocket.cpp)

# Set C++ standard requirement for the library (C++20)
target_compile_features(jsocketpp PUBLIC cxx_std_20)

# Define the public headers (located in include/jsocketpp/)
target_include_directories(
    jsocketpp PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include> # For local builds
                     $<INSTALL_INTERFACE:include> # For installed headers
)

# Platform-specific libraries for Windows
if(WIN32)
    target_link_libraries(jsocketpp PUBLIC ws2_32 iphlpapi)
endif()

# Set to PRIVATE so that these definitions do not propagate to consumers of the library
target_compile_definitions(
    jsocketpp
    PRIVATE # Disable secure warnings on MSVC
            $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
            # Enable Unicode support on Windows
            $<$<PLATFORM_ID:Windows>:UNICODE
            _UNICODE>)
# Note: NDEBUG is defined in the C standard, but DEBUG is not. Read comments: https://stackoverflow.com/a/8594122/3049315 C
# standard section: http://port70.net/~nsz/c/c11/n1570.html#7.2

# Enable warnings - inspired by: https://stackoverflow.com/a/50882216/3049315
target_compile_options(
    jsocketpp
    PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W4> # /WX
            $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall
            -Wextra
            -pedantic
            -Wmain
            -Weffc++
            -Wswitch-default
            -Wswitch-enum
            -Wmissing-include-dirs
            -Wmissing-declarations
            -Wunreachable-code
            -Winline
            -Wfloat-equal
            -Wundef
            -Wcast-align
            -Wredundant-decls
            -Winit-self
            -Wshadow
            -Wnon-virtual-dtor
            -Wconversion
            -Wstrict-aliasing
            -Wold-style-cast
            -Wsign-conversion>
            $<$<CXX_COMPILER_ID:GNU>:-Wzero-as-null-pointer-constant
            -Wsuggest-override>
            $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_GREATER:$<CXX_COMPILER_VERSION>,10.1>>:
            -Warith-conversion>
            $<$<CXX_COMPILER_ID:Clang>:-Wthread-safety>)

# Use CMake's install command to specify where the library and headers should be placed when installed
install(
    TARGETS jsocketpp
    EXPORT jsocketppTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/jsocketpp # Place header in include/jsocketpp/
)

# Export the targets to allow easy find_package usage
install(
    EXPORT jsocketppTargets
    FILE jsocketppTargets.cmake
    NAMESPACE jsocketpp::
    DESTINATION lib/cmake/jsocketpp)

# Optionally, create a version file for the CMake package (useful for versioning)
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/jsocketppConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/jsocketppConfigVersion.cmake" DESTINATION lib/cmake/jsocketpp)

# Optional: Ensure that shared libraries are built (if BUILD_SHARED_LIBS is set)
if(BUILD_SHARED_LIBS)
    set_target_properties(
        jsocketpp PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON # If targeting Windows, export all symbols for shared libraries
    )
endif()

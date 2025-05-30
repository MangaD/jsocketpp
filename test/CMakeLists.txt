# test/CMakeLists.txt - Build test executables for jsocketpp

# Add client and server executables
add_executable(client client.cpp)
target_link_libraries(client PRIVATE jsocketpp)

add_executable(server server.cpp)
target_link_libraries(server PRIVATE jsocketpp)

target_compile_definitions(client PRIVATE
        # Disable secure warnings on MSVC
        $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
        # Enable Unicode support on Windows
        $<$<PLATFORM_ID:Windows>:UNICODE _UNICODE>
)
target_compile_options(client PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4> # /WX
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic -Wmain -Weffc++ -Wswitch-default
        -Wswitch-enum -Wmissing-include-dirs -Wmissing-declarations -Wunreachable-code -Winline
        -Wfloat-equal -Wundef -Wcast-align -Wredundant-decls -Winit-self -Wshadow -Wnon-virtual-dtor
        -Wconversion -Wstrict-aliasing -Wold-style-cast -Wsign-conversion>
        $<$<CXX_COMPILER_ID:GNU>:-Wzero-as-null-pointer-constant -Wsuggest-override>
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_GREATER:$<CXX_COMPILER_VERSION>,10.1>>: -Warith-conversion>
        $<$<CXX_COMPILER_ID:Clang>:-Wthread-safety>
)

target_compile_definitions(server PRIVATE
        # Disable secure warnings on MSVC
        $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
        # Enable Unicode support on Windows
        $<$<PLATFORM_ID:Windows>:UNICODE _UNICODE>
)
target_compile_options(server PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4> # /WX
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -pedantic -Wmain -Weffc++ -Wswitch-default
        -Wswitch-enum -Wmissing-include-dirs -Wmissing-declarations -Wunreachable-code -Winline
        -Wfloat-equal -Wundef -Wcast-align -Wredundant-decls -Winit-self -Wshadow -Wnon-virtual-dtor
        -Wconversion -Wstrict-aliasing -Wold-style-cast -Wsign-conversion>
        $<$<CXX_COMPILER_ID:GNU>:-Wzero-as-null-pointer-constant -Wsuggest-override>
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_GREATER:$<CXX_COMPILER_VERSION>,10.1>>: -Warith-conversion>
        $<$<CXX_COMPILER_ID:Clang>:-Wthread-safety>
)

# GoogleTest-based unit tests
include(FetchContent)

# Fetch GoogleTest from GitHub (instead of downloading a ZIP file)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0  # Lock to a specific version
)

# Force shared CRT for GoogleTest
set(gtest_force_shared_crt ON CACHE BOOL "Force shared CRT for GoogleTest" FORCE)

# Make GoogleTest available for use
FetchContent_MakeAvailable(googletest)

# Add the test executable for socket_gtest
add_executable(socket_gtest socket_gtest.cpp)

# Link jsocketpp and GoogleTest to the test executable
target_link_libraries(socket_gtest PRIVATE jsocketpp gtest gtest_main)

# Enable test discovery for GoogleTest
include(GoogleTest)
gtest_discover_tests(socket_gtest)

# Include directories for jsocketpp and GoogleTest headers
target_include_directories(socket_gtest PRIVATE
        ${CMAKE_SOURCE_DIR}/include             # Include the jsocketpp headers
        ${gtest_SOURCE_DIR}/googletest/include  # Include GoogleTest headers
        ${gtest_SOURCE_DIR}/googlemock/include  # Include GoogleMock headers if needed
)

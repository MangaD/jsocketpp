# ───────────────────────────────────────────────
# 🧹 clang-tidy & clang-format integration
# ───────────────────────────────────────────────

# Optional clang-tidy integration (CMake-native)
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE AND (CLANG_TIDY_ENABLE OR DEFINED ENV{CLANG_TIDY_ENABLE}))
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}" "--warnings-as-errors=*")
    message(STATUS "clang-tidy enabled: ${CLANG_TIDY_EXE}")
else()
    message(STATUS "clang-tidy not enabled (pass -DCLANG_TIDY_ENABLE=ON to enable it)")
endif()

# Optional clang-format targets
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    file(
        GLOB_RECURSE
        JSOCKETPP_FORMAT_FILES
        "${CMAKE_SOURCE_DIR}/src/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/*.hpp"
        "${CMAKE_SOURCE_DIR}/tests/*.cpp"
        "${CMAKE_SOURCE_DIR}/tests/*.hpp"
        "${CMAKE_SOURCE_DIR}/examples/*.cpp")

    add_custom_target(
        clang-format
        COMMAND ${CLANG_FORMAT_EXE} -i --style=file ${JSOCKETPP_FORMAT_FILES}
        COMMENT "Formatting source code with clang-format"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        VERBATIM)

    add_custom_target(
        clang-format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror --style=file ${JSOCKETPP_FORMAT_FILES}
        COMMENT "Checking clang-format compliance"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        VERBATIM)
else()
    message(STATUS "clang-format not found — clang-format targets will not be available")
endif()

# ───────────────────────────────────────────────
# 📦 Install Rules & CMake Package Config
# ───────────────────────────────────────────────

# Install target export for consumers using find_package()
install(
    EXPORT jsocketppTargets
    FILE jsocketppTargets.cmake
    NAMESPACE jsocketpp::
    DESTINATION lib/cmake/jsocketpp)

# CMake version config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/jsocketppConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/jsocketppConfigVersion.cmake" DESTINATION lib/cmake/jsocketpp)

# Export the targets to the build directory too (for local usage)
export(EXPORT jsocketppTargets FILE "${CMAKE_CURRENT_BINARY_DIR}/jsocketppTargets.cmake")

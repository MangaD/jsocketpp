# cmake/doxygen.cmake
#
# This script configures and generates Doxygen documentation for the project. It sets various Doxygen configuration variables
# directly (instead of using a separate Doxyfile.in) and creates a custom target named "doxygen" that runs Doxygen using the
# generated configuration file.
#
# The documentation will include both the source code (in ${CMAKE_SOURCE_DIR}/src) and the markdown files in the docs directory
# (in ${CMAKE_SOURCE_DIR}/docs). The main page is set to the README.md file in the project root.
#
# Usage: In your top-level CMakeLists.txt, after calling find_package(Doxygen REQUIRED), add the following line:
#
# include(cmake/doxygen.cmake)
#
# Then, you can generate the documentation by building the target:
#
# cmake --build . --target doxygen

if(DOXYGEN_FOUND)
    # Configure Doxygen settings directly
    set(DOXYGEN_PROJECT_NAME "${PROJECT_NAME}")
    set(DOXYGEN_PROJECT_BRIEF "${PROJECT_DESCRIPTION}")
    set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
    set(DOXYGEN_PROJECT_LOGO "docs/doxygen/logo.png")
    set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/docs/doxygen")
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_EXTRACT_PRIVATE YES)
    set(DOXYGEN_EXTRACT_STATIC YES)
    set(DOXYGEN_EXTRACT_LOCAL_CLASSES YES)
    set(DOXYGEN_FULL_PATH_NAMES YES)
    set(DOXYGEN_STRIP_FROM_PATH ${CMAKE_SOURCE_DIR})
    # set(DOXYGEN_EXCLUDE "${CMAKE_SOURCE_DIR}/docs/doxygen"
    # "${CMAKE_BINARY_DIR}")
    set(DOXYGEN_RECURSIVE YES)
    set(DOXYGEN_ENABLE_PREPROCESSING NO) # https://stackoverflow.com/a/26043120/3049315

    set(DOXYGEN_GENERATE_HTML
        YES
        CACHE BOOL "Generate Doxygen HTML" FORCE)
    set(DOXYGEN_GENERATE_LATEX
        NO
        CACHE BOOL "Generate Doxygen LaTeX" FORCE)
    set(DOXYGEN_GENERATE_XML YES) # If you need XML for Sphinx or other tools.

    # Additional configuration for a nicer HTML output.
    set(DOXYGEN_GENERATE_TREEVIEW YES)
    set(DOXYGEN_DISABLE_INDEX NO)
    set(DOXYGEN_FULL_SIDEBAR NO)
    set(DOXYGEN_HTML_COLORSTYLE LIGHT)
    set(DOXYGEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen)
    set(DOXYGEN_HTML_HEADER ${DOXYGEN_DIR}/header.html)
    set(DOXYGEN_HTML_FOOTER ${DOXYGEN_DIR}/footer.html)
    set(DOXYGEN_HTML_EXTRA_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
        ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome-darkmode-toggle.js
        ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome-fragment-copy-button.js
        ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome-paragraph-link.js
        ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome-interactive-toc.js
        ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome-tabs.js)
    set(DOXYGEN_HTML_EXTRA_STYLESHEET
        ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome.css
        ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome-sidebar-only.css
        ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome-sidebar-only-darkmode-toggle.css)

    # Add Table of Contents to markdown files
    file(GLOB md_files ${CMAKE_SOURCE_DIR}/docs/*.md)

    file(COPY ${CMAKE_SOURCE_DIR}/README.md DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/md_files")
    file(COPY ${md_files} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/md_files/docs")

    file(GLOB_RECURSE md_files "${CMAKE_CURRENT_BINARY_DIR}/md_files/*.md")
    foreach(filename ${md_files})
        file(READ ${filename} MD_TEXT)
        # Insert [TOC] immediately after the first level-1 header line.
        string(REGEX REPLACE "^(# [^\n]+)\r?\n(.+)" "\\1\n[TOC]\n\\2" MD_TEXT_MODIFIED "${MD_TEXT}")
        file(WRITE ${filename} "${MD_TEXT_MODIFIED}")
    endforeach()

    # Set the main page to the README.md in the project root.
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_BINARY_DIR}/md_files/README.md")

    doxygen_add_docs(doxygen ${md_files} ${CMAKE_SOURCE_DIR}/include/jsocketpp ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/test
                     COMMENT "Generating API documentation with Doxygen")

    message(STATUS "Documentation will be output to: ${DOXYGEN_OUTPUT_DIRECTORY}")
else()
    message(WARNING "Doxygen not found! The 'doxygen' target will not be available.")
endif()

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
    set(DOXYGEN_PROJECT_NAME
        "${PROJECT_NAME}"
        CACHE INTERNAL "")
    set(DOXYGEN_PROJECT_BRIEF
        "${PROJECT_DESCRIPTION}"
        CACHE INTERNAL "")
    set(DOXYGEN_PROJECT_NUMBER
        "${PROJECT_VERSION}"
        CACHE INTERNAL "")
    set(DOXYGEN_PROJECT_LOGO "docs/doxygen/logo55px.png")
    set(DOXYGEN_PROJECT_ICON "docs/doxygen/logo55px.png")
    set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/docs/doxygen")
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_EXTRACT_PRIVATE YES)
    set(DOXYGEN_EXTRACT_STATIC YES)
    set(DOXYGEN_EXTRACT_LOCAL_CLASSES YES)
    set(DOXYGEN_FULL_PATH_NAMES YES)
    set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
    set(DOXYGEN_WARN_NO_PARAMDOC YES)
    set(DOXYGEN_STRIP_FROM_PATH ${CMAKE_SOURCE_DIR})
    # set(DOXYGEN_EXCLUDE "${CMAKE_SOURCE_DIR}/docs/doxygen"
    # "${CMAKE_BINARY_DIR}")
    set(DOXYGEN_RECURSIVE YES)
    set(DOXYGEN_ENABLE_PREPROCESSING NO) # https://stackoverflow.com/a/26043120/3049315

    set(DOXYGEN_GENERATE_HTML
        YES
        CACHE BOOL "Generate Doxygen HTML" FORCE)

    # Optional LaTeX/PDF output
    option(GENERATE_PDF_DOCS "Generate PDF documentation via LaTeX" OFF)
    if(GENERATE_PDF_DOCS)
        set(DOXYGEN_GENERATE_LATEX YES)
    else()
        set(DOXYGEN_GENERATE_LATEX NO)
    endif()

    set(DOXYGEN_GENERATE_XML YES) # If you need XML for Sphinx or other tools.

    # Additional configuration for a nicer HTML output.
    set(DOXYGEN_GENERATE_TREEVIEW YES) # Required by doxygen-awesome-css
    set(DOXYGEN_DISABLE_INDEX NO) # Required by doxygen-awesome-css
    set(DOXYGEN_FULL_SIDEBAR NO) # Required by doxygen-awesome-css
    set(DOXYGEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/docs/doxygen)
    set(DOXYGEN_HTML_COLORSTYLE "TOGGLE")

    # === Doxygen Awesome CSS
    if(DOXYGEN_VERSION VERSION_LESS 1.14.0)
        set(DOXYGEN_HTML_HEADER ${DOXYGEN_DIR}/header.html)
        set(DOXYGEN_HTML_FOOTER ${DOXYGEN_DIR}/footer.html)
        set(DOXYGEN_HTML_COLORSTYLE LIGHT) # Required by doxygen-awesome-css
        set(DOXYGEN_HTML_EXTRA_FILES
            ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome-darkmode-toggle.js
            ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome-fragment-copy-button.js
            ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome-paragraph-link.js
            ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome-interactive-toc.js
            ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome-tabs.js)
        set(DOXYGEN_HTML_EXTRA_STYLESHEET
            ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome.css
            ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome-sidebar-only.css
            ${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome-sidebar-only-darkmode-toggle.css)
    endif()

    # Add Table of Contents to markdown files
    file(GLOB md_files "${CMAKE_SOURCE_DIR}/docs/markdown/*.md")

    file(COPY "${CMAKE_SOURCE_DIR}/README.md" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/md_files")
    file(
        WRITE "${CMAKE_CURRENT_BINARY_DIR}/md_files/index.md"
        "\\defgroup docs User Guides\n\\hidegroupgraph\n\\brief Documentation for users of jsocketpp.\n\nThis group includes guides and references for getting started with jsocketpp, contributing to the project, and understanding the project's policies."
    )
    file(COPY "${CMAKE_SOURCE_DIR}/CODE_OF_CONDUCT.md" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/md_files")
    file(COPY "${CMAKE_SOURCE_DIR}/CONTRIBUTING.md" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/md_files")

    file(COPY ${md_files} DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/md_files/docs")

    file(GLOB_RECURSE md_files "${CMAKE_CURRENT_BINARY_DIR}/md_files/*.md")
    foreach(filename ${md_files})
        file(READ "${filename}" MD_TEXT)
        # Get the filename without path and extension
        get_filename_component(fname "${filename}" NAME_WE)
        get_filename_component(basename "${filename}" NAME)
        # Insert [TOC] and Doxygen group declarations after the first level-1 header line.
        if(MD_TEXT MATCHES "^# ([^\n]+)\n")
            if(NOT filename MATCHES "md_files/docs/")
                string(REGEX REPLACE "^# ([^\n]+)\r?\n(.+)" "# \\1\n[TOC]\n\\2" MD_TEXT_MODIFIED "${MD_TEXT}")
            else()
                string(
                    REGEX
                    REPLACE "^# ([^\n]+)\r?\n(.+)"
                            "\\\\defgroup ${fname} \\1\n\\\\ingroup docs\n\\\\hidegroupgraph\n# \\1\n[TOC]\n\\2"
                            MD_TEXT_MODIFIED "${MD_TEXT}")
            endif()
            file(WRITE ${filename} "${MD_TEXT_MODIFIED}")
        endif()
    endforeach()

    # Set the main page to the README.md in the project root.
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_BINARY_DIR}/md_files/README.md")

    # Copy logo to destination dir
    file(COPY "${CMAKE_SOURCE_DIR}/docs/doxygen/logo.png" DESTINATION "${CMAKE_SOURCE_DIR}/docs/doxygen/html/docs/doxygen/")

    doxygen_add_docs(doxygen ${md_files} ${CMAKE_SOURCE_DIR}/include/jsocketpp ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests
                     COMMENT "Generating API documentation with Doxygen")

    if(NOT EXISTS "${DOXYGEN_DIR}/doxygen-awesome-css/doxygen-awesome.css")
        message(WARNING "doxygen-awesome-css submodule not found. Did you run `git submodule update --init --recursive`?")
    endif()

    message(STATUS "Documentation will be output to: ${DOXYGEN_OUTPUT_DIRECTORY}")
else()
    message(
        WARNING
            "Doxygen not found! Please install it using your package manager (e.g., `apt install doxygen`, `brew install doxygen`, or `choco install doxygen`) to enable the 'doxygen' target."
    )
endif()

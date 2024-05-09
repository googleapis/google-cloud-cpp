# ~~~
# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

find_package(Doxygen QUIET)
find_program(XSLTPROC xsltproc)

function (google_cloud_cpp_doxygen_deploy_version VAR)
    set(VERSION "${PROJECT_VERSION}")
    if (${GOOGLE_CLOUD_CPP_USE_LATEST_FOR_REFDOC_LINKS})
        set(VERSION "latest")
    endif ()
    set(${VAR}
        "${VERSION}"
        PARENT_SCOPE)
endfunction ()

function (google_cloud_cpp_doxygen_targets_impl library)
    if (NOT GOOGLE_CLOUD_CPP_GENERATE_DOXYGEN OR NOT DOXYGEN_FOUND)
        return()
    endif ()

    cmake_parse_arguments(opt "RECURSIVE;THREADED" "" "INPUTS;TAGFILES;DEPENDS"
                          ${ARGN})

    # Options controlling the inputs into Doxygen
    set(GOOGLE_CLOUD_CPP_DOXYGEN_INPUTS ${_opt_INPUTS})
    set(DOXYGEN_TAGFILES ${opt_TAGFILES})
    if (opt_RECURSIVE)
        set(DOXYGEN_RECURSIVE YES)
    else ()
        set(DOXYGEN_RECURSIVE NO)
    endif ()
    if (opt_THREADED)
        set(DOXYGEN_NUM_PROC_THREADS 32)
        cmake_host_system_information(RESULT NUM_CORES
                                      QUERY NUMBER_OF_LOGICAL_CORES)
        if (NUM_CORES LESS DOXYGEN_NUM_PROC_THREADS)
            set(DOXYGEN_NUM_PROC_THREADS NUM_CORES)
        endif ()
    else ()
        set(DOXYGEN_NUM_PROC_THREADS 1)
    endif ()

    if (NOT ("${DOXYGEN_NUM_PROC_THREADS}" STREQUAL "1"))
        message("Performing Doxygen processing on library=${library}"
                " using DOXYGEN_NUM_PROC_THREADS=${DOXYGEN_NUM_PROC_THREADS}")
    endif ()

    set(DOXYGEN_FILE_PATTERNS "*.dox" "*.h")
    set(DOXYGEN_EXCLUDE_PATTERNS
        # We should skip internal directories to speed up the build. We do not
        # use "*/internal/*" because Doxygen breaks when we include
        # "*/internal/*.inc" from a public header. (Don't ask me why).
        "*/internal/*.h"
        "*/internal/*.cc"
        # We should skip all tests.
        "*_test.cc"
        # TODO(#9841): remove this special case when the files are removed.
        "retry_traits.h"
        # Handwritten libraries may contain the following subdirectories, which
        # are not customer-facing. We can skip them.
        "*/benchmarks/*"
        "*/integration_tests/*"
        "*/testing/*"
        "*/tests/*")
    set(DOXYGEN_EXAMPLE_RECURSIVE NO)
    set(DOXYGEN_EXCLUDE_SYMLINKS YES)

    # Options controlling how Doxygen behaves on errors and the level of output.
    set(DOXYGEN_QUIET YES)
    set(DOXYGEN_WARN_AS_ERROR YES)

    # Options controlling the format of the output.
    google_cloud_cpp_doxygen_deploy_version(VERSION)
    list(
        APPEND
        DOXYGEN_ALIASES
        "cloud_cpp_docs_link{2}=\"https://cloud.google.com/cpp/docs/reference/\\1/${VERSION}/\\2\""
        "cloud_cpp_reference_link{1}=\"https://github.com/googleapis/google-cloud-cpp/blob/${GOOGLE_CLOUD_CPP_COMMIT_SHA}/\\1\""
    )
    set(DOXYGEN_INLINE_INHERITED_MEMB YES)
    set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
    set(DOXYGEN_BUILTIN_STL_SUPPORT YES)
    set(DOXYGEN_IDL_PROPERTY_SUPPORT NO)
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_EXTRACT_STATIC YES)
    set(DOXYGEN_SORT_MEMBERS_CTORS_1ST YES)
    set(DOXYGEN_GENERATE_TODOLIST NO)
    set(DOXYGEN_GENERATE_BUGLIST NO)
    set(DOXYGEN_GENERATE_TESTLIST NO)
    set(DOXYGEN_HAVE_DOT NO)
    set(DOXYGEN_GRAPHICAL_HIERARCHY NO)
    set(DOXYGEN_DIRECTORY_GRAPH NO)
    set(DOXYGEN_CLASS_GRAPH NO)
    set(DOXYGEN_COLLABORATION_GRAPH NO)
    set(DOXYGEN_INCLUDE_GRAPH NO)
    set(DOXYGEN_INCLUDED_BY_GRAPH NO)
    set(DOXYGEN_STRIP_FROM_INC_PATH "${PROJECT_SOURCE_DIR}")
    set(DOXYGEN_SHOW_USED_FILES NO)
    set(DOXYGEN_SHOW_FILES NO)
    set(DOXYGEN_REFERENCES_LINK_SOURCE NO)
    set(DOXYGEN_SOURCE_BROWSER NO)
    set(DOXYGEN_DISTRIBUTE_GROUP_DOC YES)
    set(DOXYGEN_HTML_TIMESTAMP YES)
    set(DOXYGEN_LAYOUT_FILE
        "${PROJECT_SOURCE_DIR}/ci/etc/doxygen/DoxygenLayout.xml")

    # Options controlling how to parse the C++ code
    set(DOXYGEN_CLANG_ASSISTED_PARSING YES)
    # We set -DHAVE_ABSEIL to generate documentation for libraries that expose
    # OpenTelemetry types. Ideally, we would generalize this to pick up any
    # linked library's compile definitions, but that would be hard, and we have
    # other things to do.
    set(DOXYGEN_CLANG_OPTIONS
        "-Wno-deprecated-declarations -DHAVE_ABSEIL ${GOOGLE_CLOUD_CPP_DOXYGEN_CLANG_OPTIONS}"
    )
    set(DOXYGEN_CLANG_DATABASE_PATH)
    set(DOXYGEN_SEARCH_INCLUDES YES)
    set(DOXYGEN_INCLUDE_PATH
        # Most of the source code is included via this path
        "${PROJECT_SOURCE_DIR}"
        # Any generated header files (we may have eliminated these) are here
        "${PROJECT_BINARY_DIR}"
        # Generated headers from proto files by each library will be found here
        "${CMAKE_CURRENT_BINARY_DIR}"
        # Generated headers from common proto files, as well as generated
        # headers from some services that precede the automated scaffold are
        # found here
        "${PROJECT_BINARY_DIR}/external/googleapis"
        # Proto files generated by the examples are found here
        "${PROJECT_BINARY_DIR}/examples"
        # Any additional includes that the library needs
        ${GOOGLE_CLOUD_CPP_DOXYGEN_EXTRA_INCLUDES})

    # Macros confuse Doxygen. We don't use too many, so we predefine them here
    # to be noops or have simple values.
    set(DOXYGEN_MACRO_EXPANSION YES)
    set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
    set(DOXYGEN_PREDEFINED
        "GOOGLE_CLOUD_CPP_DEPRECATED(x)="
        "GOOGLE_CLOUD_CPP_PUBSUB_ADMIN_API_DEPRECATED(x)="
        "GOOGLE_CLOUD_CPP_SPANNER_ADMIN_API_DEPRECATED(x)="
        "GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN="
        "GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END=")

    # Options controlling what output is generated
    set(DOXYGEN_GENERATE_LATEX NO)
    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_GENERATE_XML YES)
    set(DOXYGEN_GENERATE_TAGFILE "${CMAKE_CURRENT_BINARY_DIR}/${library}.tag")

    doxygen_add_docs(
        ${library}-docs "${opt_INPUTS}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generate ${library} HTML documentation")
    add_dependencies(doxygen-docs ${library}-docs)
    # Extra dependencies needed by this subproject's docs target.
    if (opt_DEPENDS)
        add_dependencies(${library}-docs ${opt_DEPENDS})
    endif ()

    if (XSLTPROC AND TARGET doxygen2docfx)
        # Create a target to compile the XML output into docfx format.
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docfx")
        add_custom_target(
            ${library}-docfx
            DEPENDS ${library}-docs doxygen2docfx
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docfx"
            COMMENT "Generate DocFX YAML for ${library}"
            COMMAND
                ${XSLTPROC} -o
                "${CMAKE_CURRENT_BINARY_DIR}/xml/${library}.doxygen.xml"
                "${CMAKE_CURRENT_BINARY_DIR}/xml/combine.xslt"
                "${CMAKE_CURRENT_BINARY_DIR}/xml/index.xml"
            COMMAND
                doxygen2docfx
                "${CMAKE_CURRENT_BINARY_DIR}/xml/${library}.doxygen.xml"
                "${library}" "${PROJECT_VERSION}")
        add_dependencies(all-docfx ${library}-docfx)
    endif ()
endfunction ()

function (google_cloud_cpp_doxygen_targets library)
    cmake_parse_arguments(opt "THREADED" "" "DEPENDS" ${ARGN})
    if (opt_THREADED)
        set(DOXYGEN_THREADED THREADED)
    endif ()

    google_cloud_cpp_doxygen_deploy_version(VERSION)
    set(GOOGLE_CLOUD_CPP_COMMON_TAG
        "${PROJECT_BINARY_DIR}/google/cloud/cloud.tag")
    google_cloud_cpp_doxygen_targets_impl(
        "${library}"
        RECURSIVE
        ${DOXYGEN_THREADED}
        INPUTS
        "${CMAKE_CURRENT_SOURCE_DIR}"
        DEPENDS
        ${opt_DEPENDS}
        TAGFILES
        "${GOOGLE_CLOUD_CPP_COMMON_TAG}=https://cloud.google.com/cpp/docs/reference/common/${VERSION}/"
    )
endfunction ()

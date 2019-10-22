# ~~~
# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ~~~

# Find out the name of the subproject.
get_filename_component(GOOGLE_CLOUD_CPP_SUBPROJECT
                       "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

# Find out what flags turn on all available warnings and turn those warnings
# into errors.
include(CheckCXXCompilerFlag)
if (NOT MSVC)
    check_cxx_compiler_flag(-Wall GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_WALL)
    check_cxx_compiler_flag(-Wextra GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_WEXTRA)
    check_cxx_compiler_flag(-Werror GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_WERROR)
else ()
    check_cxx_compiler_flag("/std:c++latest"
                            GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_CPP_LATEST)
endif ()

# If possible, enable a code coverage build type.
include(EnableCoverage)

# Include support for clang-tidy if available
include(EnableClangTidy)

# C++ Exceptions are enabled by default, but allow the user to turn them off.
include(EnableCxxExceptions)

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Discover and add targets for the GTest::gtest and GTest::gmock libraries.
include(FindGMockWithTargets)

# Pick the right MSVC runtime libraries.
include(SelectMSVCRuntime)

if (${CMAKE_VERSION} VERSION_LESS "3.9")

    # Old versions of CMake have really poor support for Doxygen generation.
    message(STATUS "Doxygen generation only enabled for cmake 3.9 and higher")
else ()
    # Use externalproject_add() to download the doxygen tag file. That way it is
    # done only once, only if it is required by the build, and there is an
    # automated retry loop.
    if (NOT TARGET google-cloud-cpp-common-tag)
        include(ExternalProject)
        ExternalProject_Add(
            google-cloud-cpp-common-tag
            URL "https://cloud-cpp-doxygen-resources.storage.googleapis.com/google-cloud-common.tag"
            PREFIX "${PROJECT_BINARY_DIR}/tags"
                   DOWNLOAD_NO_EXTRACT
                   1
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND "")
        ExternalProject_Get_Property(google-cloud-cpp-common-tag DOWNLOAD_DIR)
    endif ()

    find_package(Doxygen)
    if (Doxygen_FOUND)
        set(DOXYGEN_RECURSIVE YES)
        set(DOXYGEN_FILE_PATTERNS *.h *.cc *.proto *.dox)
        set(DOXYGEN_EXAMPLE_RECURSIVE YES)
        set(DOXYGEN_EXCLUDE "third_party" "cmake-build-debug" "cmake-out")
        set(DOXYGEN_EXCLUDE_SYMLINKS YES)
        set(DOXYGEN_QUIET YES)
        set(DOXYGEN_WARN_AS_ERROR YES)
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
        set(DOXYGEN_CLANG_ASSISTED_PARSING YES)
        set(DOXYGEN_CLANG_OPTIONS "-std=c++11")
        set(DOXYGEN_SEARCH_INCLUDES YES)
        set(DOXYGEN_INCLUDE_PATH "${PROJECT_SOURCE_DIR}"
                                 "${PROJECT_BINARY_DIR}")
        set(DOXYGEN_GENERATE_LATEX NO)
        set(DOXYGEN_GRAPHICAL_HIERARCHY NO)
        set(DOXYGEN_DIRECTORY_GRAPH NO)
        set(DOXYGEN_CLASS_GRAPH NO)
        set(DOXYGEN_COLLABORATION_GRAPH NO)
        set(DOXYGEN_INCLUDE_GRAPH NO)
        set(DOXYGEN_INCLUDED_BY_GRAPH NO)
        set(DOXYGEN_DOT_TRANSPARENT YES)
        set(DOXYGEN_MACRO_EXPANSION YES)
        set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
        set(DOXYGEN_HTML_TIMESTAMP)
        set(DOXYGEN_STRIP_FROM_INC_PATH "${PROJECT_SOURCE_DIR}")
        set(DOXYGEN_SHOW_USED_FILES NO)
        set(DOXYGEN_REFERENCES_LINK_SOURCE NO)
        set(DOXYGEN_SOURCE_BROWSER YES)
        set(DOXYGEN_GENERATE_TAGFILE
            "${CMAKE_CURRENT_BINARY_DIR}/${GOOGLE_CLOUD_CPP_SUBPROJECT}.tag")
        set(DOXYGEN_LAYOUT_FILE
            "${PROJECT_SOURCE_DIR}/doc/config/DoxygenLayout.xml")

        set(GOOGLE_CLOUD_CPP_COMMON_TAG
            "${PROJECT_BINARY_DIR}/tags/src/google-cloud-common.tag")
        if (NOT "${GOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV}")
            set(DOXYGEN_TAGFILES
                "${GOOGLE_CLOUD_CPP_COMMON_TAG}=https://googleapis.dev/google-cloud-common/master/"
            )
        elseif (NOT "${GOOGLE_CLOUD_CPP_USE_MASTER_FOR_REFDOC_LINKS}")
            set(DOXYGEN_TAGFILES
                "${GOOGLE_CLOUD_CPP_COMMON_TAG}=https://googleapis.dev/google-cloud-common/master/"
            )
        else ()
            set(DOXYGEN_TAGFILES
                "${GOOGLE_CLOUD_CPP_COMMON_TAG}=../../google-cloud-common/master/"
            )
        endif ()

        doxygen_add_docs(
            ${GOOGLE_CLOUD_CPP_SUBPROJECT}-docs ${CMAKE_CURRENT_SOURCE_DIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMENT
            "Generate HTML documentation")
        add_dependencies(doxygen-docs ${GOOGLE_CLOUD_CPP_SUBPROJECT}-docs)
        add_dependencies(${GOOGLE_CLOUD_CPP_SUBPROJECT}-docs
                         google-cloud-cpp-common-tag)

    endif ()
endif ()

function (google_cloud_cpp_add_common_options target)
    if (GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_CPP_LATEST)
        target_compile_options(${target} INTERFACE "/std:c++latest")
    endif ()
    if (GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_WALL)
        target_compile_options(${target} INTERFACE "-Wall")
    endif ()
    if (GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_WEXTRA)
        target_compile_options(${target} INTERFACE "-Wextra")
    endif ()
    if (GOOGLE_CLOUD_CPP_COMPILER_SUPPORTS_WERROR)
        target_compile_options(${target} INTERFACE "-Werror")
    endif ()
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"
        AND "${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 5.0)
        # With GCC 4.x this warning is too noisy to be useful.
        target_compile_options(${target}
                               INTERFACE "-Wno-missing-field-initializers")
    endif ()
endfunction ()

function (google_cloud_cpp_add_clang_tidy target)
    if (GOOGLE_CLOUD_CPP_CLANG_TIDY_PROGRAM AND GOOGLE_CLOUD_CPP_CLANG_TIDY)
        set_target_properties(
            ${target} PROPERTIES CXX_CLANG_TIDY
                                 "${GOOGLE_CLOUD_CPP_CLANG_TIDY_PROGRAM}")
    endif ()
endfunction ()

#
# google_cloud_cpp_install_headers : install all the headers in a target
#
# Find all the headers in @p target and install them at @p destination,
# preserving the directory structure.
#
# * target the name of the target.
# * destination the destination directory, relative to <PREFIX>. Typically this
#   starts with `include/google/cloud`, the function requires the full
#   destination in case some headers get installed elsewhere in the future.
#
function (google_cloud_cpp_install_headers target destination)
    get_target_property(target_sources ${target} SOURCES)
    foreach (header ${target_sources})
        if (NOT "${header}" MATCHES "\\.h$" AND NOT "${header}" MATCHES
                                                "\\.inc$")
            continue()
        endif ()
        string(REPLACE "${CMAKE_CURRENT_BINARY_DIR}/" "" relative "${header}")
        get_filename_component(dir "${relative}" DIRECTORY)
        install(FILES "${header}" DESTINATION "${destination}/${dir}")
    endforeach ()
endfunction ()

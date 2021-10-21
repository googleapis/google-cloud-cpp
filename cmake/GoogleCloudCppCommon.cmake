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

# C++ Exceptions are enabled by default, but allow the user to turn them off.
include(EnableCxxExceptions)

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Discover and add targets for the GTest::gtest and GTest::gmock libraries.
include(FindGMockWithTargets)

# Pick the right MSVC runtime libraries.
include(SelectMSVCRuntime)

# Enable Werror
include(EnableWerror)

if (${CMAKE_VERSION} VERSION_LESS "3.12")
    # Old versions of CMake have really poor support for Doxygen generation.
    message(STATUS "Doxygen generation only enabled for cmake 3.9 and higher")
else ()
    find_package(Doxygen)
    if (Doxygen_FOUND)
        set(DOXYGEN_RECURSIVE YES)
        set(DOXYGEN_FILE_PATTERNS *.h *.cc *.dox)
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
        set(DOXYGEN_CLANG_OPTIONS)
        set(DOXYGEN_CLANG_DATABASE_PATH)
        set(DOXYGEN_SEARCH_INCLUDES YES)
        set(DOXYGEN_INCLUDE_PATH
            "${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}"
            "${PROJECT_BINARY_DIR}/google/cloud/examples"
            "${PROJECT_BINARY_DIR}/external/googleapis")
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
        # Macros confuse Doxygen. We don't use too many, so we predefine them
        # here to be noops or have simple values.
        set(DOXYGEN_PREDEFINED
            "GOOGLE_CLOUD_CPP_DEPRECATED(x)="
            "GOOGLE_CLOUD_CPP_IAM_DEPRECATED(x)="
            "GOOGLE_CLOUD_CPP_BIGTABLE_ADMIN_ASYNC_DEPRECATED(x)="
            "GOOGLE_CLOUD_CPP_BIGTABLE_IAM_DEPRECATED(x)="
            "GOOGLE_CLOUD_CPP_BIGTABLE_ASYNC_CQ_PARAM_DEPRECATED(x)="
            "GOOGLE_CLOUD_CPP_BIGTABLE_ASYNC_CQ_PARAM_DEPRECATED(x)="
            "GOOGLE_CLOUD_CPP_GENERATED_NS=v${PROJECT_VERSION_MAJOR}"
            "GOOGLE_CLOUD_CPP_NS=v${PROJECT_VERSION_MAJOR}"
            "GOOGLE_CLOUD_CPP_PUBSUB_NS=v${PROJECT_VERSION_MAJOR}"
            "BIGTABLE_CLIENT_NS=v${PROJECT_VERSION_MAJOR}"
            "SPANNER_CLIENT_NS=v${PROJECT_VERSION_MAJOR}"
            "STORAGE_CLIENT_NS=v${PROJECT_VERSION_MAJOR}")
        set(DOXYGEN_HTML_TIMESTAMP YES)
        set(DOXYGEN_STRIP_FROM_INC_PATH "${PROJECT_SOURCE_DIR}")
        set(DOXYGEN_SHOW_USED_FILES NO)
        set(DOXYGEN_REFERENCES_LINK_SOURCE NO)
        set(DOXYGEN_SOURCE_BROWSER YES)
        set(DOXYGEN_DISTRIBUTE_GROUP_DOC YES)
        set(DOXYGEN_GENERATE_TAGFILE
            "${CMAKE_CURRENT_BINARY_DIR}/${GOOGLE_CLOUD_CPP_SUBPROJECT}.tag")
        set(DOXYGEN_LAYOUT_FILE
            "${PROJECT_SOURCE_DIR}/ci/etc/doxygen/DoxygenLayout.xml")
        set(GOOGLE_CLOUD_CPP_COMMON_TAG
            "${PROJECT_BINARY_DIR}/google/cloud/cloud.tag")
        if (NOT ("cloud" STREQUAL "${GOOGLE_CLOUD_CPP_SUBPROJECT}"))
            if (NOT "${GOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV}")
                set(DOXYGEN_TAGFILES
                    "${GOOGLE_CLOUD_CPP_COMMON_TAG}=https://googleapis.dev/cpp/google-cloud-common/latest/"
                )
            elseif (NOT "${GOOGLE_CLOUD_CPP_USE_MASTER_FOR_REFDOC_LINKS}")
                set(DOXYGEN_TAGFILES
                    "${GOOGLE_CLOUD_CPP_COMMON_TAG}=https://googleapis.dev/cpp/google-cloud-common/latest/"
                )
            else ()
                set(DOXYGEN_TAGFILES
                    "${GOOGLE_CLOUD_CPP_COMMON_TAG}=../../html/")
            endif ()
        endif ()

        doxygen_add_docs(
            ${GOOGLE_CLOUD_CPP_SUBPROJECT}-docs ${CMAKE_CURRENT_SOURCE_DIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMENT
            "Generate ${GOOGLE_CLOUD_CPP_SUBPROJECT} HTML documentation")
        add_dependencies(doxygen-docs ${GOOGLE_CLOUD_CPP_SUBPROJECT}-docs)

        # Extra dependencies needed by this subproject's docs target.
        if (GOOGLE_CLOUD_CPP_DOXYGEN_DEPS)
            add_dependencies(${GOOGLE_CLOUD_CPP_SUBPROJECT}-docs
                             ${GOOGLE_CLOUD_CPP_DOXYGEN_DEPS})
        endif ()
        if (NOT ("cloud" STREQUAL "${GOOGLE_CLOUD_CPP_SUBPROJECT}"))
            add_dependencies(${GOOGLE_CLOUD_CPP_SUBPROJECT}-docs "cloud-docs")
        endif ()
    endif ()
endif ()

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
    get_target_property(target_type ${target} TYPE)
    if ("${target_type}" STREQUAL "INTERFACE_LIBRARY")
        # For interface libraries we use `INTERFACE_SOURCES` to get the list of
        # sources, which are actually just headers in this case.
        get_target_property(target_sources ${target} INTERFACE_SOURCES)
    else ()
        get_target_property(target_sources ${target} SOURCES)
    endif ()
    foreach (header ${target_sources})
        if (NOT "${header}" MATCHES "\\.h$" AND NOT "${header}" MATCHES
                                                "\\.inc$")
            continue()
        endif ()
        # Sometimes we generate header files into the binary directory, do not
        # forget to install those with their relative path.
        string(REPLACE "${CMAKE_CURRENT_BINARY_DIR}/" "" relative "${header}")
        # INTERFACE libraries use absolute paths, yuck.
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" relative "${relative}")
        get_filename_component(dir "${relative}" DIRECTORY)
        install(
            FILES "${header}"
            DESTINATION "${destination}/${dir}"
            COMPONENT google_cloud_cpp_development)
    endforeach ()
endfunction ()

#
# google_cloud_cpp_set_target_name : formats the prefix and path as a target
#
# The formatted target name looks like "<prefix>_<basename>" where <basename> is
# computed from the path. A 4th argument may optionally be specified, which
# should be the name of a variable in the parent's scope where the <basename>
# should be set. This is useful only if the caller wants both the target name
# and the basename.
#
# * target the name of the variable to be set in the parent scope to hold the
#   target name.
# * prefix a unique string to prepend to the target name. Usually this should be
#   a string indicating the product, such as "pubsub" or "storage".
# * path is the filename that should be used for the target.
# * (optional) basename the name of a variable to be set in the parent scope
#   containing the basename of the target.
#
function (google_cloud_cpp_set_target_name target prefix path)
    string(REPLACE "/" "_" basename "${path}")
    string(REPLACE ".cc" "" basename "${basename}")
    set("${target}"
        "${prefix}_${basename}"
        PARENT_SCOPE)
    # Optional 4th argument, will be set to the basename if present.
    if (ARGC EQUAL 4)
        set("${ARGV3}"
            "${basename}"
            PARENT_SCOPE)
    endif ()
endfunction ()

#
# google_cloud_cpp_add_executable : adds an executable w/ the given source and
# prefix name
#
# Computes the target name using `google_cloud_cpp_set_target_name` (see above),
# then adds an executable with a few common properties. Sets the `target` in the
# caller's scope to the name of the computed target name.
#
# * target the name of the variable to be set in the parent scope to hold the
#   target name.
# * prefix a unique string to prepend to the target name. Usually this should be
#   a string indicating the product, such as "pubsub" or "storage".
# * path is the filename that should be used for the target.
#
function (google_cloud_cpp_add_executable target prefix source)
    google_cloud_cpp_set_target_name(target_name "${prefix}" "${source}"
                                     shortname)
    add_executable("${target_name}" "${source}")
    set_target_properties("${target_name}" PROPERTIES OUTPUT_NAME ${shortname})
    set("${target}"
        "${target_name}"
        PARENT_SCOPE)
endfunction ()

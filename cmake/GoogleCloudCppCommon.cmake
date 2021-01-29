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
        set(DOXYGEN_FILE_PATTERNS *.h *.cc *.proto *.dox)
        set(DOXYGEN_EXAMPLE_RECURSIVE YES)
        set(DOXYGEN_EXCLUDE "third_party" "cmake-build-debug" "cmake-out")
        set(DOXYGEN_EXCLUDE_SYMLINKS YES)
        set(DOXYGEN_QUIET YES)
        set(DOXYGEN_WARN_AS_ERROR NO)
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
        set(DOXYGEN_CLANG_ASSISTED_PARSING NO)
        set(DOXYGEN_CLANG_OPTIONS "")
        set(DOXYGEN_CLANG_DATABASE_PATH ${CMAKE_BINARY_DIR})
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
        set(DOXYGEN_DISTRIBUTE_GROUP_DOC YES)
        set(DOXYGEN_GENERATE_TAGFILE
            "${CMAKE_CURRENT_BINARY_DIR}/${GOOGLE_CLOUD_CPP_SUBPROJECT}.tag")
        set(DOXYGEN_LAYOUT_FILE
            "${PROJECT_SOURCE_DIR}/doc/config/DoxygenLayout.xml")
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

#
# google_cloud_cpp_absl_pkg_config : finds the right -l options for Abseil
# dependencies.
#
# Finds the necessary `-l` flags to link Abseil (direct) dependencies of a
# target. First discover all the names of any necessary abseil libraries, in the
# correct order for the link line (first the library, then anything it depends
# on). Remove duplicates in reverse order.
#
function (google_cloud_cpp_absl_pkg_config var target)
    google_cloud_cpp_absl_pkg_config_all_basenames(bases ${target} 0)
    list(REVERSE bases)
    list(REMOVE_DUPLICATES bases)
    list(REVERSE bases)
    set(result)
    foreach (b ${bases})
        list(APPEND result " -l${b}")
    endforeach ()
    set("${var}"
        "${result}"
        PARENT_SCOPE)
endfunction ()

#
# google_cloud_cpp_absl_pkg_config_basename : finds the basename (if any) for an
# imported library target
#
# Computes the basename (i.e. the portion of the name suitable for -l flags) of
# an imported static or shared library target.  All Abseil libraries are
# imported library targets, but some are interface libraries that do not need -l
# flags.
#
# Discovering the name requires extracting the right location property from the
# target, which varies depending on the build type.
#
# Cleaning up the imported location (which is an actual filename)
#
function (google_cloud_cpp_absl_pkg_config_basename var target)
    set("${var}"
        ""
        PARENT_SCOPE)
    if (NOT TARGET ${target})
        return()
    endif ()
    get_target_property(type "${target}" TYPE)
    set(valid_types "STATIC_LIBRARY" "SHARED_LIBRARY")
    if ("${type}" IN_LIST valid_types)
        set(location_properties)
        list(APPEND location_properties IMPORTED_LOCATION_NOCONFIG)
        list(APPEND location_properties IMPORTED_LOCATION)
        foreach (c "${CMAKE_BUILD_TYPE}" Debug Release RelWithDebInfo
                   MinSizeRel ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER "${c}" CONFIG)
            list(APPEND location_properties IMPORTED_LOCATION_${CONFIG})
        endforeach ()
        list(REMOVE_DUPLICATES location_properties)
        foreach (property ${location_properties})
            get_target_property(location "${target}" ${property})
            if (location)
                get_filename_component(base "${location}" NAME)
                foreach (
                    decoration
                    CMAKE_STATIC_LIBRARY_PREFIX
                    CMAKE_STATIC_LIBRARY_SUFFIX
                    CMAKE_SHARED_LIBRARY_PREFIX
                    CMAKE_SHARED_LIBRARY_SUFFIX
                    CMAKE_STATIC_LIBRARY_PREFIX_CXX
                    CMAKE_STATIC_LIBRARY_SUFFIX_CXX
                    CMAKE_SHARED_LIBRARY_PREFIX_CXX
                    CMAKE_SHARED_LIBRARY_SUFFIX_CXX)
                    if ("${${decoration}}" STREQUAL "")
                        continue()
                    endif ()
                    string(REPLACE "${${decoration}}" "" base "${base}")
                endforeach ()
                set("${var}"
                    "${base}"
                    PARENT_SCOPE)
                return()
            endif ()
        endforeach ()
    endif ()
endfunction ()

# google_cloud_cpp_absl_pkg_config_all_basenames : the required -l flag values
# for all (direct) Abseil dependencies.
#
# To link a direct Abseil dependency with pkg-config we must build a list of -l
# flags for this dependency and all its (indirect) dependencies.
function (google_cloud_cpp_absl_pkg_config_all_basenames var target level)
    set("${var}"
        ""
        PARENT_SCOPE)
    if (NOT TARGET ${target})
        return()
    endif ()
    get_target_property(type "${target}" TYPE)
    get_target_property(linked_libraries ${target} INTERFACE_LINK_LIBRARIES)
    google_cloud_cpp_absl_pkg_config_basename(result "${target}")
    foreach (lib ${linked_libraries})
        string(SUBSTRING "${lib}" 0 6 prefix)
        if (NOT ("${prefix}" STREQUAL "absl::"))
            continue()
        endif ()
        math(EXPR next_level "${level} + 1")
        google_cloud_cpp_absl_pkg_config_all_basenames(sub ${lib} ${next_level})
        if (NOT "${sub}" STREQUAL "")
            list(APPEND result ${sub})
        endif ()
    endforeach ()
    set("${var}"
        "${result}"
        PARENT_SCOPE)
endfunction ()

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

include(GNUInstallDirs)

set(GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX "${CMAKE_BINARY_DIR}/external"
    CACHE STRING "Configure where the external projects are installed.")
mark_as_advanced(GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX)

function (set_library_properties_for_external_project _target _lib)
    cmake_parse_arguments(F_OPT "ALWAYS_SHARED;ALWAYS_LIB" "" "" ${ARGN})
    # This is the main disadvantage of external projects. The typicaly flow with
    # CMake is:
    # ~~~
    # 1. Configure: cmake discovers where your libraries and dependencies are,
    #    and it generates Makefiles (or Ninja files or VisualStudio files) to
    #    compile the code. The paths and/or flags needed for the dependencies
    #    are embedded in the generated files.
    # 2. Compile: make, Ninja or VisualStudio compile your code.
    # ~~~
    #
    # With external projects the flow is the same, except that step 2 has
    # additional sub-steps:
    #
    # ~~~
    # 1. Configure: as above. But the external projects are not "discovered".
    # 2. Compile: make, Ninja, or Visual Studio compile your code. But new
    #    steps are introduced to the compilation:
    # 2.1: Download any external projects that your code depends on.
    # 2.2: Compile (and maybe test) those external projects.
    # 2.3: Install those external projects in the ${CMAKE_BINARY_DIR}/...
    # 2.4: Compile your code.
    # ~~~
    #
    # We cannot use an external project's cmake configuration file because they
    # are not created nor installed until after the project is compiled.
    # However, these files would be needed in step 1 if we want to use them in
    # CMake.
    #
    # The alternative is to manually create "IMPORTED" libraries with the right
    # paths. Generally this is possible because we control how the external
    # projects are compiled and installed.
    if ("${BUILD_SHARED_LIBS}" OR "${F_OPT_ALWAYS_SHARED}")
        set(
            _libfullname
            "${CMAKE_SHARED_LIBRARY_PREFIX}${_lib}${CMAKE_SHARED_LIBRARY_SUFFIX}"
            )
    else()
        set(
            _libfullname
            "${CMAKE_STATIC_LIBRARY_PREFIX}${_lib}${CMAKE_STATIC_LIBRARY_SUFFIX}"
            )
    endif ()

    if (${F_OPT_ALWAYS_LIB})
        set(_libpath "${CMAKE_BINARY_DIR}/external/lib/${_libfullname}")
    else()
        set(
            _libpath
            "${CMAKE_BINARY_DIR}/external/${CMAKE_INSTALL_LIBDIR}/${_libfullname}"
            )
    endif ()

    set(_includepath "${CMAKE_BINARY_DIR}/external/include")
    message(STATUS "Configuring ${_target} with ${_libpath}")
    set_property(TARGET ${_target}
                 APPEND
                 PROPERTY INTERFACE_LINK_LIBRARIES "${_libpath}")
    # Manually create the directory, it will be created as part of the build,
    # but this runs in the configuration phase, and CMake generates an error if
    # we add an include directory that does not exist yet.
    file(MAKE_DIRECTORY "${_includepath}")
    set_property(TARGET ${_target}
                 APPEND
                 PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${_includepath}")
endfunction ()

function (set_executable_name_for_external_project _target _exe)
    set_property(
        TARGET ${_target}
        PROPERTY
            IMPORTED_LOCATION
            "${CMAKE_BINARY_DIR}/external/bin/${_exe}${CMAKE_EXECUTABLE_SUFFIX}"
        )
endfunction ()

function (create_external_project_library_byproduct_list var_name)
    cmake_parse_arguments(_BYPRODUCT_OPT "ALWAYS_SHARED" "" "" ${ARGN})
    if ("${BUILD_SHARED_LIBS}" OR "${_BYPRODUCT_OPT_ALWAYS_SHARED}")
        set(_byproduct_prefix "${CMAKE_SHARED_LIBRARY_PREFIX}")
        set(_byproduct_suffix "${CMAKE_SHARED_LIBRARY_SUFFIX}")
    else()
        set(_byproduct_prefix "${CMAKE_STATIC_LIBRARY_PREFIX}")
        set(_byproduct_suffix "${CMAKE_STATIC_LIBRARY_SUFFIX}")
    endif ()

    set(_decorated_byproduct_names)
    foreach (lib ${_BYPRODUCT_OPT_UNPARSED_ARGUMENTS})
        list(
            APPEND
                _decorated_byproduct_names
                "<INSTALL_DIR>/lib/${_byproduct_prefix}${lib}${_byproduct_suffix}"
            )
        list(
            APPEND
                _decorated_byproduct_names
                "<INSTALL_DIR>/lib/${_byproduct_prefix}${lib}d${_byproduct_suffix}"
            )
    endforeach ()

    set(${var_name} ${_decorated_byproduct_names} PARENT_SCOPE)
endfunction ()

function (set_external_project_build_parallel_level var_name)
    if ("${CMAKE_GENERATOR}" STREQUAL "Unix Makefiles"
        OR "${CMAKE_GENERATOR}" STREQUAL "Ninja")
        if (DEFINED ENV{NCPU})
            set(${var_name} "--" "-j" "$ENV{NCPU}" PARENT_SCOPE)
        else()
            include(ProcessorCount)
            processorcount(NCPU)
            set(${var_name} "--" "-j" "${NCPU}" PARENT_SCOPE)
        endif ()
    else()
        set(${var_name} "" PARENT_SCOPE)
    endif ()
endfunction ()

function (set_external_project_prefix_vars)
    set(GOOGLE_CLOUD_CPP_INSTALL_RPATH "<INSTALL_DIR>/lib;<INSTALL_DIR>/lib64")

    # On Linux, using an RPATH that is neither an absolute or relative path is
    # considered a security risk and will cause package building to fail. We use
    # the Linux-specific variable $ORIGIN to resolve this.
    if (UNIX AND NOT APPLE)
        set(GOOGLE_CLOUD_CPP_INSTALL_RPATH
            "\\\$ORIGIN/../lib;\\\$ORIGIN/../lib64")
    endif ()

    # When passing a semi-colon delimited list to ExternalProject_Add, we need
    # to escape the semi-colon. Quoting does not work and escaping the semi-
    # colon does not seem to work (see https://reviews.llvm.org/D40257). A
    # workaround is to use LIST_SEPARATOR to change the delimiter, which will
    # then be replaced by an escaped semi-colon by CMake. This allows us to use
    # multiple directories for our RPATH. Normally, it'd make sense to use : as
    # a delimiter since it is a typical path-list separator, but it is a special
    # character in CMake.
    string(REPLACE ";"
                   "|"
                   GOOGLE_CLOUD_CPP_INSTALL_RPATH
                   "${GOOGLE_CLOUD_CPP_INSTALL_RPATH}")

    set(GOOGLE_CLOUD_CPP_PREFIX_PATH "${CMAKE_PREFIX_PATH};<INSTALL_DIR>")
    string(REPLACE ";"
                   "|"
                   GOOGLE_CLOUD_CPP_PREFIX_PATH
                   "${GOOGLE_CLOUD_CPP_PREFIX_PATH}")

    set(GOOGLE_CLOUD_CPP_PREFIX_PATH "${GOOGLE_CLOUD_CPP_PREFIX_PATH}"
        PARENT_SCOPE)
    set(GOOGLE_CLOUD_CPP_PREFIX_RPATH "${GOOGLE_CLOUD_CPP_PREFIX_RPATH}"
        PARENT_SCOPE)
endfunction ()

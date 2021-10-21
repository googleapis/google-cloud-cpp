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

include(ExternalProject)
include(GNUInstallDirs)

message(
    WARNING
        [==[
  Super builds are no longer recommended, and will be removed shortly
  after 2021-05-01.  If you want to automatically download and build
  the `google-cloud-cpp` dependencies we recommend that you use
  `vcpkg` as described in
  /doc/contributor/howto-guide-setup-cmake-environment.md
]==])

set(GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX
    "${CMAKE_BINARY_DIR}/external"
    CACHE STRING "Configure where the external projects are installed.")
mark_as_advanced(GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX)

function (set_external_project_build_parallel_level var_name)
    if ("${CMAKE_GENERATOR}" STREQUAL "Unix Makefiles" OR "${CMAKE_GENERATOR}"
                                                          STREQUAL "Ninja")
        if (DEFINED ENV{NCPU})
            set(${var_name}
                "--" "-j" "$ENV{NCPU}"
                PARENT_SCOPE)
        else ()
            include(ProcessorCount)
            processorcount(NCPU)
            set(${var_name}
                "--" "-j" "${NCPU}"
                PARENT_SCOPE)
        endif ()
    else ()
        set(${var_name}
            ""
            PARENT_SCOPE)
    endif ()
endfunction ()

function (set_external_project_vars)
    set(GOOGLE_CLOUD_CPP_INSTALL_RPATH "<INSTALL_DIR>/lib;<INSTALL_DIR>/lib64")

    # On Linux, using an RPATH that is neither an absolute or relative path is
    # considered a security risk and will cause package building to fail. We use
    # the Linux-specific variable $ORIGIN to resolve this.
    if (UNIX AND NOT APPLE)
        set(GOOGLE_CLOUD_CPP_INSTALL_RPATH
            "\\\$ORIGIN/../lib;\\\$ORIGIN/../lib64")
    endif ()

    set(GOOGLE_CLOUD_CPP_PREFIX_PATH
        "${CMAKE_PREFIX_PATH}" "${GOOGLE_CLOUD_CPP_EXTERNAL_PREFIX}"
        "<INSTALL_DIR>")

    set(GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CMAKE_FLAGS
        ${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS})

    # When passing a semi-colon delimited list to ExternalProject_Add, we need
    # to escape the semi-colon. Quoting does not work and escaping the semi-
    # colon does not seem to work (see https://reviews.llvm.org/D40257). A
    # workaround is to use LIST_SEPARATOR to change the delimiter, which will
    # then be replaced by an escaped semi-colon by CMake. This allows us to use
    # multiple directories for our RPATH. Normally, it'd make sense to use : as
    # a delimiter since it is a typical path-list separator, but it is a special
    # character in CMake.
    string(REPLACE ";" "|" GOOGLE_CLOUD_CPP_INSTALL_RPATH
                   "${GOOGLE_CLOUD_CPP_INSTALL_RPATH}")
    string(REPLACE ";" "|" GOOGLE_CLOUD_CPP_PREFIX_PATH
                   "${GOOGLE_CLOUD_CPP_PREFIX_PATH}")

    set(GOOGLE_CLOUD_CPP_INSTALL_RPATH
        "${GOOGLE_CLOUD_CPP_INSTALL_RPATH}"
        PARENT_SCOPE)
    set(GOOGLE_CLOUD_CPP_PREFIX_PATH
        "${GOOGLE_CLOUD_CPP_PREFIX_PATH}"
        PARENT_SCOPE)
    set(GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CMAKE_FLAGS
        "${GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CMAKE_FLAGS}"
        PARENT_SCOPE)

endfunction ()

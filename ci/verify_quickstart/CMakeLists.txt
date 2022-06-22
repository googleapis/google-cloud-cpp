# ~~~
# Copyright 2021 Google LLC
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

cmake_minimum_required(VERSION 3.14)
project(verify-quickstart CXX)

# The list of libraries must be provided in the command line
if (NOT LIBRARIES)
    message(FATAL_ERROR "Missing -DLIBRARIES argument, for example:"
                        " -DLIBRARIES=\"storage;spanner\"")
endif ()

add_custom_target(verify-quickstart-cmake ALL)
add_custom_target(verify-quickstart-make ALL)

include(ExternalProject)

function (add_cmake_quickstart_target feature library target)
    set(COMMON_CMAKE_ARGS)
    if (CMAKE_PREFIX_PATH)
        list(APPEND COMMON_CMAKE_ARGS
             "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")
    endif ()
    if (CMAKE_TOOLCHAIN_FILE)
        list(APPEND COMMON_CMAKE_ARGS
             "-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
    endif ()

    externalproject_add(
        verify-quickstart-cmake-${feature}
        EXCLUDE_FROM_ALL ON
        PREFIX "${PROJECT_BINARY_DIR}/cmake-${feature}-prefix"
        SOURCE_DIR
            "${PROJECT_SOURCE_DIR}/../../google/cloud/${library}/quickstart"
        BINARY_DIR "${PROJECT_BINARY_DIR}/cmake-${feature}"
        CMAKE_ARGS ${COMMON_CMAKE_ARGS}
        BUILD_COMMAND "${CMAKE_COMMAND}" "--build" "<BINARY_DIR>" "--target"
                      "${target}"
        INSTALL_COMMAND ""
        # Use LOG_* ON to keep builds quiet on success
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_OUTPUT_ON_FAILURE ON)
    add_dependencies(verify-quickstart-cmake verify-quickstart-cmake-${feature})
endfunction ()

function (add_make_quickstart_target feature library target)
    externalproject_add(
        verify-quickstart-makefile-${feature}
        EXCLUDE_FROM_ALL ON
        PREFIX "${PROJECT_BINARY_DIR}/makefile-${feature}-prefix"
        SOURCE_DIR
            "${PROJECT_SOURCE_DIR}/../../google/cloud/${library}/quickstart"
        BINARY_DIR "${PROJECT_BINARY_DIR}/makefile-${feature}"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND
            "env"
            "PKG_CONFIG_PATH=${CMAKE_PREFIX_PATH}/lib64/pkgconfig:${CMAKE_PREFIX_PATH}/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}"
            "make"
            "-C"
            "<SOURCE_DIR>"
            "BIN=<BINARY_DIR>"
            "<BINARY_DIR>/${target}"
        INSTALL_COMMAND ""
        # Use LOG_* ON to keep builds quiet on success
        LOG_DOWNLOAD ON
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_OUTPUT_ON_FAILURE ON)
    add_dependencies(verify-quickstart-make
                     verify-quickstart-makefile-${feature})
endfunction ()

foreach (feature IN LISTS LIBRARIES)
    if ("${feature}" STREQUAL "experimental-storage-grpc")
        add_cmake_quickstart_target("${feature}" storage quickstart_grpc)
        add_make_quickstart_target("${feature}" storage quickstart_grpc)
        continue()
    endif ()
    add_cmake_quickstart_target("${feature}" "${feature}" quickstart)
    add_make_quickstart_target("${feature}" "${feature}" quickstart)
endforeach ()

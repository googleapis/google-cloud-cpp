# ~~~
# Copyright 2017 Google Inc.
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

cmake_minimum_required(VERSION 3.5)

# Define the project name and where to report bugs.
set(PACKAGE_BUGREPORT
    "https://github.com/GoogleCloudPlatform/google-cloud-cpp/issues")
project(google-cloud-cpp CXX C)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

    # gcc-4.8 C++11 support has bugs preventing us from using it.
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
        message(
            FATAL_ERROR
                "GCC version must be at least 4.9. Older versions"
                " either lack C++11 support or have bugs"
                " (e.g. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55817)"
                " that prevent us from using them.")
    endif ()
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

    # Before Clang-3.8 we ran into bugs with std::async() and exceptions.
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.8)
        message(
            FATAL_ERROR
                "Clang version must be at least 3.8. Older versions"
                " either lack C++11 support or have bugs that prevent us from"
                " using them.")
    endif ()
endif ()

# If ccache is installed use it for the build. This makes the Travis
# configuration agnostic as to wether ccache is installed or not.
option(
    GOOGLE_CLOUD_CPP_ENABLE_CCACHE "Automatically use ccache if available" ON)

if ("${GOOGLE_CLOUD_CPP_ENABLE_CCACHE}")
    find_program(CCACHE_FOUND ccache NAMES /usr/bin/ccache)
    if (CCACHE_FOUND)
        message(STATUS "ccache found: ${CCACHE_FOUND}")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_FOUND}")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_FOUND}")

        set(GOOGLE_CLOUD_CPP_EXTERNAL_PROJECT_CCACHE
            "-DCMAKE_CXX_COMPILER_LAUNCHER=${CCACHE_FOUND}"
            "-DCMAKE_CC_COMPILER_LAUNCHER=${CCACHE_FOUND}")
    endif ()
endif ()

set(PROJECT_THIRD_PARTY_DIR "${PROJECT_SOURCE_DIR}/third_party")
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Add gRPC and protobuf targets as submodules or packages, depending on the
# configuration.  This should happen before enable_testing(), because we are not
# interested in compiling and running the gRPC tests.
include(IncludeGrpc)

# Enable testing in this directory so we can do a top-level `make test`.
enable_testing()

# Each subproject adds dependencies to this target to have their docs generated.
add_custom_target(doxygen-docs)

# There are a number of targets we cannot, or do not care to compile with the
# static analyzer enabled. Either because the build times are too long, or
# because the code is an external dependency, or generated code, or all of
# above. These targets are added as dependencies of `skip-scanbuild-targets` and
# compiled with, obviously, scan-build disabled.
add_custom_target(skip-scanbuild-targets)

# Add subprojects here.
add_subdirectory(google/cloud)

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
set(PACKAGE_BUGREPORT "https://github.com/googleapis/google-cloud-cpp/issues")

# Enable support for SelectMSVCRuntime
if (NOT (CMAKE_VERSION VERSION_LESS 3.15))
    cmake_policy(SET CMP0091 NEW)
endif ()
project(
    google-cloud-cpp
    VERSION 1.31.0
    LANGUAGES CXX C)

# Configure the Compiler options, we use C++11 features by default.
set(GOOGLE_CLOUD_CPP_CXX_STANDARD
    ""
    CACHE STRING "Unused. Prefer CMAKE_CXX_STANDARD")
mark_as_advanced(GOOGLE_CLOUD_CPP_CXX_STANDARD)

set(CMAKE_CXX_STANDARD
    11
    CACHE STRING "Configure the C++ standard version for all targets.")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT ("${GOOGLE_CLOUD_CPP_CXX_STANDARD}" STREQUAL ""))
    message(
        WARNING
            "GOOGLE_CLOUD_CPP_CXX_STANDARD is retired, use CMAKE_CXX_STANDARD")
endif ()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

    # GCC >= 5.4 is required by Abseil, so we must require it too.
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.4)
        message(
            FATAL_ERROR
                "GCC version must be at least 5.4. Older versions"
                " either lack C++11 support or have bugs that prevent us from"
                " using them.")
    endif ()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

    # Clang > 3.8 is required by the versions of protobuf that can compile
    # googleapis protos. We only test with >= 6.0 because that is easy to
    # install in our CI systems.
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.0)
        message(
            FATAL_ERROR
                "Clang version must be at least 6.0. Older versions"
                " either lack C++11 support or have bugs that prevent us from"
                " using them.")
    endif ()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")

    if (CMAKE_VERSION VERSION_LESS 3.15)
        message(
            FATAL_ERROR
                "MSVC builds require CMake >= 3.15."
                " Previous versions of CMake lack a standard mechanism to"
                " select the runtime C++ library.")
    endif ()
endif ()

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(SelectMSVCRuntime)

option(GOOGLE_CLOUD_CPP_ENABLE_MACOS_OPENSSL_CHECK
       "If enabled, check that the user has defined OPENSSL_ROOT_DIR on macOS"
       ON)
if (APPLE)
    # This is an easy mistake to make, and the error messages are very
    # confusing. Help our users by giving them some guidance.
    if ("${GOOGLE_CLOUD_CPP_ENABLE_MACOS_OPENSSL_CHECK}"
        AND NOT DEFINED ENV{OPENSSL_ROOT_DIR})
        message(
            FATAL_ERROR
                [===[
The Google Cloud C++ client libraries use the native OpenSSL library. In most
macOS systems, you need to set the OPENSSL_ROOT_DIR environment variable to find
this dependency, for example:

export OPENSSL_ROOT_DIR=/usr/local/opt/openssl

You have not set this environment variable. Most likely, this will result in a
broken build with fairly obscure error messages. If your environment does not
require setting OPENSSL_ROOT_DIR, you can disable this check using:

cmake -DGOOGLE_CLOUD_CPP_ENABLE_MACOS_OPENSSL_CHECK=OFF ...

]===])
    endif ()
endif (APPLE)

# If ccache is installed use it for the build. This makes the Travis
# configuration agnostic as to wether ccache is installed or not.
option(GOOGLE_CLOUD_CPP_ENABLE_CCACHE "Automatically use ccache if available"
       ON)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_CCACHE)

if ("${GOOGLE_CLOUD_CPP_ENABLE_CCACHE}")
    find_program(GOOGLE_CLOUD_CPP_CCACHE_PROGRAM ccache NAMES /usr/bin/ccache)
    mark_as_advanced(GOOGLE_CLOUD_CPP_CCACHE_PROGRAM)
    if (GOOGLE_CLOUD_CPP_CCACHE_PROGRAM)
        message(STATUS "ccache found: ${GOOGLE_CLOUD_CPP_CCACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${GOOGLE_CLOUD_CPP_CCACHE_PROGRAM}")
        set(CMAKE_CC_COMPILER_LAUNCHER "${GOOGLE_CLOUD_CPP_CCACHE_PROGRAM}")
    endif ()
endif ()

# Search for Doxygen and create the targets for it if found.
option(GOOGLE_CLOUD_CPP_GENERATE_DOXYGEN "Generate Doxygen documentation" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_GENERATE_DOXYGEN)

# The default source for dependencies.
set(GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER
    "unused"
    CACHE STRING "This option is no longer used.")
set_property(CACHE GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER
             PROPERTY STRINGS "external" "package" "unused")
mark_as_advanced(GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER)

# Generate docs with relative URLs matching with the directory structure on
# googleapis.dev hosting.
option(GOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV
       "Use relative URLs in docs for googleapis.dev" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV)

# Use main as the version part of the relative URLs.
option(GOOGLE_CLOUD_CPP_USE_MAIN_FOR_REFDOC_LINKS
       "Use main as the version part for refdoc relative links" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_USE_MAIN_FOR_REFDOC_LINKS)

set(PROJECT_THIRD_PARTY_DIR "${PROJECT_SOURCE_DIR}/third_party")

include(CMakeDependentOption)

option(GOOGLE_CLOUD_CPP_ENABLE_BIGTABLE "Enable building the Bigtable library."
       ON)
option(GOOGLE_CLOUD_CPP_ENABLE_BIGQUERY "Enable building the Bigquery library."
       ON)
option(GOOGLE_CLOUD_CPP_ENABLE_SPANNER "Enable building the Spanner library."
       ON)
option(GOOGLE_CLOUD_CPP_ENABLE_STORAGE "Enable building the Storage library."
       ON)
option(GOOGLE_CLOUD_CPP_ENABLE_FIRESTORE
       "Enable building the Firestore library." ON)
option(GOOGLE_CLOUD_CPP_ENABLE_PUBSUB "Enable building the Pub/Sub library." ON)
option(GOOGLE_CLOUD_CPP_ENABLE_IAM "Enable building the IAM library." ON)
option(GOOGLE_CLOUD_CPP_ENABLE_LOGGING "Enable building the Logging library."
       ON)
option(GOOGLE_CLOUD_CPP_ENABLE_GENERATOR "Enable building the generator." OFF)

# The default list of libraries to build. These can be overridden by the user by
# passing a comma-separated list, i.e
# `-DGOOGLE_CLOUD_CPP_ENABLE=spanner,storage`.
set(GOOGLE_CLOUD_CPP_ENABLE
    "bigtable;bigquery;iam;firestore;logging;pubsub;spanner;storage"
    CACHE STRING "The list of libraries to build.")
string(REPLACE "," ";" GOOGLE_CLOUD_CPP_ENABLE "${GOOGLE_CLOUD_CPP_ENABLE}")

# Since these are not the recommended practice, mark them as advanced.
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_BIGTABLE)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_BIGQUERY)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_SPANNER)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_STORAGE)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_FIRESTORE)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_PUBSUB)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_IAM)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_LOGGING)
mark_as_advanced(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GENERATOR)

set(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC_DEFAULT OFF)
if (experimental-storage-grpc IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
    set(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC_DEFAULT ON)
endif ()
option(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC
       "Enable compilation for the GCS gRPC plugin (EXPERIMENTAL)"
       "${GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC_DEFAULT}")
mark_as_advanced(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC)

# We no longer build the generator by default, but if it was explicitly
# requested, we add it to the list of enabled libraries.
if (GOOGLE_CLOUD_CPP_ENABLE_GENERATOR)
    list(APPEND GOOGLE_CLOUD_CPP_ENABLE "generator")
endif ()

function (google_cloud_cpp_enable_cleanup)
    # Remove any library that's been disabled from the list.
    foreach (library ${GOOGLE_CLOUD_CPP_ENABLE})
        if ("${library}" STREQUAL "experimental-storage-grpc")
            set(feature_flag "GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC")
        else ()
            string(TOUPPER "GOOGLE_CLOUD_CPP_ENABLE_${library}" feature_flag)
        endif ()
        if (NOT ${feature_flag})
            message(
                WARNING "Using ${feature_flag} is discouraged. Please use the"
                        " unified GOOGLE_CLOUD_CPP_ENABLE list instead.")
            list(REMOVE_ITEM GOOGLE_CLOUD_CPP_ENABLE ${library})
        endif ()
    endforeach ()
    set(GOOGLE_CLOUD_CPP_ENABLE
        "${GOOGLE_CLOUD_CPP_ENABLE}"
        PARENT_SCOPE)
endfunction ()
google_cloud_cpp_enable_cleanup()

if (GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC AND NOT
                                             GOOGLE_CLOUD_CPP_ENABLE_STORAGE)
    message(
        FATAL_ERROR
            "The experimental GCS+gRPC plugin is enabled, but the base storage"
            " library is disabled. This is not a valid configuration, please"
            " review the options provided to CMake. Pay particular attention to"
            " GOOGLE_CLOUD_CPP_ENABLE, GOOGLE_CLOUD_CPP_ENABLE_STORAGE, and"
            " GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC.")
endif ()

# The only case where gRPC support is not needed is when compiling *only* the
# storage library.
set(GOOGLE_CLOUD_CPP_ENABLE_GRPC_EXPRESSION ON)
if ("${GOOGLE_CLOUD_CPP_ENABLE}" STREQUAL "storage")
    set(GOOGLE_CLOUD_CPP_ENABLE_GRPC_EXPRESSION OFF)
endif ()

cmake_dependent_option(
    GOOGLE_CLOUD_CPP_ENABLE_GRPC "Enable building the gRPC utilities library."
    ON "GOOGLE_CLOUD_CPP_ENABLE_GRPC_EXPRESSION" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_GRPC)

# Building this target results in all protobufs being compiled.
add_custom_target(google-cloud-cpp-protos)

# Each subproject adds dependencies to this target to have their docs generated.
add_custom_target(doxygen-docs)

if (${GOOGLE_CLOUD_CPP_ENABLE_GRPC})
    add_subdirectory(external/googleapis)
endif ()

# Enable testing in this directory so we can do a top-level `make test`. This
# also includes the BUILD_TESTING option, which is on by default.
include(CTest)

# Ensure that GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS is initialized since it's
# used in the depends condition of the next option.
include(EnableCxxExceptions)

# The examples use exception handling to simplify the code. Therefore they
# cannot be compiled when exceptions are disabled, and applications cannot force
# the flag.
cmake_dependent_option(
    GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES "Compile the google-cloud-cpp examples."
    ON "GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS;BUILD_TESTING" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES)

add_subdirectory(google/cloud)
function (google_cloud_cpp_enable_features)
    foreach (feature ${GOOGLE_CLOUD_CPP_ENABLE})
        if ("${feature}" STREQUAL "generator")
            add_subdirectory(generator)
        elseif ("${feature}" STREQUAL "experimental-storage-grpc")
            if (NOT ("storage" IN_LIST GOOGLE_CLOUD_CPP_ENABLE))
                add_subdirectory(google/cloud/storage)
            endif ()
        else ()
            add_subdirectory(google/cloud/${feature})
        endif ()
    endforeach ()
endfunction ()
google_cloud_cpp_enable_features()

# The examples are more readable if we use exceptions for error handling. We had
# to tradeoff readability vs. "making them compile everywhere".
if (GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES
    AND bigtable IN_LIST GOOGLE_CLOUD_CPP_ENABLE
    AND storage IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
    add_subdirectory(google/cloud/examples)
endif ()

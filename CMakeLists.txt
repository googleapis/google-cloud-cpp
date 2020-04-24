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
project(google-cloud-cpp CXX C)

set(GOOGLE_CLOUD_CPP_VERSION_MAJOR 1)
set(GOOGLE_CLOUD_CPP_VERSION_MINOR 13)
set(GOOGLE_CLOUD_CPP_VERSION_PATCH 0)

# Configure the Compiler options, we use C++11 features by default.
set(GOOGLE_CLOUD_CPP_CXX_STANDARD
    11
    CACHE STRING "Configure CMAKE_CXX_STANDARD")
mark_as_advanced(GOOGLE_CLOUD_CPP_CXX_STANDARD)

set(CMAKE_CXX_STANDARD "${GOOGLE_CLOUD_CPP_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

    # gcc-4.8 is the first release that claims C++11 support.
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.8)
        message(
            FATAL_ERROR
                "GCC version must be at least 4.8. Older versions"
                " either lack C++11 support or have bugs that prevent us from"
                " using them.")
    endif ()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

    # Before Clang-3.8 we ran into bugs with std::async() and exceptions.
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.8)
        message(
            FATAL_ERROR
                "Clang version must be at least 3.8. Older versions"
                " either lack C++11 support or have bugs that prevent us from"
                " using them.")
    endif ()
endif ()

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

export OPENSSL_ROOT_DIR=/usr/local/opt/libressl

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
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE
                                     "${GOOGLE_CLOUD_CPP_CCACHE_PROGRAM}")
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK
                                     "${GOOGLE_CLOUD_CPP_CCACHE_PROGRAM}")
    endif ()
endif ()

# The default source for dependencies.
set(GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER
    "unused"
    CACHE STRING "This option is no longer used.")

set_property(CACHE GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER
             PROPERTY STRINGS "external" "package" "unused")

# Generate docs with relative URLs matching with the directory structure on
# googleapis.dev hositng.
option(GOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV
       "Use relative URLs in docs for googleapis.dev" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV)

# Use master as the version part of the relative URLs.
option(GOOGLE_CLOUD_CPP_USE_MASTER_FOR_REFDOC_LINKS
       "Use master as the version part for refdoc relative links" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_USE_MASTER_FOR_REFDOC_LINKS)

set(PROJECT_THIRD_PARTY_DIR "${PROJECT_SOURCE_DIR}/third_party")
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

option(GOOGLE_CLOUD_CPP_TESTING_UTIL_ENABLE_INSTALL
       "Install the testing utilities." OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_TESTING_UTIL_ENABLE_INSTALL)

include(CMakeDependentOption)

option(GOOGLE_CLOUD_CPP_ENABLE_BIGQUERY "Enable building the BigQuery library."
       ON)
option(GOOGLE_CLOUD_CPP_ENABLE_BIGTABLE "Enable building the Bigtable library."
       ON)
option(GOOGLE_CLOUD_CPP_ENABLE_STORAGE "Enable building the Storage library."
       ON)
option(GOOGLE_CLOUD_CPP_ENABLE_FIRESTORE
       "Enable building the Firestore library." ON)
option(GOOGLE_CLOUD_CPP_ENABLE_PUBSUB "Enable building the Pub/Sub library." ON)

cmake_dependent_option(
    GOOGLE_CLOUD_CPP_ENABLE_GRPC_UTILS
    "Enable building the gRPC utilities library." ON
    "GOOGLE_CLOUD_CPP_ENABLE_BIGTABLE" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_GRPC_UTILS)

# Enable testing in this directory so we can do a top-level `make test`. This
# also includes the BUILD_TESTING option, which is on by default.
include(CTest)

# Each subproject adds dependencies to this target to have their docs generated.
add_custom_target(doxygen-docs)

# TODO(#3926) - minimize the components built in the common library. For
# example, if only google/cloud/storage is enabled, we can disable the gRPC
# components.
add_subdirectory(google/cloud)
if (GOOGLE_CLOUD_CPP_ENABLE_STORAGE)
    add_subdirectory(google/cloud/storage)
endif ()
if (GOOGLE_CLOUD_CPP_ENABLE_BIGQUERY)
    add_subdirectory(google/cloud/bigquery)
endif ()
if (GOOGLE_CLOUD_CPP_ENABLE_BIGTABLE)
    add_subdirectory(google/cloud/bigtable)
endif ()
if (GOOGLE_CLOUD_CPP_ENABLE_FIRESTORE)
    add_subdirectory(google/cloud/firestore)
endif ()
if (GOOGLE_CLOUD_CPP_ENABLE_PUBSUB)
    add_subdirectory(google/cloud/pubsub)
endif ()

# The examples are more readable if we use exceptions for error handling. We had
# to tradeoff readability vs. "making them compile everywhere".
if (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS
    AND GOOGLE_CLOUD_CPP_ENABLE_BIGTABLE
    AND GOOGLE_CLOUD_CPP_ENABLE_STORAGE)
    add_subdirectory(google/cloud/examples)
endif ()

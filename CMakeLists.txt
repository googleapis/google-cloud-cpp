# ~~~
# Copyright 2017 Google Inc.
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

cmake_minimum_required(VERSION 3.10...3.24)

# Define the project name and where to report bugs.
set(PACKAGE_BUGREPORT "https://github.com/googleapis/google-cloud-cpp/issues")

project(
    google-cloud-cpp
    VERSION 2.10.0
    LANGUAGES CXX)
set(PROJECT_VERSION_PRE_RELEASE "rc")

if (NOT "${PROJECT_VERSION_PRE_RELEASE}" STREQUAL "")
    set(PROJECT_VERSION "${PROJECT_VERSION}-${PROJECT_VERSION_PRE_RELEASE}")
endif ()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.3)
        message(
            WARNING "The `google-cloud-cpp` library is tested with GCC >= 7.3."
                    " We will consider patches for older versions.")
    endif ()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.0)
        message(
            WARNING
                "The `google-cloud-cpp` library is tested with Clang >= 6.0."
                " We will consider patches for older versions.")
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
# This is an easy mistake to make, and the error messages are very confusing.
# Help our users by giving them some guidance.
if (APPLE
    AND GOOGLE_CLOUD_CPP_ENABLE_MACOS_OPENSSL_CHECK
    AND NOT DEFINED OPENSSL_ROOT_DIR)
    message(
        WARNING
            "The Google Cloud C++ client libraries use the native OpenSSL "
            "library. In most macOS systems, you need to set the "
            "OPENSSL_ROOT_DIR environment variable to find this dependency, "
            "for example:"
            "\n"
            "    cmake -DOPENSSL_ROOT_DIR=/usr/local/opt/openssl ..."
            "\n"
            "You have not set this variable. Most likely, this will result in "
            "a broken build with fairly obscure error messages. If your "
            "environment does not require setting OPENSSL_ROOT_DIR, you can "
            "disable this check using:"
            "\n"
            "    cmake -DGOOGLE_CLOUD_CPP_ENABLE_MACOS_OPENSSL_CHECK=OFF ...")
endif ()

# If ccache is installed use it for the build.
option(GOOGLE_CLOUD_CPP_ENABLE_CCACHE "Automatically use ccache if available"
       OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_CCACHE)

if (GOOGLE_CLOUD_CPP_ENABLE_CCACHE)
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

# Compile the DocFX tool.
option(
    GOOGLE_CLOUD_CPP_INTERNAL_DOCFX
    "Compile the docfx tool. Only google-cloud-cpp developers are expected to use this flag."
    OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_INTERNAL_DOCFX)

# Generate docs with relative URLs matching with the directory structure on
# googleapis.dev hosting.
option(GOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV
       "Use relative URLs in docs for googleapis.dev" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_GEN_DOCS_FOR_GOOGLEAPIS_DEV)

# Use main as the version part of the relative URLs.
option(GOOGLE_CLOUD_CPP_USE_MAIN_FOR_REFDOC_LINKS
       "Use main as the version part for refdoc relative links" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_USE_MAIN_FOR_REFDOC_LINKS)

option(
    GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND
    [==[Enable the workarounds for [ctype = CORD] when using Protobuf < v23.
More details at

https://github.com/googleapis/google-cloud-cpp/blob/main/doc/ctype-cord-workaround.md
]==]
    OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND)

set(GOOGLE_CLOUD_CPP_OVERRIDE_GOOGLEAPIS_URL
    ""
    CACHE
        STRING
        [==[
Override the default URL for the googleapis protos. google-cloud-cpp needs the
proto and gRPC definitions for most Google Cloud services. By default these
definitions are downloaded from GitHub. In some environments, this may not be
possible. This variable can be used to use a local directory, or a local
tarball, with the proto definitions.

If you override this variable, you must ensure the proto definitions match the
expectations in google-cloud-cpp. Using the same SHA is the recommended
practice. Using older SHAs is **NOT** supported, and will almost certainly
result in build problems. You may be able to use a more recent SHA of the
protos, but do not expect that new features will become exposed via the C++
client libraries.
]==])
mark_as_advanced(GOOGLE_CLOUD_CPP_OVERRIDE_GOOGLEAPIS_URL)
set(GOOGLE_CLOUD_CPP_OVERRIDE_GOOGLEAPIS_URL_HASH
    ""
    CACHE
        STRING
        [==[
Override the hash for the googleapis protos tarball. See
GOOGLE_CLOUD_CPP_OVERRIDE_GOOGLEAPIS_URL for details.
]==])
mark_as_advanced(GOOGLE_CLOUD_CPP_OVERRIDE_GOOGLEAPIS_URL_HASH)

include(GoogleCloudCppFeatures)

# Controls the list of features compiled by `google-cloud-cpp`.
set(GOOGLE_CLOUD_CPP_ENABLE
    ${GOOGLE_CLOUD_CPP_LEGACY_FEATURES}
    CACHE STRING "The list of libraries to build.")
string(REPLACE "," ";" GOOGLE_CLOUD_CPP_ENABLE "${GOOGLE_CLOUD_CPP_ENABLE}")

# We use some functions to enable the features configured in
# `GOOGLE_CLOUD_CPP_ENABLE`. Using functions makes it easier to keep the code
# structured, and documents what some of these things are doing.
google_cloud_cpp_define_legacy_feature_options()
google_cloud_cpp_apply_legacy_feature_options()
google_cloud_cpp_enable_deps()
google_cloud_cpp_enable_cleanup()

# Building this target results in all protobufs being compiled.
add_custom_target(google-cloud-cpp-protos)

# Each subproject adds dependencies to this target to have their docs generated.
add_custom_target(doxygen-docs)
add_custom_target(all-docfx)

find_package(absl CONFIG REQUIRED)
if (${GOOGLE_CLOUD_CPP_ENABLE_GRPC})
    find_package(gRPC REQUIRED QUIET)
    find_package(Protobuf REQUIRED QUIET)
    add_subdirectory(external/googleapis)
endif ()

# Enable testing in this directory so we can do a top-level `make test`. This
# also includes the BUILD_TESTING option, which is on by default.
include(CTest)

# Ensure that GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS is initialized since it's
# used in the depends condition of the next option.
include(EnableCxxExceptions)

if (BUILD_TESTING)
    # Discover and add targets for the GTest::gtest and GTest::gmock libraries.
    include(FindGMockWithTargets)
endif ()

# The examples use exception handling to simplify the code. Therefore they
# cannot be compiled when exceptions are disabled, and applications cannot force
# the flag.
cmake_dependent_option(
    GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES "Compile the google-cloud-cpp examples."
    ON "GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS;BUILD_TESTING" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES)

# Add any subdirectories configured in the application.
if (GOOGLE_CLOUD_CPP_INTERNAL_DOCFX)
    # `docfx/` must be included before any library directories. The tool is used
    # to generate the library documentation.
    add_subdirectory(docfx)
endif ()
add_subdirectory(google/cloud)
google_cloud_cpp_enable_features()

# The examples are more readable if we use exceptions for error handling. We had
# to tradeoff readability vs. "making them compile everywhere".
if (GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES
    AND bigtable IN_LIST GOOGLE_CLOUD_CPP_ENABLE
    AND iam IN_LIST GOOGLE_CLOUD_CPP_ENABLE
    AND spanner IN_LIST GOOGLE_CLOUD_CPP_ENABLE
    AND storage IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
    add_subdirectory(examples)
endif ()

# Obsolete / retired flags and options. Removing them just cleans up a bit of
# our code at the cost of breaking customers, while putting them here makes them
# basically invisible to us.
set(GOOGLE_CLOUD_CPP_CXX_STANDARD
    ""
    CACHE STRING "Unused. Prefer CMAKE_CXX_STANDARD")
mark_as_advanced(GOOGLE_CLOUD_CPP_CXX_STANDARD)

# The default source for dependencies.
set(GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER
    "unused"
    CACHE STRING "This option is no longer used.")
set_property(CACHE GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER
             PROPERTY STRINGS "external" "package" "unused")
mark_as_advanced(GOOGLE_CLOUD_CPP_DEPENDENCY_PROVIDER)

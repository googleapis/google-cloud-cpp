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

cmake_minimum_required(VERSION 3.13...3.24)

# Define the project name and where to report bugs.
set(PACKAGE_BUGREPORT "https://github.com/googleapis/google-cloud-cpp/issues")

project(
    google-cloud-cpp
    VERSION 2.39.0
    LANGUAGES CXX)
set(PROJECT_VERSION_PRE_RELEASE "rc")

if (NOT "${PROJECT_VERSION_PRE_RELEASE}" STREQUAL "")
    set(PROJECT_VERSION "${PROJECT_VERSION}-${PROJECT_VERSION_PRE_RELEASE}")
endif ()

# Automatically update the version(s) in the `MODULE.bazel` file.
file(READ "MODULE.bazel" module_contents)
string(
    REGEX
    REPLACE "version = \".*\",  # Updated by CMake"
            "version = \"${PROJECT_VERSION}\",  # Updated by CMake"
            updated_contents "${module_contents}")
string(
    REGEX
    REPLACE
        "compatibility_level = .*,  # Updated by CMake"
        "compatibility_level = ${PROJECT_VERSION_MAJOR},  # Updated by CMake"
        updated_contents "${updated_contents}")
if (NOT ("${updated_contents}" STREQUAL "${module_contents}"))
    # Rewrite the file *only* if it was updated.
    file(WRITE "MODULE.bazel" "${updated_contents}")
endif ()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.5)
        message(
            WARNING "The `google-cloud-cpp` library is tested with GCC >= 7.5."
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
    endif ()
endif ()

# Search for Doxygen and create the targets for it if found.
option(GOOGLE_CLOUD_CPP_GENERATE_DOXYGEN [==[
Generate Doxygen documentation. Only google-cloud-cpp developers are expected to
use this flag.]==] OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_GENERATE_DOXYGEN)

option(
    GOOGLE_CLOUD_CPP_USE_LATEST_FOR_REFDOC_LINKS
    [==[
Link to "latest" in our reference documentation. Otherwise, link to the project
version. Only google-cloud-cpp developers are expected to use this flag.]==]
    OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_USE_LATEST_FOR_REFDOC_LINKS)

# Compile the DocFX tool.
option(
    GOOGLE_CLOUD_CPP_INTERNAL_DOCFX
    "Compile the docfx tool. Only google-cloud-cpp developers are expected to use this flag."
    OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_INTERNAL_DOCFX)

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

option(
    GOOGLE_CLOUD_CPP_USE_INSTALLED_COMMON
    [==[
Use a pre-installed version of the common libraries and proto files.

This option is required to support package maintainers that split
`google-cloud-cpp` into multiple packages.
]==]
    OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_USE_INSTALLED_COMMON)

include(CMakeDependentOption)
cmake_dependent_option(
    GOOGLE_CLOUD_CPP_WITH_MOCKS
    [==[Build the google-cloud-cpp mocking libraries.

google-cloud-cpp offers mocking libraries with mock classes, to facilitate unit
testing of Cloud C++ clients. Consumers of this library that do not use the
provided mocks to test code involving the Cloud C++ clients may wish to turn
this flag off.]==]
    ON
    "NOT BUILD_TESTING"
    ON)
mark_as_advanced(GOOGLE_CLOUD_CPP_WITH_MOCKS)

# The examples use exception handling to simplify the code. Therefore they
# cannot be compiled when exceptions are disabled, and applications cannot force
# the flag.
cmake_dependent_option(
    GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES "Compile the google-cloud-cpp examples."
    ON "GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS;BUILD_TESTING" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES)

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
google_cloud_cpp_define_dependent_legacy_feature_options()

# Building this target results in all protobufs being compiled.
add_custom_target(google-cloud-cpp-protos)

# Each subproject adds dependencies to this target to have their docs generated.
add_custom_target(doxygen-docs)
add_custom_target(all-docfx)

find_package(absl CONFIG REQUIRED)
if (GOOGLE_CLOUD_CPP_ENABLE_GRPC)
    find_package(gRPC REQUIRED QUIET)
    # `Protobuf_PROTOC_EXECUTABLE` is pretty standard. It has been in use by
    # CMake's FindProtobuf module for years, and it is defined by Protobuf's
    # CMake configuration files. There is no obvious analog for the gRPC plugin.
    # gRPC only exports the targets (e.g. `gRPC::grpc_cpp_plugin`) when
    # applicable. We define our own variable.
    set(GOOGLE_CLOUD_CPP_GRPC_PLUGIN_EXECUTABLE
        $<TARGET_FILE:gRPC::grpc_cpp_plugin>
        CACHE
            STRING
            [==[Override gRPC's protoc plugin location.
`google-cloud-cpp` uses the gRPC's protoc plugin to generate gRPC stub code.
One may need to override the default location when cross-compiling, as the gRPC
package will point to the target environment and the plugin won't run on the
host environment.
]==])
    mark_as_advanced(GOOGLE_CLOUD_CPP_GRPC_PLUGIN_EXECUTABLE)

    set(GOOGLE_CLOUD_CPP_FIND_DEPENDENCY_PROTOBUF
        "find_dependency(Protobuf CONFIG)")
    find_package(Protobuf CONFIG QUIET)
    if (NOT Protobuf_FOUND)
        set(GOOGLE_CLOUD_CPP_FIND_DEPENDENCY_PROTOBUF
            "find_dependency(Protobuf)")
        find_package(Protobuf REQUIRED)
    endif ()
endif ()

# Enable testing in this directory so we can do a top-level `make test`. This
# also includes the BUILD_TESTING option, which is on by default.
include(CTest)

# Ensure that GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS is initialized since it's
# used in the depends condition of the next option.
include(EnableCxxExceptions)

if (BUILD_TESTING OR GOOGLE_CLOUD_CPP_WITH_MOCKS)
    # Discover and add targets for the GTest::gtest and GTest::gmock libraries.
    include(FindGMockWithTargets)
endif ()

# Add any subdirectories configured in the application.

# `docfx/` must be included before any library directories. The tool is used to
# generate the library documentation.
add_subdirectory(docfx)

# We always include the external protos. If this is not needed it immediately
# returns.
add_subdirectory(external/googleapis)
add_subdirectory(google/cloud)

if (GOOGLE_CLOUD_CPP_USE_INSTALLED_COMMON)
    find_package(google_cloud_cpp_common CONFIG REQUIRED)
    if (GOOGLE_CLOUD_CPP_ENABLE_GRPC)
        find_package(google_cloud_cpp_grpc_utils CONFIG REQUIRED)
        find_package(google_cloud_cpp_googleapis CONFIG REQUIRED)
    endif ()
    if (GOOGLE_CLOUD_CPP_ENABLE_REST)
        find_package(google_cloud_cpp_rest_internal CONFIG REQUIRED)
    endif ()
    if (GOOGLE_CLOUD_CPP_ENABLE_REST AND GOOGLE_CLOUD_CPP_ENABLE_GRPC)
        find_package(google_cloud_cpp_rest_protobuf_internal CONFIG REQUIRED)
    endif ()
endif ()

google_cloud_cpp_enable_features()

add_subdirectory(examples)

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

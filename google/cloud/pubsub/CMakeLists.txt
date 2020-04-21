# ~~~
# Copyright 2020 Google LLC
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

find_package(google_cloud_cpp_common CONFIG REQUIRED)
find_package(google_cloud_cpp_grpc_utils CONFIG REQUIRED)
find_package(googleapis CONFIG REQUIRED)

set(DOXYGEN_PROJECT_NAME "Google Cloud Pub/Sub C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Pub/Sub")
set(DOXYGEN_PROJECT_NUMBER "${GOOGLE_CLOUD_CPP_CLIENT_VERSION}")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/samples)

include(GoogleCloudCppCommon)

# Define a function to fetch the current git revision. Using a function creates
# a new scope, so the CMake variables do not leak to the global namespace.
function (google_cloud_cpp_pubsub_initialize_git_head var)
    set(result "unknown")
    # If we cannot find a `.git` directory do not even try to guess the git
    # revision.
    if (IS_DIRECTORY ${PROJECT_SOURCE_DIR}/.git)
        # We need `git` to find the revision.
        find_program(GOOGLE_CLOUD_CPP_PUBSUB_GIT_PROGRAM NAMES git)
        mark_as_advanced(GOOGLE_CLOUD_CPP_PUBSUB_GIT_PROGRAM)
        if (GOOGLE_CLOUD_CPP_PUBSUB_GIT_PROGRAM)
            # Run `git rev-parse --short HEAD` and capture the output in a
            # variable.
            execute_process(
                COMMAND "${GOOGLE_CLOUD_CPP_PUBSUB_GIT_PROGRAM}" rev-parse
                        --short HEAD
                OUTPUT_VARIABLE GIT_HEAD_LOG
                ERROR_VARIABLE GIT_HEAD_LOG)
            string(REPLACE "\n" "" result "${GIT_HEAD_LOG}")
        endif ()
    endif ()
    set(${var}
        "${result}"
        PARENT_SCOPE)
endfunction ()

# Capture the compiler version and the git revision into variables, then
# generate a config file with the values.
if (NOT "${GOOGLE_CLOUD_CPP_PUBSUB_BUILD_METADATA}" STREQUAL "")
    # The build metadata flag is already defined, do not re-compute the
    # initialization value. This works both when the user supplies
    # -DGOOGLE_CLOUD_CPP_PUBSUB_METADATA=value in the command line, and when
    # GOOGLE_CLOUD_CPP_PUBSUB_METADATA has a cached value
    set(GOOGLE_CLOUD_CPP_PUBSUB_GIT_HEAD "unused")
else ()
    google_cloud_cpp_pubsub_initialize_git_head(
        GOOGLE_CLOUD_CPP_PUBSUB_GIT_HEAD)
endif ()

# Define a CMake configuration option to set the build metadata. By default this
# is initialized from `git rev-parse --short HEAD`, but the developer (or the
# script building via CMake) can override the value.
set(GOOGLE_CLOUD_CPP_PUBSUB_BUILD_METADATA
    "${GOOGLE_CLOUD_CPP_PUBSUB_GIT_HEAD}"
    CACHE STRING "Append build metadata to the library version number")
# This option is rarely needed. Mark it as "advanced" to remove it from the
# default CMake UIs.
mark_as_advanced(GOOGLE_CLOUD_CPP_PUBSUB_BUILD_METADATA)

message(STATUS "google-cloud-cpp-pubsub build metadata set to"
               " ${GOOGLE_CLOUD_CPP_PUBSUB_BUILD_METADATA}")

# Create the file that captures build information. Having access to the compiler
# and build flags at runtime allows us to print better benchmark results.
string(TOUPPER "${CMAKE_BUILD_TYPE}" GOOGLE_CLOUD_CPP_PUBSUB_BUILD_TYPE_UPPER)
configure_file(internal/build_info.cc.in internal/build_info.cc)

configure_file(version_info.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version_info.h)
add_library(
    googleapis_cpp_pubsub_client # cmake-format: sort
    ${CMAKE_CURRENT_BINARY_DIR}/internal/build_info.cc
    connection_options.cc
    connection_options.h
    create_subscription_builder.h
    create_topic_builder.h
    internal/build_info.h
    internal/compiler_info.cc
    internal/compiler_info.h
    internal/publisher_stub.cc
    internal/publisher_stub.h
    internal/subscriber_stub.cc
    internal/subscriber_stub.h
    internal/user_agent_prefix.cc
    internal/user_agent_prefix.h
    publisher_client.cc
    publisher_client.h
    publisher_connection.cc
    publisher_connection.h
    subscriber_client.cc
    subscriber_client.h
    subscriber_connection.cc
    subscriber_connection.h
    subscription.cc
    subscription.h
    topic.cc
    topic.h
    version.cc
    version.h
    version_info.h)
target_include_directories(
    googleapis_cpp_pubsub_client
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    googleapis_cpp_pubsub_client
    PUBLIC google_cloud_cpp_grpc_utils google_cloud_cpp_common
           googleapis-c++::pubsub_protos)
set_target_properties(
    googleapis_cpp_pubsub_client
    PROPERTIES VERSION "${GOOGLE_CLOUD_CPP_CLIENT_VERSION}"
               SOVERSION "${GOOGLE_CLOUD_CPP_CLIENT_VERSION_MAJOR}")
target_compile_options(googleapis_cpp_pubsub_client
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

google_cloud_cpp_add_common_options(googleapis_cpp_pubsub_client)

add_library(googleapis-c++::pubsub_client ALIAS googleapis_cpp_pubsub_client)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(googleapis_cpp_pubsub_client YEAR "2020")

# Define the tests in a function so we have a new scope for variable names.
function (google_cloud_cpp_pubsub_client_define_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    find_package(google_cloud_cpp_testing CONFIG REQUIRED)

    set(googleapis_cpp_pubsub_client_unit_tests
        # cmake-format: sort
        create_subscription_builder_test.cc
        create_topic_builder_test.cc
        internal/build_info_test.cc
        internal/compiler_info_test.cc
        internal/user_agent_prefix_test.cc
        subscription_test.cc
        topic_test.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("googleapis_cpp_pubsub_client_unit_tests.bzl"
                         "googleapis_cpp_pubsub_client_unit_tests" YEAR "2020")

    # Generate a target for each unit test.
    foreach (fname ${googleapis_cpp_pubsub_client_unit_tests})
        string(REPLACE "/" "_" basename ${fname})
        string(REPLACE ".cc" "" basename ${basename})
        set(target "pubsub_${basename}")
        add_executable(${target} ${fname})
        set_target_properties(${target} PROPERTIES OUTPUT_NAME ${basename})
        target_link_libraries(
            ${target}
            PRIVATE googleapis-c++::pubsub_client google_cloud_cpp_testing
                    google_cloud_cpp_testing_grpc GTest::gmock_main
                    GTest::gmock GTest::gtest)
        google_cloud_cpp_add_common_options(${target})

        # With googletest it is relatively easy to exceed the default number of
        # sections (~65,000) in a single .obj file. Add the /bigobj option to
        # all the tests, even if it is not needed.
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()
endfunction ()

# Only define the tests if testing is enabled. Package maintainers may not want
# to build all the tests everytime they create a new package or when the package
# is installed from source.
if (BUILD_TESTING)
    google_cloud_cpp_pubsub_client_define_tests()
endif (BUILD_TESTING)

add_subdirectory(integration_tests)

# Only compile the samples if we're building with exceptions enabled. They
# require exceptions to keep them simple and idiomatic.
if (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_subdirectory(samples)
endif (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT pubsub-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/googleapis_cpp_pubsub_client")

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS googleapis_cpp_pubsub_client
    EXPORT pubsub-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_pubsub_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_pubsub_runtime
            NAMELINK_SKIP
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_pubsub_development)
# With CMake-3.12 and higher we could avoid this separate command (and the
# duplication).
install(
    TARGETS googleapis_cpp_pubsub_client
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_pubsub_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_pubsub_development)

google_cloud_cpp_install_headers("googleapis_cpp_pubsub_client"
                                 "include/google/cloud/pubsub")

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CONFIG_VERSION_MAJOR ${GOOGLE_CLOUD_CPP_CLIENT_VERSION_MAJOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_MINOR ${GOOGLE_CLOUD_CPP_CLIENT_VERSION_MINOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_PATCH ${GOOGLE_CLOUD_CPP_CLIENT_VERSION_PATCH})
set(GOOGLE_CLOUD_PC_NAME "The Google Cloud Pubsub C++ Client Library")
set(GOOGLE_CLOUD_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Pubsub.")
set(GOOGLE_CLOUD_PC_REQUIRES
    "google_cloud_cpp_grpc_utils google_cloud_cpp_common")
set(GOOGLE_CLOUD_PC_LIBS "-lgoogleapis_cpp_pubsub_client")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/pubsub/config.pc.in"
               "pubsub_client.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/pubsub_client.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_pubsub_development)

# Create and install the CMake configuration files.
configure_file("config.cmake.in" "googleapis_cpp_pubsub_client-config.cmake"
               @ONLY)
configure_file(
    "${PROJECT_SOURCE_DIR}/google/cloud/pubsub/config-version.cmake.in"
    "googleapis_cpp_pubsub_client-config-version.cmake" @ONLY)
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/googleapis_cpp_pubsub_client-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/googleapis_cpp_pubsub_client-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/googleapis_cpp_pubsub_client"
    COMPONENT google_cloud_cpp_pubsub_development)

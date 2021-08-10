# ~~~
# Copyright 2021 Google LLC
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

set(DOXYGEN_PROJECT_NAME "Google Cloud BigQuery C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud BigQuery")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/samples
                         ${CMAKE_CURRENT_SOURCE_DIR}/quickstart)
set(DOXYGEN_EXCLUDE_PATTERNS "*/*_test.cc")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "bigquery_internal" "bigquery_testing"
                            "examples")

# Creates the proto headers needed by doxygen.
set(GOOGLE_CLOUD_CPP_DOXYGEN_DEPS google-cloud-cpp::cloud_bigquery_protos)

include(GoogleCloudCppCommon)

find_package(absl CONFIG REQUIRED)

# configure_file(version_info.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version_info.h)
add_library(
    google_cloud_cpp_bigquery # cmake-format: sort
    bigquery_read_client.cc
    bigquery_read_client.h
    bigquery_read_connection.cc
    bigquery_read_connection.h
    bigquery_read_connection_idempotency_policy.cc
    bigquery_read_connection_idempotency_policy.h
    bigquery_read_options.h
    internal/bigquery_read_auth_decorator.cc
    internal/bigquery_read_auth_decorator.h
    internal/bigquery_read_logging_decorator.cc
    internal/bigquery_read_logging_decorator.h
    internal/bigquery_read_metadata_decorator.cc
    internal/bigquery_read_metadata_decorator.h
    internal/bigquery_read_option_defaults.cc
    internal/bigquery_read_option_defaults.h
    internal/bigquery_read_stub.cc
    internal/bigquery_read_stub.h
    internal/bigquery_read_stub_factory.cc
    internal/bigquery_read_stub_factory.h
    retry_traits.h
    streaming.cc)
target_include_directories(
    google_cloud_cpp_bigquery
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_bigquery
    PUBLIC google-cloud-cpp::grpc_utils google-cloud-cpp::common
           google-cloud-cpp::cloud_bigquery_protos)
google_cloud_cpp_add_common_options(google_cloud_cpp_bigquery)
set_target_properties(
    google_cloud_cpp_bigquery
    PROPERTIES EXPORT_NAME google-cloud-cpp::bigquery VERSION
                                                      "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_bigquery
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::bigquery ALIAS google_cloud_cpp_bigquery)
add_library(google-cloud-cpp::experimental-bigquery ALIAS
            google_cloud_cpp_bigquery)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_bigquery YEAR "2021")

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file). Unfortunately INTERFACE libraries are a bit weird in
# that they need absolute paths for their sources. target_sources example:
# ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_servicename_v2_connection.h
add_library(google_cloud_cpp_bigquery_mocks INTERFACE)
target_sources(
    google_cloud_cpp_bigquery_mocks
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_bigquery_read_connection.h)
target_link_libraries(
    google_cloud_cpp_bigquery_mocks
    INTERFACE google-cloud-cpp::bigquery GTest::gmock_main GTest::gmock
              GTest::gtest)
create_bazel_config(google_cloud_cpp_bigquery_mocks YEAR "2021")
target_include_directories(
    google_cloud_cpp_bigquery_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_bigquery_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_subdirectory(integration_tests)
# Examples are enabled if possible, but package maintainers may want to disable
# compilation to speed up their builds.
if (GOOGLE_CLOUD_CPP_ENABLE_EXAMPLES)
    add_subdirectory(samples)
endif ()

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_bigquery-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_bigquery"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_bigquery
    EXPORT google_cloud_cpp_bigquery-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_SKIP
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)
# With CMake-3.12 and higher we could avoid this separate command (and the
# duplication).
install(
    TARGETS google_cloud_cpp_bigquery
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers("google_cloud_cpp_bigquery"
                                 "include/google/cloud/bigquery")
google_cloud_cpp_install_headers("google_cloud_cpp_bigquery_mocks"
                                 "include/google/cloud/bigquery")

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CONFIG_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(GOOGLE_CLOUD_PC_NAME "The Google Cloud BigQuery C++ Client Library")
set(GOOGLE_CLOUD_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud BigQuery.")
set(GOOGLE_CLOUD_PC_LIBS "-lgoogle_cloud_cpp_bigquery")
string(CONCAT GOOGLE_CLOUD_PC_REQUIRES "google_cloud_cpp_grpc_utils"
              " google_cloud_cpp_common"
              " google_cloud_cpp_cloud_bigquery_protos")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/bigquery/config.pc.in"
               "google_cloud_cpp_bigquery.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_bigquery.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_development)

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_bigquery-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_bigquery-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_bigquery-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_bigquery-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_bigquery"
    COMPONENT google_cloud_cpp_development)

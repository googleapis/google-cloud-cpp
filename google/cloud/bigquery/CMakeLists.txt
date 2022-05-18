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

include(GoogleapisConfig)
set(DOXYGEN_PROJECT_NAME "Google Cloud BigQuery C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud BigQuery")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/samples
                         ${CMAKE_CURRENT_SOURCE_DIR}/quickstart)
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "bigquery_internal" "bigquery_testing"
                            "examples")

# Creates the proto headers needed by doxygen.
set(GOOGLE_CLOUD_CPP_DOXYGEN_DEPS google-cloud-cpp::cloud_bigquery_protos)

include(GoogleCloudCppCommon)

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
    bigquery_write_client.cc
    bigquery_write_client.h
    bigquery_write_connection.cc
    bigquery_write_connection.h
    bigquery_write_connection_idempotency_policy.cc
    bigquery_write_connection_idempotency_policy.h
    bigquery_write_options.h
    connection_client.cc
    connection_client.h
    connection_connection.cc
    connection_connection.h
    connection_connection_idempotency_policy.cc
    connection_connection_idempotency_policy.h
    connection_options.h
    data_transfer_client.cc
    data_transfer_client.h
    data_transfer_connection.cc
    data_transfer_connection.h
    data_transfer_connection_idempotency_policy.cc
    data_transfer_connection_idempotency_policy.h
    data_transfer_options.h
    internal/bigquery_read_auth_decorator.cc
    internal/bigquery_read_auth_decorator.h
    internal/bigquery_read_connection_impl.cc
    internal/bigquery_read_connection_impl.h
    internal/bigquery_read_logging_decorator.cc
    internal/bigquery_read_logging_decorator.h
    internal/bigquery_read_metadata_decorator.cc
    internal/bigquery_read_metadata_decorator.h
    internal/bigquery_read_option_defaults.cc
    internal/bigquery_read_option_defaults.h
    internal/bigquery_read_retry_traits.h
    internal/bigquery_read_stub.cc
    internal/bigquery_read_stub.h
    internal/bigquery_read_stub_factory.cc
    internal/bigquery_read_stub_factory.h
    internal/bigquery_write_auth_decorator.cc
    internal/bigquery_write_auth_decorator.h
    internal/bigquery_write_connection_impl.cc
    internal/bigquery_write_connection_impl.h
    internal/bigquery_write_logging_decorator.cc
    internal/bigquery_write_logging_decorator.h
    internal/bigquery_write_metadata_decorator.cc
    internal/bigquery_write_metadata_decorator.h
    internal/bigquery_write_option_defaults.cc
    internal/bigquery_write_option_defaults.h
    internal/bigquery_write_retry_traits.h
    internal/bigquery_write_stub.cc
    internal/bigquery_write_stub.h
    internal/bigquery_write_stub_factory.cc
    internal/bigquery_write_stub_factory.h
    internal/connection_auth_decorator.cc
    internal/connection_auth_decorator.h
    internal/connection_connection_impl.cc
    internal/connection_connection_impl.h
    internal/connection_logging_decorator.cc
    internal/connection_logging_decorator.h
    internal/connection_metadata_decorator.cc
    internal/connection_metadata_decorator.h
    internal/connection_option_defaults.cc
    internal/connection_option_defaults.h
    internal/connection_retry_traits.h
    internal/connection_stub.cc
    internal/connection_stub.h
    internal/connection_stub_factory.cc
    internal/connection_stub_factory.h
    internal/data_transfer_auth_decorator.cc
    internal/data_transfer_auth_decorator.h
    internal/data_transfer_connection_impl.cc
    internal/data_transfer_connection_impl.h
    internal/data_transfer_logging_decorator.cc
    internal/data_transfer_logging_decorator.h
    internal/data_transfer_metadata_decorator.cc
    internal/data_transfer_metadata_decorator.h
    internal/data_transfer_option_defaults.cc
    internal/data_transfer_option_defaults.h
    internal/data_transfer_retry_traits.h
    internal/data_transfer_stub.cc
    internal/data_transfer_stub.h
    internal/data_transfer_stub_factory.cc
    internal/data_transfer_stub_factory.h
    internal/model_auth_decorator.cc
    internal/model_auth_decorator.h
    internal/model_connection_impl.cc
    internal/model_connection_impl.h
    internal/model_logging_decorator.cc
    internal/model_logging_decorator.h
    internal/model_metadata_decorator.cc
    internal/model_metadata_decorator.h
    internal/model_option_defaults.cc
    internal/model_option_defaults.h
    internal/model_retry_traits.h
    internal/model_stub.cc
    internal/model_stub.h
    internal/model_stub_factory.cc
    internal/model_stub_factory.h
    internal/reservation_auth_decorator.cc
    internal/reservation_auth_decorator.h
    internal/reservation_connection_impl.cc
    internal/reservation_connection_impl.h
    internal/reservation_logging_decorator.cc
    internal/reservation_logging_decorator.h
    internal/reservation_metadata_decorator.cc
    internal/reservation_metadata_decorator.h
    internal/reservation_option_defaults.cc
    internal/reservation_option_defaults.h
    internal/reservation_retry_traits.h
    internal/reservation_stub.cc
    internal/reservation_stub.h
    internal/reservation_stub_factory.cc
    internal/reservation_stub_factory.h
    internal/streaming.cc
    model_client.cc
    model_client.h
    model_connection.cc
    model_connection.h
    model_connection_idempotency_policy.cc
    model_connection_idempotency_policy.h
    model_options.h
    reservation_client.cc
    reservation_client.h
    reservation_connection.cc
    reservation_connection.h
    reservation_connection_idempotency_policy.cc
    reservation_connection_idempotency_policy.h
    reservation_options.h
    retry_traits.h)
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
    PROPERTIES EXPORT_NAME google-cloud-cpp::bigquery
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_bigquery
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::bigquery ALIAS google_cloud_cpp_bigquery)

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
    INTERFACE # cmake-format: sort
              ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_bigquery_read_connection.h
              ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_bigquery_write_connection.h
              ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_connection_connection.h
              ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_data_transfer_connection.h
              ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_model_connection.h
              ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_reservation_connection.h)
target_link_libraries(
    google_cloud_cpp_bigquery_mocks
    INTERFACE google-cloud-cpp::bigquery GTest::gmock_main GTest::gmock
              GTest::gtest)
set_target_properties(google_cloud_cpp_bigquery_mocks
                      PROPERTIES EXPORT_NAME google-cloud-cpp::bigquery_mocks)
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
    add_executable(bigquery_quickstart "quickstart/quickstart.cc")
    target_link_libraries(bigquery_quickstart
                          PRIVATE google-cloud-cpp::bigquery)
    google_cloud_cpp_add_common_options(bigquery_quickstart)
    add_test(
        NAME bigquery_quickstart
        COMMAND
            cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
            $<TARGET_FILE:bigquery_quickstart> GOOGLE_CLOUD_PROJECT
            GOOGLE_CLOUD_CPP_BIGQUERY_TEST_QUICKSTART_TABLE)
    set_tests_properties(bigquery_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
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
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google Cloud BigQuery C++ Client Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud BigQuery.")
set(GOOGLE_CLOUD_CPP_PC_LIBS "-lgoogle_cloud_cpp_bigquery")
string(CONCAT GOOGLE_CLOUD_CPP_PC_REQUIRES "google_cloud_cpp_grpc_utils"
              " google_cloud_cpp_common"
              " google_cloud_cpp_cloud_bigquery_protos")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/cmake/templates/config.pc.in"
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

# ~~~
# Copyright 2023 Google LLC
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

add_library(
    google_cloud_cpp_bigquery_rest # cmake-format: sort
    v2/minimal/internal/bigquery_http_response.cc
    v2/minimal/internal/bigquery_http_response.h
    v2/minimal/internal/common_options.h
    v2/minimal/internal/common_v2_resources.cc
    v2/minimal/internal/common_v2_resources.h
    v2/minimal/internal/dataset.cc
    v2/minimal/internal/dataset.h
    v2/minimal/internal/dataset_client.cc
    v2/minimal/internal/dataset_client.h
    v2/minimal/internal/dataset_connection.cc
    v2/minimal/internal/dataset_connection.h
    v2/minimal/internal/dataset_idempotency_policy.cc
    v2/minimal/internal/dataset_idempotency_policy.h
    v2/minimal/internal/dataset_logging.cc
    v2/minimal/internal/dataset_logging.h
    v2/minimal/internal/dataset_metadata.cc
    v2/minimal/internal/dataset_metadata.h
    v2/minimal/internal/dataset_options.cc
    v2/minimal/internal/dataset_options.h
    v2/minimal/internal/dataset_request.cc
    v2/minimal/internal/dataset_request.h
    v2/minimal/internal/dataset_response.cc
    v2/minimal/internal/dataset_response.h
    v2/minimal/internal/dataset_rest_connection_impl.cc
    v2/minimal/internal/dataset_rest_connection_impl.h
    v2/minimal/internal/dataset_rest_stub.cc
    v2/minimal/internal/dataset_rest_stub.h
    v2/minimal/internal/dataset_rest_stub_factory.cc
    v2/minimal/internal/dataset_rest_stub_factory.h
    v2/minimal/internal/dataset_retry_policy.h
    v2/minimal/internal/job.cc
    v2/minimal/internal/job.h
    v2/minimal/internal/job_client.cc
    v2/minimal/internal/job_client.h
    v2/minimal/internal/job_configuration.cc
    v2/minimal/internal/job_configuration.h
    v2/minimal/internal/job_configuration_query.cc
    v2/minimal/internal/job_configuration_query.h
    v2/minimal/internal/job_connection.cc
    v2/minimal/internal/job_connection.h
    v2/minimal/internal/job_idempotency_policy.cc
    v2/minimal/internal/job_idempotency_policy.h
    v2/minimal/internal/job_logging.cc
    v2/minimal/internal/job_logging.h
    v2/minimal/internal/job_metadata.cc
    v2/minimal/internal/job_metadata.h
    v2/minimal/internal/job_options.cc
    v2/minimal/internal/job_options.h
    v2/minimal/internal/job_query_stats.cc
    v2/minimal/internal/job_query_stats.h
    v2/minimal/internal/job_request.cc
    v2/minimal/internal/job_request.h
    v2/minimal/internal/job_response.cc
    v2/minimal/internal/job_response.h
    v2/minimal/internal/job_rest_connection_impl.cc
    v2/minimal/internal/job_rest_connection_impl.h
    v2/minimal/internal/job_rest_stub.cc
    v2/minimal/internal/job_rest_stub.h
    v2/minimal/internal/job_rest_stub_factory.cc
    v2/minimal/internal/job_rest_stub_factory.h
    v2/minimal/internal/job_retry_policy.h
    v2/minimal/internal/job_stats.cc
    v2/minimal/internal/job_stats.h
    v2/minimal/internal/json_utils.cc
    v2/minimal/internal/json_utils.h
    v2/minimal/internal/log_wrapper.h
    v2/minimal/internal/project.cc
    v2/minimal/internal/project.h
    v2/minimal/internal/project_client.cc
    v2/minimal/internal/project_client.h
    v2/minimal/internal/project_connection.cc
    v2/minimal/internal/project_connection.h
    v2/minimal/internal/project_idempotency_policy.cc
    v2/minimal/internal/project_idempotency_policy.h
    v2/minimal/internal/project_logging.cc
    v2/minimal/internal/project_logging.h
    v2/minimal/internal/project_metadata.cc
    v2/minimal/internal/project_metadata.h
    v2/minimal/internal/project_options.cc
    v2/minimal/internal/project_options.h
    v2/minimal/internal/project_request.cc
    v2/minimal/internal/project_request.h
    v2/minimal/internal/project_response.cc
    v2/minimal/internal/project_response.h
    v2/minimal/internal/project_rest_connection_impl.cc
    v2/minimal/internal/project_rest_connection_impl.h
    v2/minimal/internal/project_rest_stub.cc
    v2/minimal/internal/project_rest_stub.h
    v2/minimal/internal/project_rest_stub_factory.cc
    v2/minimal/internal/project_rest_stub_factory.h
    v2/minimal/internal/project_retry_policy.h
    v2/minimal/internal/rest_stub_utils.cc
    v2/minimal/internal/rest_stub_utils.h
    v2/minimal/internal/table.cc
    v2/minimal/internal/table.h
    v2/minimal/internal/table_client.cc
    v2/minimal/internal/table_client.h
    v2/minimal/internal/table_connection.cc
    v2/minimal/internal/table_connection.h
    v2/minimal/internal/table_constraints.cc
    v2/minimal/internal/table_constraints.h
    v2/minimal/internal/table_idempotency_policy.cc
    v2/minimal/internal/table_idempotency_policy.h
    v2/minimal/internal/table_logging.cc
    v2/minimal/internal/table_logging.h
    v2/minimal/internal/table_metadata.cc
    v2/minimal/internal/table_metadata.h
    v2/minimal/internal/table_options.cc
    v2/minimal/internal/table_options.h
    v2/minimal/internal/table_partition.cc
    v2/minimal/internal/table_partition.h
    v2/minimal/internal/table_request.cc
    v2/minimal/internal/table_request.h
    v2/minimal/internal/table_response.cc
    v2/minimal/internal/table_response.h
    v2/minimal/internal/table_rest_connection_impl.cc
    v2/minimal/internal/table_rest_connection_impl.h
    v2/minimal/internal/table_rest_stub.cc
    v2/minimal/internal/table_rest_stub.h
    v2/minimal/internal/table_rest_stub_factory.cc
    v2/minimal/internal/table_rest_stub_factory.h
    v2/minimal/internal/table_retry_policy.h
    v2/minimal/internal/table_schema.cc
    v2/minimal/internal/table_schema.h
    v2/minimal/internal/table_view.cc
    v2/minimal/internal/table_view.h)
target_include_directories(
    google_cloud_cpp_bigquery_rest
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_bigquery_rest PUBLIC google-cloud-cpp::rest_internal
                                          google-cloud-cpp::common)
google_cloud_cpp_add_common_options(google_cloud_cpp_bigquery_rest)
set_target_properties(
    google_cloud_cpp_bigquery_rest
    PROPERTIES EXPORT_NAME google-cloud-cpp::experimental-bigquery_rest
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_bigquery_rest
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::experimental-bigquery_rest ALIAS
            google_cloud_cpp_bigquery_rest)

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_bigquery_rest-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_bigquery_rest"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_bigquery_rest
    EXPORT google_cloud_cpp_bigquery_rest-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_COMPONENT google_cloud_cpp_development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers("google_cloud_cpp_bigquery_rest"
                                 "include/google/cloud/bigquery")

google_cloud_cpp_add_pkgconfig(
    bigquery_rest "Experimental BigQuery REST client library"
    "An experimental BigQuery C++ client library using REST for transport."
    "google_cloud_cpp_rest_internal")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config-rest.cmake.in"
               "google_cloud_cpp_bigquery_rest-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_bigquery_rest-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_bigquery_rest-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_bigquery_rest-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_bigquery_rest"
    COMPONENT google_cloud_cpp_development)

if (GOOGLE_CLOUD_CPP_WITH_MOCKS)
    # Create a header-only library for the mocks.
    add_library(google_cloud_cpp_bigquery_rest_mocks INTERFACE)
    target_sources(
        google_cloud_cpp_bigquery_rest_mocks
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/mocks/mock_dataset_connection.h
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/mocks/mock_job_connection.h
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/mocks/mock_project_connection.h
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/mocks/mock_table_connection.h
    )
    target_link_libraries(
        google_cloud_cpp_bigquery_rest_mocks
        INTERFACE google-cloud-cpp::experimental-bigquery_rest GTest::gmock
                  GTest::gtest)
    set_target_properties(
        google_cloud_cpp_bigquery_rest_mocks
        PROPERTIES EXPORT_NAME
                   google-cloud-cpp::experimental-bigquery_rest_mocks)
    target_include_directories(
        google_cloud_cpp_bigquery_rest_mocks
        INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                  $<INSTALL_INTERFACE:include>)
    target_compile_options(google_cloud_cpp_bigquery_rest_mocks
                           INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
    add_library(google-cloud-cpp::experimental-bigquery_rest_mocks ALIAS
                google_cloud_cpp_bigquery_rest_mocks)
    create_bazel_config(google_cloud_cpp_bigquery_rest_mocks YEAR "2023")

    google_cloud_cpp_install_headers("google_cloud_cpp_bigquery_rest_mocks"
                                     "include/google/cloud/bigquery")
endif ()

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_bigquery_rest YEAR "2023")

# Define the tests in a function so we have a new scope for variable names.
function (bigquery_rest_define_tests)
    # The tests require googletest to be installed. Force CMake to use the
    # config file for googletest (that is, the CMake file installed by
    # googletest itself), because the generic `FindGTest` module does not define
    # the GTest::gmock target, and the target names are also weird.
    find_package(GTest CONFIG REQUIRED)

    add_library(bigquery_rest_testing INTERFACE)
    target_sources(
        bigquery_rest_testing
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/common_v2_test_utils.cc
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/common_v2_test_utils.h
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/job_query_test_utils.cc
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/job_query_test_utils.h
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/job_test_utils.cc
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/job_test_utils.h
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/metadata_test_utils.cc
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/metadata_test_utils.h
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/mock_dataset_rest_stub.h
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/mock_job_rest_stub.h
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/mock_project_rest_stub.h
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/mock_table_rest_stub.h
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/project_test_utils.cc
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/project_test_utils.h
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/table_test_utils.cc
            ${CMAKE_CURRENT_SOURCE_DIR}/v2/minimal/testing/table_test_utils.h)
    target_link_libraries(
        bigquery_rest_testing
        INTERFACE google_cloud_cpp_testing
                  google-cloud-cpp::experimental-bigquery_rest_mocks
                  google-cloud-cpp::experimental-bigquery_rest
                  GTest::gmock_main
                  GTest::gmock
                  GTest::gtest)
    target_include_directories(
        bigquery_rest_testing
        INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                  $<INSTALL_INTERFACE:include>)
    target_compile_options(bigquery_rest_testing
                           INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
    create_bazel_config(bigquery_rest_testing YEAR "2023")

    set(bigquery_rest_unit_tests
        # cmake-format: sort
        v2/minimal/internal/bigquery_http_response_test.cc
        v2/minimal/internal/common_v2_resources_test.cc
        v2/minimal/internal/dataset_client_test.cc
        v2/minimal/internal/dataset_connection_test.cc
        v2/minimal/internal/dataset_idempotency_policy_test.cc
        v2/minimal/internal/dataset_logging_test.cc
        v2/minimal/internal/dataset_metadata_test.cc
        v2/minimal/internal/dataset_options_test.cc
        v2/minimal/internal/dataset_request_test.cc
        v2/minimal/internal/dataset_response_test.cc
        v2/minimal/internal/dataset_rest_stub_test.cc
        v2/minimal/internal/dataset_test.cc
        v2/minimal/internal/job_client_test.cc
        v2/minimal/internal/job_configuration_query_test.cc
        v2/minimal/internal/job_configuration_test.cc
        v2/minimal/internal/job_connection_test.cc
        v2/minimal/internal/job_idempotency_policy_test.cc
        v2/minimal/internal/job_logging_test.cc
        v2/minimal/internal/job_metadata_test.cc
        v2/minimal/internal/job_options_test.cc
        v2/minimal/internal/job_query_stats_test.cc
        v2/minimal/internal/job_request_test.cc
        v2/minimal/internal/job_response_test.cc
        v2/minimal/internal/job_rest_stub_test.cc
        v2/minimal/internal/job_stats_test.cc
        v2/minimal/internal/job_test.cc
        v2/minimal/internal/json_utils_test.cc
        v2/minimal/internal/project_client_test.cc
        v2/minimal/internal/project_connection_test.cc
        v2/minimal/internal/project_idempotency_policy_test.cc
        v2/minimal/internal/project_logging_test.cc
        v2/minimal/internal/project_metadata_test.cc
        v2/minimal/internal/project_options_test.cc
        v2/minimal/internal/project_request_test.cc
        v2/minimal/internal/project_response_test.cc
        v2/minimal/internal/project_rest_stub_test.cc
        v2/minimal/internal/project_test.cc
        v2/minimal/internal/rest_stub_utils_test.cc
        v2/minimal/internal/table_client_test.cc
        v2/minimal/internal/table_connection_test.cc
        v2/minimal/internal/table_idempotency_policy_test.cc
        v2/minimal/internal/table_logging_test.cc
        v2/minimal/internal/table_metadata_test.cc
        v2/minimal/internal/table_options_test.cc
        v2/minimal/internal/table_request_test.cc
        v2/minimal/internal/table_response_test.cc
        v2/minimal/internal/table_rest_stub_test.cc
        v2/minimal/internal/table_test.cc)

    # Export the list of unit tests to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("bigquery_rest_unit_tests.bzl"
                         "bigquery_rest_unit_tests" YEAR "2023")

    # Create a custom target so we can say "build all the tests"
    add_custom_target(bigquery_rest-tests)

    # Generate a target for each unit test.
    foreach (fname ${bigquery_rest_unit_tests})
        google_cloud_cpp_add_executable(target "bigquery" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE bigquery_rest_testing
                    google_cloud_cpp_testing
                    google-cloud-cpp::experimental-bigquery_rest
                    google-cloud-cpp::experimental-bigquery_rest_mocks
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
        add_test(NAME ${target} COMMAND ${target})
        add_dependencies(bigquery_rest-tests ${target})
    endforeach ()
endfunction ()

if (BUILD_TESTING)
    bigquery_rest_define_tests()
endif ()

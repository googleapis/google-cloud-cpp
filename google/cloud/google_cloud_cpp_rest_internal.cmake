# ~~~
# Copyright 2022 Google LLC
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

include(IncludeNlohmannJson)
include(FindCurlWithTargets)
find_package(OpenSSL REQUIRED)

# the library
add_library(
    google_cloud_cpp_rest_internal # cmake-format: sort
    internal/binary_data_as_debug_string.cc
    internal/binary_data_as_debug_string.h
    internal/curl_handle.cc
    internal/curl_handle.h
    internal/curl_handle_factory.cc
    internal/curl_handle_factory.h
    internal/curl_http_payload.cc
    internal/curl_http_payload.h
    internal/curl_impl.cc
    internal/curl_impl.h
    internal/curl_options.h
    internal/curl_rest_client.cc
    internal/curl_rest_client.h
    internal/curl_rest_response.cc
    internal/curl_rest_response.h
    internal/curl_wrappers.cc
    internal/curl_wrappers.h
    internal/external_account_parsing.cc
    internal/external_account_parsing.h
    internal/external_account_token_source_file.cc
    internal/external_account_token_source_file.h
    internal/http_payload.h
    internal/make_jwt_assertion.cc
    internal/make_jwt_assertion.h
    internal/oauth2_access_token_credentials.cc
    internal/oauth2_access_token_credentials.h
    internal/oauth2_anonymous_credentials.cc
    internal/oauth2_anonymous_credentials.h
    internal/oauth2_authorized_user_credentials.cc
    internal/oauth2_authorized_user_credentials.h
    internal/oauth2_compute_engine_credentials.cc
    internal/oauth2_compute_engine_credentials.h
    internal/oauth2_credential_constants.h
    internal/oauth2_credentials.cc
    internal/oauth2_credentials.h
    internal/oauth2_error_credentials.cc
    internal/oauth2_error_credentials.h
    internal/oauth2_google_application_default_credentials_file.cc
    internal/oauth2_google_application_default_credentials_file.h
    internal/oauth2_google_credentials.cc
    internal/oauth2_google_credentials.h
    internal/oauth2_impersonate_service_account_credentials.cc
    internal/oauth2_impersonate_service_account_credentials.h
    internal/oauth2_minimal_iam_credentials_rest.cc
    internal/oauth2_minimal_iam_credentials_rest.h
    internal/oauth2_refreshing_credentials_wrapper.cc
    internal/oauth2_refreshing_credentials_wrapper.h
    internal/oauth2_service_account_credentials.cc
    internal/oauth2_service_account_credentials.h
    internal/openssl_util.cc
    internal/openssl_util.h
    internal/rest_client.h
    internal/rest_context.cc
    internal/rest_context.h
    internal/rest_options.h
    internal/rest_parse_json_error.cc
    internal/rest_parse_json_error.h
    internal/rest_request.cc
    internal/rest_request.h
    internal/rest_response.cc
    internal/rest_response.h
    internal/unified_rest_credentials.cc
    internal/unified_rest_credentials.h
    rest_options.h)
target_link_libraries(
    google_cloud_cpp_rest_internal
    PUBLIC absl::span google-cloud-cpp::common CURL::libcurl
           nlohmann_json::nlohmann_json OpenSSL::SSL OpenSSL::Crypto)
google_cloud_cpp_add_common_options(google_cloud_cpp_rest_internal)
target_include_directories(
    google_cloud_cpp_rest_internal
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_rest_internal
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(
    google_cloud_cpp_rest_internal
    PROPERTIES EXPORT_NAME "google-cloud-cpp::rest_internal"
               VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(google-cloud-cpp::rest_internal ALIAS
            google_cloud_cpp_rest_internal)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_rest_internal-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_rest_internal"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_rest_internal
    EXPORT google_cloud_cpp_rest_internal-targets
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
    TARGETS google_cloud_cpp_rest_internal
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers(google_cloud_cpp_rest_internal
                                 include/google/cloud)

google_cloud_cpp_add_pkgconfig(
    rest_internal "REST library for the Google Cloud C++ Client Library"
    "Provides REST Transport for the Google Cloud C++ Client Library."
    "google_cloud_cpp_common" " libcurl" " openssl")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config-rest.cmake.in"
               "google_cloud_cpp_rest_internal-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_rest_internal-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_rest_internal-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_rest_internal-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_rest_internal"
    COMPONENT google_cloud_cpp_development)

create_bazel_config(google_cloud_cpp_rest_internal YEAR 2021)

# Define the unit tests in a function so we have a new scope for variable names.
function (google_cloud_cpp_rest_internal_add_test fname labels)
    google_cloud_cpp_add_executable(target "rest_internal" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE google-cloud-cpp::rest_internal
                google_cloud_cpp_testing
                google_cloud_cpp_testing_rest
                google-cloud-cpp::common
                absl::span
                GTest::gmock_main
                GTest::gmock
                GTest::gtest
                CURL::libcurl)
    google_cloud_cpp_add_common_options(${target})
    if (MSVC)
        target_compile_options(${target} PRIVATE "/bigobj")
    endif ()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "${labels}")
endfunction ()

if (BUILD_TESTING)
    # List the unit tests, then setup the targets and dependencies.
    set(google_cloud_cpp_rest_internal_unit_tests
        # cmake-format: sort
        internal/binary_data_as_debug_string_test.cc
        internal/curl_handle_factory_test.cc
        internal/curl_handle_test.cc
        internal/curl_http_payload_test.cc
        internal/curl_impl_test.cc
        internal/curl_rest_client_test.cc
        internal/curl_wrappers_disable_sigpipe_handler_test.cc
        internal/curl_wrappers_enable_sigpipe_handler_test.cc
        internal/curl_wrappers_locking_already_present_test.cc
        internal/curl_wrappers_locking_disabled_test.cc
        internal/curl_wrappers_locking_enabled_test.cc
        internal/curl_wrappers_test.cc
        internal/external_account_parsing_test.cc
        internal/external_account_token_source_file_test.cc
        internal/make_jwt_assertion_test.cc
        internal/oauth2_access_token_credentials_test.cc
        internal/oauth2_anonymous_credentials_test.cc
        internal/oauth2_authorized_user_credentials_test.cc
        internal/oauth2_compute_engine_credentials_test.cc
        internal/oauth2_google_application_default_credentials_file_test.cc
        internal/oauth2_google_credentials_test.cc
        internal/oauth2_impersonate_service_account_credentials_test.cc
        internal/oauth2_minimal_iam_credentials_rest_test.cc
        internal/oauth2_refreshing_credentials_wrapper_test.cc
        internal/oauth2_service_account_credentials_test.cc
        internal/openssl_util_test.cc
        internal/rest_context_test.cc
        internal/rest_parse_json_error_test.cc
        internal/rest_request_test.cc
        internal/rest_response_test.cc
        internal/unified_rest_credentials_test.cc)

    # List the emulator integration tests, then setup the targets and
    # dependencies.
    set(google_cloud_cpp_rest_internal_emulator_integration_tests
        # cmake-format: sort
        internal/curl_rest_client_integration_test.cc)

    # List the production integration tests, then setup the targets and
    # dependencies.
    set(google_cloud_cpp_rest_internal_production_integration_tests
        # cmake-format: sort
        internal/unified_rest_credentials_integration_test.cc)

    # Export the list of unit and integration tests so the Bazel BUILD file can
    # pick them up.
    export_list_to_bazel("google_cloud_cpp_rest_internal_unit_tests.bzl"
                         "google_cloud_cpp_rest_internal_unit_tests" YEAR 2021)
    export_list_to_bazel(
        "google_cloud_cpp_rest_internal_emulator_integration_tests.bzl"
        "google_cloud_cpp_rest_internal_emulator_integration_tests" YEAR 2022)
    export_list_to_bazel(
        "google_cloud_cpp_rest_internal_production_integration_tests.bzl"
        "google_cloud_cpp_rest_internal_production_integration_tests" YEAR 2022)

    foreach (fname ${google_cloud_cpp_rest_internal_unit_tests})
        google_cloud_cpp_rest_internal_add_test("${fname}" "")
    endforeach ()

    foreach (fname ${google_cloud_cpp_rest_internal_emulator_integration_tests})
        google_cloud_cpp_rest_internal_add_test(
            "${fname}" "integration-test;integration-test-emulator")
    endforeach ()

    foreach (fname
             ${google_cloud_cpp_rest_internal_production_integration_tests})
        google_cloud_cpp_rest_internal_add_test(
            "${fname}" "integration-test;integration-test-production")
    endforeach ()

    set(google_cloud_cpp_rest_internal_benchmarks
        # cmake-format: sortable
        internal/curl_handle_factory_benchmark.cc)

    # Export the list of benchmarks to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel(
        "google_cloud_cpp_rest_internal_benchmarks.bzl"
        "google_cloud_cpp_rest_internal_benchmarks" YEAR "2022")

    # Generate a target for each benchmark.
    foreach (fname ${google_cloud_cpp_rest_internal_benchmarks})
        google_cloud_cpp_add_executable(target "common" "${fname}")
        add_test(NAME ${target} COMMAND ${target})
        target_link_libraries(
            ${target}
            PRIVATE google-cloud-cpp::rest_internal google-cloud-cpp::common
                    benchmark::benchmark_main)
        google_cloud_cpp_add_common_options(${target})
    endforeach ()
endif ()

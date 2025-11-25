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

find_package(CURL REQUIRED)
if (NOT WIN32)
    find_package(OpenSSL REQUIRED)
endif ()

# the client library
add_library(
    google_cloud_cpp_storage # cmake-format: sort
    auto_finalize.cc
    auto_finalize.h
    bucket_access_control.cc
    bucket_access_control.h
    bucket_autoclass.cc
    bucket_autoclass.h
    bucket_billing.h
    bucket_cors_entry.cc
    bucket_cors_entry.h
    bucket_custom_placement_config.cc
    bucket_custom_placement_config.h
    bucket_encryption.h
    bucket_hierarchical_namespace.cc
    bucket_hierarchical_namespace.h
    bucket_iam_configuration.cc
    bucket_iam_configuration.h
    bucket_ip_filter.cc
    bucket_ip_filter.h
    bucket_lifecycle.h
    bucket_logging.cc
    bucket_logging.h
    bucket_metadata.cc
    bucket_metadata.h
    bucket_object_retention.cc
    bucket_object_retention.h
    bucket_retention_policy.cc
    bucket_retention_policy.h
    bucket_rpo.h
    bucket_soft_delete_policy.cc
    bucket_soft_delete_policy.h
    bucket_versioning.h
    bucket_website.h
    client.cc
    client.h
    client_options.cc
    client_options.h
    download_options.h
    enable_object_retention.h
    hash_mismatch_error.h
    hashing_options.cc
    hashing_options.h
    headers_map.h
    hmac_key_metadata.cc
    hmac_key_metadata.h
    iam_policy.cc
    iam_policy.h
    idempotency_policy.cc
    idempotency_policy.h
    include_folders_as_prefixes.h
    internal/access_control_common.h
    internal/access_control_common_parser.cc
    internal/access_control_common_parser.h
    internal/access_token_credentials.cc
    internal/access_token_credentials.h
    internal/base64.cc
    internal/base64.h
    internal/binary_data_as_debug_string.h
    internal/bucket_access_control_parser.cc
    internal/bucket_access_control_parser.h
    internal/bucket_acl_requests.cc
    internal/bucket_acl_requests.h
    internal/bucket_metadata_parser.cc
    internal/bucket_metadata_parser.h
    internal/bucket_requests.cc
    internal/bucket_requests.h
    internal/common_metadata.h
    internal/common_metadata_parser.h
    internal/complex_option.h
    internal/compute_engine_util.cc
    internal/compute_engine_util.h
    internal/connection_factory.cc
    internal/connection_factory.h
    internal/connection_impl.cc
    internal/connection_impl.h
    internal/const_buffer.cc
    internal/const_buffer.h
    internal/crc32c.cc
    internal/crc32c.h
    internal/curl/request.h
    internal/curl/request_builder.h
    internal/default_object_acl_requests.cc
    internal/default_object_acl_requests.h
    internal/empty_response.cc
    internal/empty_response.h
    internal/error_credentials.cc
    internal/error_credentials.h
    internal/generate_message_boundary.cc
    internal/generate_message_boundary.h
    internal/generic_object_request.h
    internal/generic_request.h
    internal/generic_stub.h
    internal/generic_stub_adapter.cc
    internal/generic_stub_adapter.h
    internal/generic_stub_factory.cc
    internal/generic_stub_factory.h
    internal/hash_function.cc
    internal/hash_function.h
    internal/hash_function_impl.cc
    internal/hash_function_impl.h
    internal/hash_validator.cc
    internal/hash_validator.h
    internal/hash_validator_impl.cc
    internal/hash_validator_impl.h
    internal/hash_values.cc
    internal/hash_values.h
    internal/hmac_key_metadata_parser.cc
    internal/hmac_key_metadata_parser.h
    internal/hmac_key_requests.cc
    internal/hmac_key_requests.h
    internal/http_response.cc
    internal/http_response.h
    internal/impersonate_service_account_credentials.cc
    internal/impersonate_service_account_credentials.h
    internal/lifecycle_rule_parser.cc
    internal/lifecycle_rule_parser.h
    internal/logging_stub.cc
    internal/logging_stub.h
    internal/make_jwt_assertion.cc
    internal/make_jwt_assertion.h
    internal/md5hash.cc
    internal/md5hash.h
    internal/metadata_parser.cc
    internal/metadata_parser.h
    internal/notification_metadata_parser.cc
    internal/notification_metadata_parser.h
    internal/notification_requests.cc
    internal/notification_requests.h
    internal/object_access_control_parser.cc
    internal/object_access_control_parser.h
    internal/object_acl_requests.cc
    internal/object_acl_requests.h
    internal/object_metadata_parser.cc
    internal/object_metadata_parser.h
    internal/object_read_source.h
    internal/object_read_streambuf.cc
    internal/object_read_streambuf.h
    internal/object_requests.cc
    internal/object_requests.h
    internal/object_write_streambuf.cc
    internal/object_write_streambuf.h
    internal/openssl/hash_function_impl.cc
    internal/patch_builder.cc
    internal/patch_builder.h
    internal/patch_builder_details.cc
    internal/patch_builder_details.h
    internal/policy_document_request.cc
    internal/policy_document_request.h
    internal/request_project_id.cc
    internal/request_project_id.h
    internal/rest/object_read_source.cc
    internal/rest/object_read_source.h
    internal/rest/request_builder.cc
    internal/rest/request_builder.h
    internal/rest/stub.cc
    internal/rest/stub.h
    internal/retry_object_read_source.cc
    internal/retry_object_read_source.h
    internal/service_account_parser.cc
    internal/service_account_parser.h
    internal/service_account_requests.cc
    internal/service_account_requests.h
    internal/sign_blob_requests.cc
    internal/sign_blob_requests.h
    internal/signed_url_requests.cc
    internal/signed_url_requests.h
    internal/storage_connection.cc
    internal/storage_connection.h
    internal/tracing_connection.cc
    internal/tracing_connection.h
    internal/tracing_object_read_source.cc
    internal/tracing_object_read_source.h
    internal/tuple_filter.h
    internal/unified_rest_credentials.cc
    internal/unified_rest_credentials.h
    internal/well_known_parameters_impl.h
    internal/win32/hash_function_impl.cc
    lifecycle_rule.cc
    lifecycle_rule.h
    list_buckets_reader.cc
    list_buckets_reader.h
    list_hmac_keys_reader.cc
    list_hmac_keys_reader.h
    list_objects_and_prefixes_reader.h
    list_objects_reader.cc
    list_objects_reader.h
    notification_event_type.h
    notification_metadata.cc
    notification_metadata.h
    notification_payload_format.h
    oauth2/anonymous_credentials.cc
    oauth2/anonymous_credentials.h
    oauth2/authorized_user_credentials.cc
    oauth2/authorized_user_credentials.h
    oauth2/compute_engine_credentials.cc
    oauth2/compute_engine_credentials.h
    oauth2/credential_constants.h
    oauth2/credentials.cc
    oauth2/credentials.h
    oauth2/google_application_default_credentials_file.cc
    oauth2/google_application_default_credentials_file.h
    oauth2/google_credentials.cc
    oauth2/google_credentials.h
    oauth2/refreshing_credentials_wrapper.cc
    oauth2/refreshing_credentials_wrapper.h
    oauth2/service_account_credentials.cc
    oauth2/service_account_credentials.h
    object_access_control.cc
    object_access_control.h
    object_metadata.cc
    object_metadata.h
    object_read_stream.cc
    object_read_stream.h
    object_retention.cc
    object_retention.h
    object_rewriter.cc
    object_rewriter.h
    object_stream.h
    object_write_stream.cc
    object_write_stream.h
    options.h
    override_default_project.h
    override_unlocked_retention.h
    owner.h
    parallel_upload.cc
    parallel_upload.h
    policy_document.cc
    policy_document.h
    project_team.h
    retry_policy.h
    service_account.cc
    service_account.h
    signed_url_options.h
    soft_deleted.h
    storage_class.h
    upload_options.h
    user_ip_option.h
    version.cc
    version.h
    version_info.h
    well_known_headers.cc
    well_known_headers.h
    well_known_parameters.cc
    well_known_parameters.h)
target_link_libraries(
    google_cloud_cpp_storage
    PUBLIC absl::cord
           absl::memory
           absl::strings
           absl::time
           absl::variant
           google-cloud-cpp::common
           google-cloud-cpp::rest_internal
           nlohmann_json::nlohmann_json
           Crc32c::crc32c
           CURL::libcurl
           Threads::Threads)
if (WIN32)
    target_compile_definitions(google_cloud_cpp_storage
                               PRIVATE WIN32_LEAN_AND_MEAN)
    # We use `setsockopt()` directly, which requires the ws2_32 (Winsock2 for
    # Windows32?) library on Windows.
    target_link_libraries(google_cloud_cpp_storage PUBLIC ws2_32 bcrypt)
else ()
    target_link_libraries(google_cloud_cpp_storage PUBLIC OpenSSL::Crypto)
endif ()
google_cloud_cpp_add_common_options(google_cloud_cpp_storage)
target_include_directories(
    google_cloud_cpp_storage PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                    $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_storage
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

# GCC-7.x issues a warning (a member variable may be used without being
# initialized), in this file. GCC-8.0 no longer emits that diagnostic, and
# neither does Clang. On the assumption that this is a spurious warning we
# disable it for older versions of GCC.
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" AND ${CMAKE_CXX_COMPILER_VERSION}
                                                VERSION_LESS 8.0)
    set_property(
        SOURCE list_objects_reader.cc
        APPEND_STRING
        PROPERTY COMPILE_FLAGS "-Wno-maybe-uninitialized")
endif ()

set_target_properties(
    google_cloud_cpp_storage
    PROPERTIES EXPORT_NAME "google-cloud-cpp::storage"
               VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(google-cloud-cpp::storage ALIAS google_cloud_cpp_storage)

create_bazel_config(google_cloud_cpp_storage)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT storage-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_storage"
    COMPONENT google_cloud_cpp_development)

install(
    TARGETS google_cloud_cpp_storage
    EXPORT storage-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_COMPONENT google_cloud_cpp_development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers(google_cloud_cpp_storage
                                 include/google/cloud/storage)

google_cloud_cpp_add_pkgconfig(
    "storage"
    "The Google Cloud Storage C++ Client Library"
    "Provides C++ APIs to access Google Cloud Storage."
    "google_cloud_cpp_common"
    "google_cloud_cpp_rest_internal"
    "libcurl"
    "absl_cord"
    "absl_strings"
    "absl_str_format"
    "absl_time"
    "absl_variant"
    NON_WIN32_REQUIRES
    openssl
    LIBS
    crc32c
    WIN32_LIBS
    ws2_32
    bcrypt)

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_storage-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_storage-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_storage-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_storage-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_storage"
    COMPONENT google_cloud_cpp_development)

install(
    FILES testing/mock_client.h
    DESTINATION include/google/cloud/storage/testing
    COMPONENT google_cloud_cpp_development)

if (BUILD_TESTING)
    add_library(
        storage_client_testing # cmake-format: sort
        testing/canonical_errors.h
        testing/client_unit_test.cc
        testing/client_unit_test.h
        testing/constants.h
        testing/mock_client.h
        testing/mock_generic_stub.h
        testing/mock_hash_function.h
        testing/mock_hash_validator.h
        testing/mock_http_request.cc
        testing/mock_http_request.h
        testing/mock_resume_policy.h
        testing/mock_storage_stub.h
        testing/object_integration_test.cc
        testing/object_integration_test.h
        testing/random_names.cc
        testing/random_names.h
        testing/remove_stale_buckets.cc
        testing/remove_stale_buckets.h
        testing/retry_http_request.cc
        testing/retry_http_request.h
        testing/retry_tests.cc
        testing/retry_tests.h
        testing/storage_integration_test.cc
        testing/storage_integration_test.h
        testing/temp_file.cc
        testing/temp_file.h
        testing/upload_hash_cases.cc
        testing/upload_hash_cases.h
        testing/write_base64.cc
        testing/write_base64.h)
    target_link_libraries(
        storage_client_testing
        PUBLIC absl::memory
               google-cloud-cpp::storage
               google_cloud_cpp_testing
               nlohmann_json::nlohmann_json
               GTest::gmock_main
               GTest::gmock
               GTest::gtest)
    google_cloud_cpp_add_common_options(storage_client_testing)
    target_include_directories(
        storage_client_testing PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                      $<INSTALL_INTERFACE:include>)
    target_compile_options(storage_client_testing
                           PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
    create_bazel_config(storage_client_testing)

    # List the unit tests, then setup the targets and dependencies.
    set(storage_client_unit_tests
        # cmake-format: sort
        auto_finalize_test.cc
        bucket_access_control_test.cc
        bucket_cors_entry_test.cc
        bucket_iam_configuration_test.cc
        bucket_ip_filter_test.cc
        bucket_metadata_test.cc
        bucket_object_retention_test.cc
        bucket_soft_delete_policy_test.cc
        client_bucket_acl_test.cc
        client_bucket_test.cc
        client_default_object_acl_test.cc
        client_notifications_test.cc
        client_object_acl_test.cc
        client_object_copy_test.cc
        client_object_test.cc
        client_options_test.cc
        client_service_account_test.cc
        client_sign_policy_document_test.cc
        client_sign_url_test.cc
        client_test.cc
        client_write_object_test.cc
        compose_many_test.cc
        delete_by_prefix_test.cc
        hashing_options_test.cc
        hmac_key_metadata_test.cc
        idempotency_policy_test.cc
        internal/access_control_common_parser_test.cc
        internal/access_control_common_test.cc
        internal/access_token_credentials_test.cc
        internal/base64_test.cc
        internal/bucket_acl_requests_test.cc
        internal/bucket_requests_test.cc
        internal/complex_option_test.cc
        internal/compute_engine_util_test.cc
        internal/connection_impl_bucket_acl_test.cc
        internal/connection_impl_bucket_test.cc
        internal/connection_impl_default_object_acl_test.cc
        internal/connection_impl_file_upload_test.cc
        internal/connection_impl_notifications_test.cc
        internal/connection_impl_object_acl_test.cc
        internal/connection_impl_object_copy_test.cc
        internal/connection_impl_object_test.cc
        internal/connection_impl_service_account_test.cc
        internal/connection_impl_sign_blob_test.cc
        internal/connection_impl_test.cc
        internal/const_buffer_test.cc
        internal/crc32c_test.cc
        internal/default_object_acl_requests_test.cc
        internal/generate_message_boundary_test.cc
        internal/generic_request_test.cc
        internal/hash_function_impl_test.cc
        internal/hash_validator_test.cc
        internal/hash_values_test.cc
        internal/hmac_key_requests_test.cc
        internal/http_response_test.cc
        internal/impersonate_service_account_credentials_test.cc
        internal/logging_stub_test.cc
        internal/make_jwt_assertion_test.cc
        internal/md5hash_test.cc
        internal/metadata_parser_test.cc
        internal/notification_requests_test.cc
        internal/object_acl_requests_test.cc
        internal/object_read_streambuf_test.cc
        internal/object_requests_test.cc
        internal/object_write_streambuf_test.cc
        internal/patch_builder_test.cc
        internal/policy_document_request_test.cc
        internal/request_project_id_test.cc
        internal/rest/object_read_source_test.cc
        internal/rest/request_builder_test.cc
        internal/rest/stub_test.cc
        internal/retry_object_read_source_test.cc
        internal/service_account_requests_test.cc
        internal/sign_blob_requests_test.cc
        internal/signed_url_requests_test.cc
        internal/storage_connection_test.cc
        internal/tracing_connection_test.cc
        internal/tracing_object_read_source_test.cc
        internal/tuple_filter_test.cc
        internal/unified_rest_credentials_test.cc
        lifecycle_rule_test.cc
        list_buckets_reader_test.cc
        list_hmac_keys_reader_test.cc
        list_objects_and_prefixes_reader_test.cc
        list_objects_reader_test.cc
        notification_metadata_test.cc
        oauth2/anonymous_credentials_test.cc
        oauth2/authorized_user_credentials_test.cc
        oauth2/compute_engine_credentials_test.cc
        oauth2/google_application_default_credentials_file_test.cc
        oauth2/google_credentials_test.cc
        oauth2/service_account_credentials_test.cc
        object_access_control_test.cc
        object_metadata_test.cc
        object_retention_test.cc
        object_stream_test.cc
        parallel_uploads_test.cc
        policy_document_test.cc
        retry_policy_test.cc
        service_account_test.cc
        signed_url_options_test.cc
        storage_class_test.cc
        storage_iam_policy_test.cc
        storage_version_test.cc
        testing/remove_stale_buckets_test.cc
        well_known_headers_test.cc
        well_known_parameters_test.cc)

    foreach (fname ${storage_client_unit_tests})
        google_cloud_cpp_add_executable(target "storage" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE absl::memory
                    storage_client_testing
                    google_cloud_cpp_testing
                    google_cloud_cpp_testing_rest
                    google-cloud-cpp::storage
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest
                    CURL::libcurl
                    nlohmann_json::nlohmann_json)
        google_cloud_cpp_add_common_options(${target})
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()
    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("storage_client_unit_tests.bzl"
                         "storage_client_unit_tests" YEAR "2018")

    include(FindBenchmarkWithWorkarounds)

    set(storage_client_benchmarks # cmake-format: sort
                                  internal/crc32c_benchmark.cc)

    # Export the list of benchmarks to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("storage_client_benchmarks.bzl"
                         "storage_client_benchmarks" YEAR "2023")

    # Generate a target for each benchmark.
    foreach (fname IN LISTS storage_client_benchmarks)
        google_cloud_cpp_add_executable(target "storage" "${fname}")
        add_test(NAME ${target} COMMAND ${target})
        target_link_libraries(
            ${target} PRIVATE google-cloud-cpp::storage storage_client_testing
                              benchmark::benchmark_main)
        google_cloud_cpp_add_common_options(${target})
    endforeach ()

    add_subdirectory(tests)
    add_subdirectory(benchmarks)
endif ()

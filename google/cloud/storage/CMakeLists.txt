# ~~~
# Copyright 2018 Google LLC
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

# TODO(#4146) - remove FPHSA_NAME_MISMATCHED manipulation on next absl release
set(FPHSA_NAME_MISMATCHED Threads) # Quiet warning caused by Abseil
find_package(absl CONFIG REQUIRED)
unset(FPHSA_NAME_MISMATCHED)

option(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC
       "Enable compilation for the GCS gRPC plugin (EXPERIMENTAL)" OFF)
mark_as_advanced(GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC)

set(DOXYGEN_PROJECT_NAME "Google Cloud Storage C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Storage")
set(DOXYGEN_PROJECT_NUMBER
    "${GOOGLE_CLOUD_CPP_VERSION_MAJOR}.${GOOGLE_CLOUD_CPP_VERSION_MINOR}.${GOOGLE_CLOUD_CPP_VERSION_PATCH}"
)
set(DOXYGEN_EXAMPLE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/examples"
                         "${CMAKE_CURRENT_SOURCE_DIR}/quickstart")
set(DOXYGEN_PREDEFINED "GOOGLE_CLOUD_CPP_NS=v1"
                       "STORAGE_CLIENT_NS=v${GOOGLE_CLOUD_CPP_VERSION_MAJOR}")
set(DOXYGEN_EXCLUDE_PATTERNS
    "*/google/cloud/storage/README.md"
    "*/google/cloud/storage/ci/*"
    "*/google/cloud/storage/examples/*.md"
    "*/google/cloud/storage/internal/*"
    "*/google/cloud/storage/testing/*"
    "*/google/cloud/storage/tests/*"
    "*/google/cloud/storage/*_test.cc")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "storage_benchmarks")

include(GoogleCloudCppCommon)

include(IncludeNlohmannJson)
find_package(Crc32c)

# Generate the version information from the CMake values.
configure_file(version_info.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version_info.h)

# Export the version information for Bazel.
include(CreateBazelConfig)

# Enable unit tests
include(CTest)

include(FindCurlWithTargets)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# the client library
add_library(
    storage_client # cmake-format: sort
    bucket_access_control.cc
    bucket_access_control.h
    bucket_metadata.cc
    bucket_metadata.h
    client.cc
    client.h
    client_options.cc
    client_options.h
    download_options.h
    hashing_options.cc
    hashing_options.h
    hmac_key_metadata.cc
    hmac_key_metadata.h
    iam_policy.cc
    iam_policy.h
    idempotency_policy.cc
    idempotency_policy.h
    internal/access_control_common.h
    internal/access_control_common_parser.cc
    internal/access_control_common_parser.h
    internal/binary_data_as_debug_string.cc
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
    internal/const_buffer.cc
    internal/const_buffer.h
    internal/curl_client.cc
    internal/curl_client.h
    internal/curl_download_request.cc
    internal/curl_download_request.h
    internal/curl_handle.cc
    internal/curl_handle.h
    internal/curl_handle_factory.cc
    internal/curl_handle_factory.h
    internal/curl_request.cc
    internal/curl_request.h
    internal/curl_request_builder.cc
    internal/curl_request_builder.h
    internal/curl_resumable_upload_session.cc
    internal/curl_resumable_upload_session.h
    internal/curl_wrappers.cc
    internal/curl_wrappers.h
    internal/default_object_acl_requests.cc
    internal/default_object_acl_requests.h
    internal/empty_response.cc
    internal/empty_response.h
    internal/generate_message_boundary.h
    internal/generic_object_request.h
    internal/generic_request.h
    internal/hash_validator.cc
    internal/hash_validator.h
    internal/hash_validator_impl.cc
    internal/hash_validator_impl.h
    internal/hmac_key_metadata_parser.cc
    internal/hmac_key_metadata_parser.h
    internal/hmac_key_requests.cc
    internal/hmac_key_requests.h
    internal/http_response.cc
    internal/http_response.h
    internal/lifecycle_rule_parser.cc
    internal/lifecycle_rule_parser.h
    internal/logging_client.cc
    internal/logging_client.h
    internal/logging_resumable_upload_session.cc
    internal/logging_resumable_upload_session.h
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
    internal/object_requests.cc
    internal/object_requests.h
    internal/object_streambuf.cc
    internal/object_streambuf.h
    internal/openssl_util.cc
    internal/openssl_util.h
    internal/parameter_pack_validation.h
    internal/patch_builder.cc
    internal/patch_builder.h
    internal/policy_document_request.cc
    internal/policy_document_request.h
    internal/range_from_pagination.h
    internal/raw_client.h
    internal/raw_client_wrapper_utils.h
    internal/resumable_upload_session.cc
    internal/resumable_upload_session.h
    internal/retry_client.cc
    internal/retry_client.h
    internal/retry_object_read_source.cc
    internal/retry_object_read_source.h
    internal/retry_resumable_upload_session.cc
    internal/retry_resumable_upload_session.h
    internal/service_account_parser.cc
    internal/service_account_parser.h
    internal/service_account_requests.cc
    internal/service_account_requests.h
    internal/sha256_hash.cc
    internal/sha256_hash.h
    internal/sign_blob_requests.cc
    internal/sign_blob_requests.h
    internal/signed_url_requests.cc
    internal/signed_url_requests.h
    internal/tuple_filter.h
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
    object_rewriter.cc
    object_rewriter.h
    object_stream.cc
    object_stream.h
    override_default_project.h
    parallel_upload.cc
    parallel_upload.h
    policy_document.cc
    policy_document.h
    retry_policy.h
    service_account.cc
    service_account.h
    signed_url_options.h
    storage_class.h
    upload_options.h
    version.cc
    version.h
    version_info.h
    well_known_headers.cc
    well_known_headers.h
    well_known_parameters.cc
    well_known_parameters.h)
target_link_libraries(
    storage_client
    PUBLIC absl::memory
           absl::strings
           absl::str_format
           absl::time
           absl::variant
           google_cloud_cpp_common
           nlohmann_json::nlohmann_json
           Crc32c::crc32c
           CURL::libcurl
           Threads::Threads
           OpenSSL::SSL
           OpenSSL::Crypto
           ZLIB::ZLIB)
google_cloud_cpp_add_common_options(storage_client)
target_include_directories(
    storage_client PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                          $<INSTALL_INTERFACE:include>)
target_compile_options(storage_client
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

# GCC-7.3 (the default GCC version on Ubuntu:18.04) issues a warning (a member
# variable may be used without being initialized), in this file. GCC-8.0 no
# longers emits that diagnostic, and neither does Clang. On the assumption that
# this is a spurious warning we disable it for older versions of GCC. I (coryan)
# did not research in exactly what version was this warning introduced, and when
# it was fixed. I do not believe we need to be that accurate.
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" AND ${CMAKE_CXX_COMPILER_VERSION}
                                                VERSION_LESS 8.0)
    set_property(
        SOURCE list_objects_reader.cc
        APPEND_STRING
        PROPERTY COMPILE_FLAGS "-Wno-maybe-uninitialized")
endif ()

set_target_properties(
    storage_client
    PROPERTIES
        VERSION
        ${GOOGLE_CLOUD_CPP_VERSION_MAJOR}.${GOOGLE_CLOUD_CPP_VERSION_MINOR}.${GOOGLE_CLOUD_CPP_VERSION_PATCH}
        SOVERSION ${GOOGLE_CLOUD_CPP_VERSION_MAJOR})

create_bazel_config(storage_client)

if (GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC)
    find_package(gRPC REQUIRED)
    # We need to disable clang tidy for the generated CC files.
    set(saved_tidy "${CMAKE_CXX_CLANG_TIDY}")
    set(CMAKE_CXX_CLANG_TIDY "")
    google_cloud_cpp_proto_library(
        google_cloud_cpp_storage_protos
        internal/grpc_resumable_upload_session_url.proto PROTO_PATH_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/internal LOCAL_INCLUDE)
    set(CMAKE_CXX_CLANG_TIDY "${saved_tidy}")
    add_library(
        storage_client_grpc
        grpc_plugin.cc
        grpc_plugin.h
        internal/grpc_client.cc
        internal/grpc_client.h
        internal/grpc_object_read_source.cc
        internal/grpc_object_read_source.h
        internal/grpc_resumable_upload_session.cc
        internal/grpc_resumable_upload_session.h
        internal/grpc_resumable_upload_session_url.cc
        internal/grpc_resumable_upload_session_url.h
        internal/hybrid_client.cc
        internal/hybrid_client.h)
    target_link_libraries(
        storage_client_grpc
        PUBLIC storage_client
               google_cloud_cpp_grpc_utils
               google_cloud_cpp_common
               google_cloud_cpp_storage_protos
               nlohmann_json::nlohmann_json
               googleapis-c++::storage_protos
               gRPC::grpc++
               protobuf::libprotobuf
               absl::strings
               Crc32c::crc32c
               CURL::libcurl
               Threads::Threads
               OpenSSL::SSL
               OpenSSL::Crypto
               ZLIB::ZLIB)
    google_cloud_cpp_add_common_options(storage_client_grpc)
    target_include_directories(
        storage_client_grpc PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                   $<INSTALL_INTERFACE:include>)
    target_compile_options(storage_client_grpc
                           PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
    target_compile_definitions(storage_client_grpc
                               PUBLIC GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC)
    set_target_properties(
        storage_client_grpc
        PROPERTIES
            VERSION
            ${GOOGLE_CLOUD_CPP_VERSION_MAJOR}.${GOOGLE_CLOUD_CPP_VERSION_MINOR}.${GOOGLE_CLOUD_CPP_VERSION_PATCH}
            SOVERSION ${GOOGLE_CLOUD_CPP_VERSION_MAJOR})

    create_bazel_config(storage_client_grpc)
else ()
    add_library(storage_client_grpc INTERFACE)
endif (GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC)

if (BUILD_TESTING)
    add_library(
        storage_client_testing # cmake-format: sort
        testing/canonical_errors.h
        testing/mock_client.h
        testing/mock_fake_clock.cc
        testing/mock_fake_clock.h
        testing/mock_http_request.cc
        testing/mock_http_request.h
        testing/object_integration_test.cc
        testing/object_integration_test.h
        testing/random_names.cc
        testing/random_names.h
        testing/remove_stale_buckets.cc
        testing/remove_stale_buckets.h
        testing/retry_tests.h
        testing/storage_integration_test.cc
        testing/storage_integration_test.h
        testing/temp_file.cc
        testing/temp_file.h)
    target_link_libraries(
        storage_client_testing
        PUBLIC absl::memory
               storage_client_grpc
               storage_client
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
        bucket_access_control_test.cc
        bucket_metadata_test.cc
        bucket_test.cc
        client_bucket_acl_test.cc
        client_default_object_acl_test.cc
        client_notifications_test.cc
        client_object_acl_test.cc
        client_object_copy_test.cc
        client_options_test.cc
        client_service_account_test.cc
        client_sign_policy_document_test.cc
        client_sign_url_test.cc
        client_test.cc
        client_write_object_test.cc
        hashing_options_test.cc
        hmac_key_metadata_test.cc
        idempotency_policy_test.cc
        internal/access_control_common_parser_test.cc
        internal/access_control_common_test.cc
        internal/binary_data_as_debug_string_test.cc
        internal/bucket_acl_requests_test.cc
        internal/bucket_requests_test.cc
        internal/complex_option_test.cc
        internal/compute_engine_util_test.cc
        internal/const_buffer_test.cc
        internal/curl_client_test.cc
        internal/curl_handle_factory_test.cc
        internal/curl_handle_test.cc
        internal/curl_resumable_upload_session_test.cc
        internal/curl_wrappers_disable_sigpipe_handler_test.cc
        internal/curl_wrappers_enable_sigpipe_handler_test.cc
        internal/curl_wrappers_locking_already_present_test.cc
        internal/curl_wrappers_locking_disabled_test.cc
        internal/curl_wrappers_locking_enabled_test.cc
        internal/curl_wrappers_test.cc
        internal/default_object_acl_requests_test.cc
        internal/generate_message_boundary_test.cc
        internal/generic_request_test.cc
        internal/hash_validator_test.cc
        internal/hmac_key_requests_test.cc
        internal/http_response_test.cc
        internal/logging_client_test.cc
        internal/logging_resumable_upload_session_test.cc
        internal/metadata_parser_test.cc
        internal/notification_requests_test.cc
        internal/object_acl_requests_test.cc
        internal/object_requests_test.cc
        internal/object_streambuf_test.cc
        internal/openssl_util_test.cc
        internal/parameter_pack_validation_test.cc
        internal/patch_builder_test.cc
        internal/policy_document_request_test.cc
        internal/resumable_upload_session_test.cc
        internal/retry_client_test.cc
        internal/retry_object_read_source_test.cc
        internal/retry_resumable_upload_session_test.cc
        internal/service_account_requests_test.cc
        internal/sha256_hash_test.cc
        internal/sign_blob_requests_test.cc
        internal/signed_url_requests_test.cc
        internal/tuple_filter_test.cc
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
        object_stream_test.cc
        object_test.cc
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
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        target_link_libraries(
            ${target}
            PRIVATE absl::memory
                    storage_client_testing
                    google_cloud_cpp_testing
                    storage_client
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest
                    CURL::libcurl
                    nlohmann_json::nlohmann_json)
        google_cloud_cpp_add_common_options(${target})
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()

    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("storage_client_unit_tests.bzl"
                         "storage_client_unit_tests")

    add_subdirectory(tests)
endif ()

if (GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC)
    if (BUILD_TESTING)
        set(storage_client_grpc_unit_tests
            # cmake-format: sort
            internal/grpc_client_bucket_metadata_test.cc
            internal/grpc_client_bucket_request_test.cc
            internal/grpc_client_failures_test.cc
            internal/grpc_client_object_request_test.cc
            internal/grpc_client_test.cc
            internal/grpc_object_read_source_test.cc
            internal/grpc_resumable_upload_session_test.cc
            internal/grpc_resumable_upload_session_url_test.cc)

        foreach (fname ${storage_client_grpc_unit_tests})
            google_cloud_cpp_add_executable(target "storage" "${fname}")
            target_link_libraries(
                ${target}
                PRIVATE storage_client_grpc
                        storage_client_testing
                        google_cloud_cpp_testing
                        google_cloud_cpp_testing_grpc
                        storage_client
                        GTest::gmock_main
                        GTest::gmock
                        GTest::gtest
                        CURL::libcurl
                        nlohmann_json::nlohmann_json)
            google_cloud_cpp_add_common_options(${target})
            if (MSVC)
                target_compile_options(${target} PRIVATE "/bigobj")
            endif ()
            add_test(NAME ${target} COMMAND ${target})
        endforeach ()

        # Export the list of unit tests so the Bazel BUILD file can pick it up.
        export_list_to_bazel("storage_client_grpc_unit_tests.bzl"
                             "storage_client_grpc_unit_tests")
    endif ()
endif (GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC)

add_subdirectory(benchmarks)

if (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    # The examples are more readable if we use exceptions for error handling. We
    # had to tradeoff readability vs. "making them compile everywhere".
    add_subdirectory(examples)
endif (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)

# Export the CMake targets to make it easy to create configuration files.
install(EXPORT storage-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/storage_client")

install(
    TARGETS storage_client
    EXPORT storage-targets
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
    TARGETS storage_client
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers(storage_client include/google/cloud/storage)

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google Cloud Storage C++ Client Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Storage.")
set(GOOGLE_CLOUD_CPP_PC_REQUIRES "google_cloud_cpp_common libcurl openssl")
google_cloud_cpp_absl_pkg_config(absl_pkg_config storage_client)
string(CONCAT GOOGLE_CLOUD_CPP_PC_LIBS "-lstorage_client" " -lcrc32c"
              ${absl_pkg_config})

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
               "storage_client.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/storage_client.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "storage_client-config.cmake" @ONLY)
write_basic_package_version_file(
    "storage_client-config-version.cmake"
    VERSION ${GOOGLE_CLOUD_CPP_VERSION}
    COMPATIBILITY ExactVersion)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/storage_client-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/storage_client-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/storage_client")

install(FILES testing/mock_client.h
        DESTINATION include/google/cloud/storage/testing)

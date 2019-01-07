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

# This should be included from the top level CMakeLists file
set(STORAGE_CLIENT_VERSION_MAJOR 0)
set(STORAGE_CLIENT_VERSION_MINOR 4)
set(STORAGE_CLIENT_VERSION_PATCH 0)

set(DOXYGEN_PROJECT_NAME "Google Cloud Storage C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Storage")
set(
    DOXYGEN_PROJECT_NUMBER
    "${STORAGE_CLIENT_VERSION_MAJOR}.${STORAGE_CLIENT_VERSION_MINOR}.${STORAGE_CLIENT_VERSION_PATCH}"
    )
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/examples)
set(DOXYGEN_PREDEFINED "STORAGE_CLIENT_NS=v${STORAGE_CLIENT_VERSION_MAJOR}")
set(DOXYGEN_EXCLUDE_PATTERNS
    "*/google/cloud/storage/README.md"
    "*/google/cloud/storage/ci/*"
    "*/google/cloud/storage/examples/*.md"
    "*/google/cloud/storage/internal/*"
    "*/google/cloud/storage/testing/*"
    "*/google/cloud/storage/tests/*"
    "*/google/cloud/storage/*_test.cc")

include(GoogleCloudCppCommon)
include(IncludeNlohmannJson)
include(IncludeCrc32c)

# Generate the version information from the CMake values.
configure_file(version_info.h.in version_info.h)

# Export the version information for Bazel.
include(CreateBazelConfig)
export_variables_to_bazel("storage_client_version.bzl"
                          STORAGE_CLIENT_VERSION_MAJOR
                          STORAGE_CLIENT_VERSION_MINOR
                          STORAGE_CLIENT_VERSION_PATCH)

# Define an interface library, i.e., a library that really has no sources, and
# add public target options to it.  The targets then use the library via
# target_link_libraries(target PRIVATE storage_common_options), which implicitly
# adds the options to those targets, but does *not* add the options to any
# targets that depend on them.  This is a CMake idiom to add options to targets
# without having to painfully set special variables, it also allows users to add
# more common options without having to touch the code.
add_library(storage_common_options INTERFACE)
google_cloud_cpp_add_common_options(storage_common_options)

# Enable unit tests
include(CTest)

include(IncludeCurl)

# the client library
add_library(storage_client
            ${CMAKE_CURRENT_BINARY_DIR}/version_info.h
            bucket_access_control.h
            bucket_access_control.cc
            bucket_metadata.h
            bucket_metadata.cc
            client.h
            client.cc
            client_options.h
            client_options.cc
            download_options.h
            hashing_options.h
            hashing_options.cc
            idempotency_policy.h
            idempotency_policy.cc
            internal/access_control_common.h
            internal/access_control_common.cc
            internal/binary_data_as_debug_string.h
            internal/binary_data_as_debug_string.cc
            internal/bucket_acl_requests.h
            internal/bucket_acl_requests.cc
            internal/bucket_requests.h
            internal/bucket_requests.cc
            internal/complex_option.h
            internal/common_metadata.h
            internal/compute_engine_util.h
            internal/compute_engine_util.cc
            internal/curl_handle.h
            internal/curl_handle.cc
            internal/curl_handle_factory.h
            internal/curl_handle_factory.cc
            internal/curl_download_request.h
            internal/curl_download_request.cc
            internal/curl_request.h
            internal/curl_request.cc
            internal/curl_request_builder.h
            internal/curl_request_builder.cc
            internal/curl_resumable_streambuf.h
            internal/curl_resumable_streambuf.cc
            internal/curl_upload_request.cc
            internal/curl_upload_request.h
            internal/curl_wrappers.h
            internal/curl_wrappers.cc
            internal/curl_client.h
            internal/curl_client.cc
            internal/curl_resumable_upload_session.h
            internal/curl_resumable_upload_session.cc
            internal/curl_streambuf.h
            internal/curl_streambuf.cc
            internal/default_object_acl_requests.h
            internal/default_object_acl_requests.cc
            internal/empty_response.h
            internal/empty_response.cc
            internal/format_rfc3339.h
            internal/format_rfc3339.cc
            internal/generate_message_boundary.h
            internal/generic_object_request.h
            internal/generic_request.h
            internal/hash_validator.h
            internal/hash_validator.cc
            internal/http_response.h
            internal/http_response.cc
            internal/logging_client.h
            internal/logging_client.cc
            internal/logging_resumable_upload_session.h
            internal/logging_resumable_upload_session.cc
            internal/metadata_parser.h
            internal/metadata_parser.cc
            internal/nljson.h
            internal/notification_requests.h
            internal/notification_requests.cc
            internal/openssl_util.h
            internal/openssl_util.cc
            internal/object_acl_requests.h
            internal/object_acl_requests.cc
            internal/object_requests.h
            internal/object_requests.cc
            internal/object_streambuf.h
            internal/object_streambuf.cc
            internal/parse_rfc3339.h
            internal/parse_rfc3339.cc
            internal/patch_builder.h
            internal/raw_client.h
            internal/raw_client_wrapper_utils.h
            internal/resumable_upload_session.h
            internal/retry_client.h
            internal/retry_client.cc
            internal/retry_resumable_upload_session.h
            internal/retry_resumable_upload_session.cc
            internal/service_account_requests.h
            internal/service_account_requests.cc
            internal/signed_url_requests.h
            internal/signed_url_requests.cc
            lifecycle_rule.h
            lifecycle_rule.cc
            list_buckets_reader.h
            list_buckets_reader.cc
            list_objects_reader.h
            list_objects_reader.cc
            notification_event_type.h
            notification_metadata.h
            notification_metadata.cc
            notification_payload_format.h
            oauth2/anonymous_credentials.h
            oauth2/anonymous_credentials.cc
            oauth2/authorized_user_credentials.h
            oauth2/authorized_user_credentials.cc
            oauth2/compute_engine_credentials.h
            oauth2/credential_constants.h
            oauth2/credentials.h
            oauth2/google_application_default_credentials_file.h
            oauth2/google_application_default_credentials_file.cc
            oauth2/google_credentials.h
            oauth2/google_credentials.cc
            oauth2/refreshing_credentials_wrapper.h
            oauth2/refreshing_credentials_wrapper.cc
            oauth2/service_account_credentials.h
            oauth2/service_account_credentials.cc
            object_access_control.h
            object_access_control.cc
            object_metadata.h
            object_metadata.cc
            object_rewriter.h
            object_rewriter.cc
            object_stream.h
            object_stream.cc
            retry_policy.h
            service_account.h
            service_account.cc
            signed_url_options.h
            signed_url_options.cc
            storage_class.h
            upload_options.h
            version.h
            version.cc
            well_known_headers.h
            well_known_headers.cc
            well_known_parameters.h)
target_link_libraries(storage_client
                      PUBLIC google_cloud_cpp_common
                             nlohmann_json
                             Crc32c::crc32c
                             CURL::CURL
                             Threads::Threads
                             OpenSSL::SSL
                             OpenSSL::Crypto
                             ZLIB::ZLIB
                      PRIVATE storage_common_options)
target_include_directories(storage_client
                           PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                                  $<INSTALL_INTERFACE:include>)
target_compile_options(storage_client
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
if (MSVC)
    # MSVC warns about using sscanf(), this is a valuable warning, so we do not
    # want to disable for everything. But for these files the usage is "safe".
    # This is a relatively obscure feature to add compile flags to just one
    # source:
    set_property(SOURCE internal/parse_rfc3339.cc
                 APPEND_STRING
                 PROPERTY COMPILE_DEFINITIONS "_CRT_SECURE_NO_WARNINGS")
    set_property(SOURCE internal/object_requests.cc
                 APPEND_STRING
                 PROPERTY COMPILE_DEFINITIONS "_CRT_SECURE_NO_WARNINGS")
endif ()

# GCC-7.3 (the default GCC version on Ubuntu:18.04) issues a warning (a member
# variable may be used without being initialized), in this file. GCC-8.0 no
# longers emits that diagnostic, and neither does Clang. On the assumption that
# this is a spurious warning we disable it for older versions of GCC. I (coryan)
# did not research in exactly what version was this warning introduced, and when
# it was fixed. I do not believe we need to be that accurate.
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU"
    AND ${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 8.0)
    set_property(SOURCE list_objects_reader.cc
                 APPEND_STRING
                 PROPERTY COMPILE_FLAGS "-Wno-maybe-uninitialized")
endif ()

set_target_properties(
    storage_client
    PROPERTIES
        VERSION
        ${STORAGE_CLIENT_VERSION_MAJOR}.${STORAGE_CLIENT_VERSION_MINOR}.${STORAGE_CLIENT_VERSION_PATCH}
        SOVERSION
        ${STORAGE_CLIENT_VERSION_MAJOR})

create_bazel_config(storage_client)
google_cloud_cpp_add_clang_tidy(storage_client)

if (BUILD_TESTING)
    add_library(storage_client_testing
                testing/canonical_errors.h
                testing/mock_client.h
                testing/mock_http_request.h
                testing/mock_http_request.cc
                testing/retry_tests.h
                testing/storage_integration_test.h
                testing/storage_integration_test.cc)
    target_link_libraries(storage_client_testing
                          PUBLIC storage_client
                                 nlohmann_json
                                 GTest::gmock_main
                                 GTest::gmock
                                 GTest::gtest
                          PRIVATE storage_common_options)
    target_include_directories(storage_client_testing
                               PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                      $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                                      $<INSTALL_INTERFACE:include>)
    target_compile_options(storage_client_testing
                           PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

    create_bazel_config(storage_client_testing)

    # List the unit tests, then setup the targets and dependencies.
    set(storage_client_unit_tests
        bucket_access_control_test.cc
        bucket_metadata_test.cc
        bucket_test.cc
        client_bucket_acl_test.cc
        client_default_object_acl_test.cc
        client_object_acl_test.cc
        client_object_copy_test.cc
        client_service_account_test.cc
        client_notifications_test.cc
        client_sign_url_test.cc
        client_test.cc
        client_write_object_test.cc
        hashing_options_test.cc
        idempotency_policy_test.cc
        internal/access_control_common_test.cc
        internal/binary_data_as_debug_string_test.cc
        internal/bucket_acl_requests_test.cc
        internal/bucket_requests_test.cc
        internal/compute_engine_util_test.cc
        internal/curl_client_test.cc
        internal/curl_resumable_upload_session_test.cc
        internal/curl_wrappers_locking_already_present_test.cc
        internal/curl_wrappers_locking_enabled_test.cc
        internal/curl_wrappers_locking_disabled_test.cc
        internal/default_object_acl_requests_test.cc
        internal/format_rfc3339_test.cc
        internal/generate_message_boundary_test.cc
        internal/hash_validator_test.cc
        internal/http_response_test.cc
        internal/logging_client_test.cc
        internal/logging_resumable_upload_session_test.cc
        internal/metadata_parser_test.cc
        internal/nljson_test.cc
        internal/notification_requests_test.cc
        internal/object_acl_requests_test.cc
        internal/object_requests_test.cc
        internal/parse_rfc3339_test.cc
        internal/patch_builder_test.cc
        internal/retry_client_test.cc
        internal/retry_resumable_upload_session_test.cc
        internal/service_account_requests_test.cc
        internal/signed_url_requests_test.cc
        lifecycle_rule_test.cc
        list_buckets_reader_test.cc
        list_objects_reader_test.cc
        oauth2/anonymous_credentials_test.cc
        oauth2/authorized_user_credentials_test.cc
        oauth2/compute_engine_credentials_test.cc
        oauth2/google_application_default_credentials_file_test.cc
        oauth2/google_credentials_test.cc
        oauth2/service_account_credentials_test.cc
        object_access_control_test.cc
        object_metadata_test.cc
        object_test.cc
        notification_metadata_test.cc
        retry_policy_test.cc
        service_account_test.cc
        signed_url_options_test.cc
        storage_class_test.cc
        storage_client_options_test.cc
        storage_version_test.cc
        well_known_headers_test.cc)

    foreach (fname ${storage_client_unit_tests})
        string(REPLACE "/"
                       "_"
                       target
                       ${fname})
        string(REPLACE ".cc"
                       ""
                       target
                       ${target})
        add_executable(${target} ${fname})
        if (BUILD_TESTING)
            target_link_libraries(${target}
                                  PRIVATE storage_client_testing
                                          google_cloud_cpp_testing
                                          storage_client
                                          GTest::gmock_main
                                          GTest::gmock
                                          GTest::gtest
                                          CURL::CURL
                                          storage_common_options
                                          nlohmann_json)
            add_test(NAME ${target} COMMAND ${target})
        endif ()
    endforeach ()

    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("storage_client_unit_tests.bzl"
                         "storage_client_unit_tests")

    add_subdirectory(tests)
endif ()

if (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_subdirectory(benchmarks)
    add_subdirectory(examples)
endif (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(TARGETS storage_common_options nlohmann_json
        EXPORT storage-targets
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Export the CMake targets to make it easy to create configuration files.
install(EXPORT storage-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/storage_client")

install(TARGETS storage_client
        EXPORT storage-targets
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
google_cloud_cpp_install_headers(storage_client include/google/cloud/storage)
install(
    FILES
        ${CMAKE_BINARY_DIR}/external/nlohmann_json/include/google/cloud/storage/internal/nlohmann_json.hpp
        DESTINATION include/google/cloud/storage/internal)

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MAJOR ${STORAGE_CLIENT_VERSION_MAJOR})
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_MINOR ${STORAGE_CLIENT_VERSION_MINOR})
set(GOOGLE_CLOUD_CPP_CONFIG_VERSION_PATCH ${STORAGE_CLIENT_VERSION_PATCH})
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google Cloud Storage C++ Client Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Storage.")
set(GOOGLE_CLOUD_CPP_PC_LIBS "-lstorage_client")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
               "storage_client.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/storage_client.pc" DESTINATION
              "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Create and install the CMake configuration files.
configure_file("config.cmake.in" "storage_client-config.cmake" @ONLY)
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config-version.cmake.in"
               "storage_client-config-version.cmake" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/storage_client-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/storage_client-config-version.cmake"
              DESTINATION
              "${CMAKE_INSTALL_LIBDIR}/cmake/storage_client")

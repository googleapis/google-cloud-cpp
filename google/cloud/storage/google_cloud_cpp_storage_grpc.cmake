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

if (NOT GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC)
    # If disabled, defined an empty library so the tests can have a simpler
    # link-line
    add_library(google_cloud_cpp_storage_grpc INTERFACE)
    add_library(google-cloud-cpp::experimental-storage-grpc ALIAS
                google_cloud_cpp_storage_grpc)
    set_target_properties(
        google_cloud_cpp_storage_grpc
        PROPERTIES EXPORT_NAME "google-cloud-cpp::experimental-storage-grpc")
    if (GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND)
        target_compile_definitions(
            google_cloud_cpp_storage_grpc
            INTERFACE GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND)
    endif ()
else ()
    add_library(
        google_cloud_cpp_storage_grpc # cmake-format: sort
        async_client.cc
        async_client.h
        async_object_responses.h
        grpc_plugin.cc
        grpc_plugin.h
        internal/async_accumulate_read_object.cc
        internal/async_accumulate_read_object.h
        internal/async_connection.h
        internal/async_connection_impl.cc
        internal/async_connection_impl.h
        internal/grpc_bucket_access_control_parser.cc
        internal/grpc_bucket_access_control_parser.h
        internal/grpc_bucket_metadata_parser.cc
        internal/grpc_bucket_metadata_parser.h
        internal/grpc_bucket_name.cc
        internal/grpc_bucket_name.h
        internal/grpc_bucket_request_parser.cc
        internal/grpc_bucket_request_parser.h
        internal/grpc_buffer_read_object_data.cc
        internal/grpc_buffer_read_object_data.h
        internal/grpc_channel_refresh.cc
        internal/grpc_channel_refresh.h
        internal/grpc_client.cc
        internal/grpc_client.h
        internal/grpc_configure_client_context.cc
        internal/grpc_configure_client_context.h
        internal/grpc_ctype_cord_workaround.h
        internal/grpc_hmac_key_metadata_parser.cc
        internal/grpc_hmac_key_metadata_parser.h
        internal/grpc_hmac_key_request_parser.cc
        internal/grpc_hmac_key_request_parser.h
        internal/grpc_notification_metadata_parser.cc
        internal/grpc_notification_metadata_parser.h
        internal/grpc_notification_request_parser.cc
        internal/grpc_notification_request_parser.h
        internal/grpc_object_access_control_parser.cc
        internal/grpc_object_access_control_parser.h
        internal/grpc_object_metadata_parser.cc
        internal/grpc_object_metadata_parser.h
        internal/grpc_object_read_source.cc
        internal/grpc_object_read_source.h
        internal/grpc_object_request_parser.cc
        internal/grpc_object_request_parser.h
        internal/grpc_owner_parser.cc
        internal/grpc_owner_parser.h
        internal/grpc_service_account_parser.cc
        internal/grpc_service_account_parser.h
        internal/grpc_sign_blob_request_parser.cc
        internal/grpc_sign_blob_request_parser.h
        internal/grpc_split_write_object_data.cc
        internal/grpc_split_write_object_data.h
        internal/grpc_synthetic_self_link.cc
        internal/grpc_synthetic_self_link.h
        internal/hybrid_client.cc
        internal/hybrid_client.h
        internal/storage_auth_decorator.cc
        internal/storage_auth_decorator.h
        internal/storage_logging_decorator.cc
        internal/storage_logging_decorator.h
        internal/storage_metadata_decorator.cc
        internal/storage_metadata_decorator.h
        internal/storage_round_robin_decorator.cc
        internal/storage_round_robin_decorator.h
        internal/storage_stub.cc
        internal/storage_stub.h
        internal/storage_stub_factory.cc
        internal/storage_stub_factory.h
        internal/storage_tracing_stub.cc
        internal/storage_tracing_stub.h)
    target_link_libraries(
        google_cloud_cpp_storage_grpc
        PUBLIC google-cloud-cpp::storage
               google-cloud-cpp::grpc_utils
               google-cloud-cpp::common
               google-cloud-cpp::storage_protos
               nlohmann_json::nlohmann_json
               gRPC::grpc++
               protobuf::libprotobuf
               absl::strings
               Crc32c::crc32c
               CURL::libcurl
               Threads::Threads
               OpenSSL::SSL
               OpenSSL::Crypto
               ZLIB::ZLIB)
    add_library(google-cloud-cpp::experimental-storage-grpc ALIAS
                google_cloud_cpp_storage_grpc)
    google_cloud_cpp_add_common_options(google_cloud_cpp_storage_grpc)
    target_include_directories(
        google_cloud_cpp_storage_grpc
        PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
               $<INSTALL_INTERFACE:include>)
    target_compile_options(google_cloud_cpp_storage_grpc
                           PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
    target_compile_definitions(google_cloud_cpp_storage_grpc
                               PUBLIC GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC)
    if (GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND)
        target_compile_definitions(
            google_cloud_cpp_storage_grpc
            PUBLIC GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND)
    endif ()
    set_target_properties(
        google_cloud_cpp_storage_grpc
        PROPERTIES EXPORT_NAME "google-cloud-cpp::experimental-storage-grpc"
                   VERSION ${PROJECT_VERSION}
                   SOVERSION ${PROJECT_VERSION_MAJOR})

    create_bazel_config(google_cloud_cpp_storage_grpc)
endif ()

google_cloud_cpp_add_pkgconfig(
    storage_grpc
    "The GCS (Google Cloud Storage) gRPC plugin"
    "An extension to the GCS C++ client library using gRPC for transport."
    "google_cloud_cpp_storage"
    "google_cloud_cpp_grpc_utils"
    "google_cloud_cpp_storage_protos"
    "google_cloud_cpp_rpc_status_protos"
    "google_cloud_cpp_rpc_error_details_protos"
    "google_cloud_cpp_common"
    "libcurl"
    "openssl")

install(
    TARGETS google_cloud_cpp_storage_grpc
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
    TARGETS google_cloud_cpp_storage_grpc
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers(google_cloud_cpp_storage_grpc
                                 include/google/cloud/storage)

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC)
    # This is a bit weird, we add an additional link library to
    # `storage_client_testing`
    target_link_libraries(storage_client_testing
                          PUBLIC google-cloud-cpp::experimental-storage-grpc)

    set(storage_client_grpc_unit_tests
        # cmake-format: sort
        grpc_plugin_test.cc
        internal/async_accumulate_read_object_test.cc
        internal/async_connection_impl_test.cc
        internal/grpc_bucket_access_control_parser_test.cc
        internal/grpc_bucket_metadata_parser_test.cc
        internal/grpc_bucket_name_test.cc
        internal/grpc_bucket_request_parser_test.cc
        internal/grpc_buffer_read_object_data_test.cc
        internal/grpc_client_acl_test.cc
        internal/grpc_client_failures_test.cc
        internal/grpc_client_insert_object_media_test.cc
        internal/grpc_client_read_object_test.cc
        internal/grpc_client_test.cc
        internal/grpc_client_upload_chunk_test.cc
        internal/grpc_configure_client_context_test.cc
        internal/grpc_hmac_key_metadata_parser_test.cc
        internal/grpc_hmac_key_request_parser_test.cc
        internal/grpc_notification_metadata_parser_test.cc
        internal/grpc_notification_request_parser_test.cc
        internal/grpc_object_access_control_parser_test.cc
        internal/grpc_object_metadata_parser_test.cc
        internal/grpc_object_read_source_test.cc
        internal/grpc_object_request_parser_test.cc
        internal/grpc_owner_parser_test.cc
        internal/grpc_service_account_parser_test.cc
        internal/grpc_sign_blob_request_parser_test.cc
        internal/grpc_split_write_object_data_test.cc
        internal/grpc_synthetic_self_link_test.cc
        internal/storage_stub_factory_test.cc)

    foreach (fname ${storage_client_grpc_unit_tests})
        google_cloud_cpp_add_executable(target "storage" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE storage_client_testing
                    google_cloud_cpp_testing
                    google_cloud_cpp_testing_grpc
                    google-cloud-cpp::experimental-storage-grpc
                    google-cloud-cpp::storage
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
                         "storage_client_grpc_unit_tests" YEAR "2018")
endif ()

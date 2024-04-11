# ~~~
# Copyright 2018 Google LLC
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

set(storage_client_integration_tests
    # cmake-format : sort
    async_client_integration_test.cc
    auto_finalize_integration_test.cc
    bucket_integration_test.cc
    create_client_integration_test.cc
    curl_sign_blob_integration_test.cc
    decompressive_transcoding_integration_test.cc
    error_injection_integration_test.cc
    error_parsing_integration_test.cc
    grpc_bucket_acl_integration_test.cc
    grpc_bucket_metadata_integration_test.cc
    grpc_default_object_acl_integration_test.cc
    grpc_hmac_key_integration_test.cc
    grpc_integration_test.cc
    grpc_notification_integration_test.cc
    grpc_object_acl_integration_test.cc
    grpc_object_media_integration_test.cc
    grpc_object_metadata_integration_test.cc
    grpc_service_account_integration_test.cc
    object_basic_crud_integration_test.cc
    object_checksum_integration_test.cc
    object_compose_many_integration_test.cc
    object_file_integration_test.cc
    object_file_multi_threaded_test.cc
    object_hash_integration_test.cc
    object_insert_integration_test.cc
    object_insert_preconditions_integration_test.cc
    object_integration_test.cc
    object_list_objects_versions_integration_test.cc
    object_media_integration_test.cc
    object_parallel_upload_integration_test.cc
    object_plenty_clients_serially_integration_test.cc
    object_plenty_clients_simultaneously_integration_test.cc
    object_read_headers_integration_test.cc
    object_read_large_integration_test.cc
    object_read_preconditions_integration_test.cc
    object_read_range_integration_test.cc
    object_read_stream_integration_test.cc
    object_resumable_parallel_upload_integration_test.cc
    object_resumable_write_integration_test.cc
    object_rewrite_integration_test.cc
    object_write_preconditions_integration_test.cc
    object_write_stream_integration_test.cc
    object_write_streambuf_integration_test.cc
    service_account_credentials_integration_test.cc
    service_account_integration_test.cc
    signed_url_conformance_test.cc
    slow_reader_chunk_integration_test.cc
    slow_reader_stream_integration_test.cc
    small_reads_integration_test.cc
    smoke_test_async.cc
    smoke_test_grpc.cc
    smoke_test_rest.cc
    storage_include_test.cc
    thread_integration_test.cc
    tracing_integration_test.cc)
set(storage_client_integration_tests_production
    # cmake-format: sort
    alternative_endpoint_integration_test.cc key_file_integration_test.cc
    signed_url_integration_test.cc unified_credentials_integration_test.cc)
list(APPEND storage_client_integration_tests
     ${storage_client_integration_tests_production})
list(SORT storage_client_integration_tests)

include(CreateBazelConfig)
export_list_to_bazel(
    "storage_client_integration_tests.bzl" "storage_client_integration_tests"
    "storage_client_integration_tests_production" YEAR "2018")

# Signed URL conformance tests are parsed using protos. In order to simplify the
# build files, only build these conformance tests if gRPC in GCS is compiled.
if (GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC)
    # We need to disable clang tidy for the generated CC files.
    google_cloud_cpp_proto_library(
        google_cloud_cpp_storage_tests_conformance_protos
        conformance_tests.proto PROTO_PATH_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR})
endif ()

foreach (fname IN LISTS storage_client_integration_tests)
    google_cloud_cpp_add_executable(target "storage" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE storage_client_testing
                google_cloud_cpp_testing
                google-cloud-cpp::storage
                google-cloud-cpp::common
                GTest::gmock_main
                GTest::gmock
                GTest::gtest
                Threads::Threads
                absl::strings
                nlohmann_json)
    if (GOOGLE_CLOUD_CPP_STORAGE_ENABLE_GRPC)
        target_link_libraries(
            ${target}
            PRIVATE google-cloud-cpp::storage google_cloud_cpp_testing_grpc
                    google_cloud_cpp_storage_tests_conformance_protos)
    endif ()

    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(
        ${target} PROPERTIES LABELS
                             "integration-test;integration-test-emulator")
    google_cloud_cpp_add_common_options(${target})
endforeach ()
foreach (target IN ITEMS storage_smoke_test_async storage_smoke_test_grpc
                         storage_smoke_test_rest)
    set_tests_properties(
        ${target}
        PROPERTIES
            LABELS
            "integration-test;integration-test-emulator;integration-test-gha")
endforeach ()

# We just know that these tests need to be run against production.
foreach (fname IN LISTS storage_client_integration_tests_production)
    google_cloud_cpp_set_target_name(target "storage" "${fname}")
    set_tests_properties(
        ${target} PROPERTIES LABELS
                             "integration-test;integration-test-production")
endforeach ()

target_link_libraries(storage_error_injection_integration_test
                      PRIVATE ${CMAKE_DL_LIBS})

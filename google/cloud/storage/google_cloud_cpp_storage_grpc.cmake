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
    add_library(google-cloud-cpp::storage_grpc ALIAS
                google_cloud_cpp_storage_grpc)
    add_library(google_cloud_cpp_storage_protos INTERFACE)
    add_library(google-cloud-cpp::storage_protos ALIAS
                google_cloud_cpp_storage_protos)

    option(
        GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND
        [==[Unused, as GCS+gRPC is not enabled.

    More details at

    https://github.com/googleapis/google-cloud-cpp/blob/main/doc/ctype-cord-workarounds.md
    ]==]
        OFF)
    mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND)
    return()
endif ()

include(GoogleCloudCppLibrary)
google_cloud_cpp_add_library_protos(storage)

# Find out if `opentelemetry-cpp` is available.
find_package(opentelemetry-cpp CONFIG)

set(GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND_DEFAULT ON)
# Protobuf versions are ... complicated.  Protobuf used to call itself 3.21.*,
# 3.20.*, 3.19.*, etc. Then it starts calling itself 22.*, 23.*, etc. This may
# change in the future:
#
# https://github.com/protocolbuffers/protobuf/issues/13103
#
# Guessing the new version scheme is tempting, but probably unwise.
#
# In any case, `ctype=CORD` is supported starting in 23.x.
if (Protobuf_VERSION VERSION_GREATER_EQUAL "23")
    set(GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND_DEFAULT OFF)
endif ()
option(
    GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND
    [==[Enable the workarounds for [ctype = CORD] when using Protobuf < v23.
    More details at

    https://github.com/googleapis/google-cloud-cpp/blob/main/doc/ctype-cord-workarounds.md
    ]==]
    ${GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND_DEFAULT})
mark_as_advanced(GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND)

add_library(
    google_cloud_cpp_storage_grpc # cmake-format: sort
    async/bucket_name.cc
    async/bucket_name.h
    async/client.cc
    async/client.h
    async/connection.h
    async/idempotency_policy.cc
    async/idempotency_policy.h
    async/object_descriptor.cc
    async/object_descriptor.h
    async/object_descriptor_connection.h
    async/object_responses.cc
    async/object_responses.h
    async/options.h
    async/read_all.cc
    async/read_all.h
    async/reader.cc
    async/reader.h
    async/reader_connection.h
    async/resume_policy.cc
    async/resume_policy.h
    async/retry_policy.h
    async/rewriter.cc
    async/rewriter.h
    async/rewriter_connection.h
    async/token.h
    async/write_payload.h
    async/writer.cc
    async/writer.h
    async/writer_connection.h
    grpc_plugin.cc
    grpc_plugin.h
    internal/async/connection_fwd.h
    internal/async/connection_impl.cc
    internal/async/connection_impl.h
    internal/async/connection_tracing.cc
    internal/async/connection_tracing.h
    internal/async/default_options.cc
    internal/async/default_options.h
    internal/async/handle_redirect_error.cc
    internal/async/handle_redirect_error.h
    internal/async/insert_object.cc
    internal/async/insert_object.h
    internal/async/object_descriptor_connection_tracing.cc
    internal/async/object_descriptor_connection_tracing.h
    internal/async/object_descriptor_impl.cc
    internal/async/object_descriptor_impl.h
    internal/async/object_descriptor_reader.cc
    internal/async/object_descriptor_reader.h
    internal/async/object_descriptor_reader_tracing.cc
    internal/async/object_descriptor_reader_tracing.h
    internal/async/open_object.cc
    internal/async/open_object.h
    internal/async/open_stream.cc
    internal/async/open_stream.h
    internal/async/partial_upload.cc
    internal/async/partial_upload.h
    internal/async/read_payload_fwd.h
    internal/async/read_payload_impl.h
    internal/async/read_range.cc
    internal/async/read_range.h
    internal/async/reader_connection_factory.cc
    internal/async/reader_connection_factory.h
    internal/async/reader_connection_impl.cc
    internal/async/reader_connection_impl.h
    internal/async/reader_connection_resume.cc
    internal/async/reader_connection_resume.h
    internal/async/reader_connection_tracing.cc
    internal/async/reader_connection_tracing.h
    internal/async/rewriter_connection_impl.cc
    internal/async/rewriter_connection_impl.h
    internal/async/rewriter_connection_tracing.cc
    internal/async/rewriter_connection_tracing.h
    internal/async/token_impl.cc
    internal/async/token_impl.h
    internal/async/write_object.cc
    internal/async/write_object.h
    internal/async/write_payload_fwd.h
    internal/async/write_payload_impl.h
    internal/async/writer_connection_buffered.cc
    internal/async/writer_connection_buffered.h
    internal/async/writer_connection_finalized.cc
    internal/async/writer_connection_finalized.h
    internal/async/writer_connection_impl.cc
    internal/async/writer_connection_impl.h
    internal/async/writer_connection_resumed.cc
    internal/async/writer_connection_resumed.h
    internal/async/writer_connection_tracing.cc
    internal/async/writer_connection_tracing.h
    internal/grpc/bucket_access_control_parser.cc
    internal/grpc/bucket_access_control_parser.h
    internal/grpc/bucket_metadata_parser.cc
    internal/grpc/bucket_metadata_parser.h
    internal/grpc/bucket_name.cc
    internal/grpc/bucket_name.h
    internal/grpc/bucket_request_parser.cc
    internal/grpc/bucket_request_parser.h
    internal/grpc/buffer_read_object_data.cc
    internal/grpc/buffer_read_object_data.h
    internal/grpc/channel_refresh.cc
    internal/grpc/channel_refresh.h
    internal/grpc/configure_client_context.cc
    internal/grpc/configure_client_context.h
    internal/grpc/ctype_cord_workaround.h
    internal/grpc/default_options.cc
    internal/grpc/default_options.h
    internal/grpc/enable_metrics.cc
    internal/grpc/enable_metrics.h
    internal/grpc/make_cord.cc
    internal/grpc/make_cord.h
    internal/grpc/metrics_exporter_impl.cc
    internal/grpc/metrics_exporter_impl.h
    internal/grpc/metrics_exporter_options.cc
    internal/grpc/metrics_exporter_options.h
    internal/grpc/metrics_histograms.cc
    internal/grpc/metrics_histograms.h
    internal/grpc/metrics_meter_provider.cc
    internal/grpc/metrics_meter_provider.h
    internal/grpc/monitoring_project.cc
    internal/grpc/monitoring_project.h
    internal/grpc/object_access_control_parser.cc
    internal/grpc/object_access_control_parser.h
    internal/grpc/object_metadata_parser.cc
    internal/grpc/object_metadata_parser.h
    internal/grpc/object_read_source.cc
    internal/grpc/object_read_source.h
    internal/grpc/object_request_parser.cc
    internal/grpc/object_request_parser.h
    internal/grpc/owner_parser.cc
    internal/grpc/owner_parser.h
    internal/grpc/scale_stall_timeout.cc
    internal/grpc/scale_stall_timeout.h
    internal/grpc/sign_blob_request_parser.cc
    internal/grpc/sign_blob_request_parser.h
    internal/grpc/split_write_object_data.cc
    internal/grpc/split_write_object_data.h
    internal/grpc/stub.cc
    internal/grpc/stub.h
    internal/grpc/synthetic_self_link.cc
    internal/grpc/synthetic_self_link.h
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
           google-cloud-cpp::storage_protos
           google-cloud-cpp::grpc_utils
           google-cloud-cpp::common
           nlohmann_json::nlohmann_json
           gRPC::grpc++
           absl::optional
           absl::strings
           absl::time
           Threads::Threads)
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
        PRIVATE GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND)
endif ()
if ((TARGET gRPC::grpcpp_otel_plugin)
    AND (TARGET google-cloud-cpp::opentelemetry)
    AND (TARGET opentelemetry-cpp::metrics))
    target_compile_definitions(
        google_cloud_cpp_storage_grpc
        PRIVATE GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS)
    target_link_libraries(
        google_cloud_cpp_storage_grpc
        PUBLIC google-cloud-cpp::opentelemetry gRPC::grpcpp_otel_plugin
               opentelemetry-cpp::metrics)
    set(EXTRA_MODULES "google_cloud_cpp_opentelemetry" "grpcpp_otel_plugin"
                      "opentelemetry_metrics")
endif ()
set_target_properties(
    google_cloud_cpp_storage_grpc
    PROPERTIES EXPORT_NAME "google-cloud-cpp::storage_grpc"
               VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR})

create_bazel_config(google_cloud_cpp_storage_grpc)

add_library(google-cloud-cpp::storage_grpc ALIAS google_cloud_cpp_storage_grpc)

# TODO(#13857) - remove the backwards compatibility shims
add_library(google_cloud_cpp_experimental_storage_grpc INTERFACE)
set_target_properties(
    google_cloud_cpp_experimental_storage_grpc
    PROPERTIES EXPORT_NAME "google-cloud-cpp::experimental-storage_grpc")
target_link_libraries(google_cloud_cpp_experimental_storage_grpc
                      INTERFACE google-cloud-cpp::storage_grpc)
add_library(google-cloud-cpp::experimental-storage_grpc ALIAS
            google_cloud_cpp_experimental_storage_grpc)

google_cloud_cpp_add_pkgconfig(
    storage_grpc
    "The GCS (Google Cloud Storage) gRPC plugin"
    "An extension to the GCS C++ client library using gRPC for transport."
    "google_cloud_cpp_storage"
    "google_cloud_cpp_storage_protos"
    "google_cloud_cpp_grpc_utils"
    "google_cloud_cpp_common"
    "grpc++"
    "absl_optional"
    "absl_strings"
    "absl_time"
    ${EXTRA_MODULES})

install(
    EXPORT storage_grpc-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_storage_grpc"
    COMPONENT google_cloud_cpp_development)

# Create and install the CMake configuration files.
configure_file("config-grpc.cmake.in"
               "google_cloud_cpp_storage_grpc-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_storage_grpc-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_storage_grpc-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_storage_grpc-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_storage_grpc"
    COMPONENT google_cloud_cpp_development)

install(
    TARGETS google_cloud_cpp_storage_grpc google_cloud_cpp_storage_protos
            # TODO(#13857) - remove the backwards compatibility shims
            google_cloud_cpp_experimental_storage_grpc
    EXPORT storage_grpc-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_COMPONENT google_cloud_cpp_development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers(google_cloud_cpp_storage_grpc
                                 include/google/cloud/storage)

if (GOOGLE_CLOUD_CPP_WITH_MOCKS)
    # Create a header-only library for the mocks. We use a CMake `INTERFACE`
    # library for these, a regular library would not work on macOS (where the
    # library needs at least one .o file). Unfortunately INTERFACE libraries are
    # a bit weird in that they need absolute paths for their sources.
    add_library(google_cloud_cpp_storage_grpc_mocks INTERFACE)
    set(google_cloud_cpp_storage_grpc_mocks_hdrs
        # cmake-format: sort
        mocks/mock_async_connection.h
        mocks/mock_async_object_descriptor_connection.h
        mocks/mock_async_reader_connection.h
        mocks/mock_async_rewriter_connection.h
        mocks/mock_async_writer_connection.h)
    export_list_to_bazel("google_cloud_cpp_storage_grpc_mocks.bzl"
                         "google_cloud_cpp_storage_grpc_mocks_hdrs" YEAR "2023")
    target_link_libraries(google_cloud_cpp_storage_grpc_mocks
                          INTERFACE google-cloud-cpp::storage_grpc GTest::gmock)
    set_target_properties(
        google_cloud_cpp_storage_grpc_mocks
        PROPERTIES EXPORT_NAME "google-cloud-cpp::storage_grpc_mocks")
    target_include_directories(
        google_cloud_cpp_storage_grpc_mocks
        INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
                  $<INSTALL_INTERFACE:include>)
    target_compile_options(google_cloud_cpp_storage_grpc_mocks
                           INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
    add_library(google-cloud-cpp::storage_grpc_mocks ALIAS
                google_cloud_cpp_storage_grpc_mocks)

    # TODO(#13857) - remove backwards compatibility shims
    add_library(google_cloud_cpp_experimental_storage_grpc_mocks INTERFACE)
    target_link_libraries(google_cloud_cpp_experimental_storage_grpc_mocks
                          INTERFACE google-cloud-cpp::storage_grpc_mocks)
    add_library(google-cloud-cpp::experimental-storage_grpc_mocks ALIAS
                google_cloud_cpp_experimental_storage_grpc_mocks)

    install(
        FILES ${google_cloud_cpp_storage_grpc_mocks_hdrs}
        DESTINATION "include/google/cloud/storage/mocks"
        COMPONENT google_cloud_cpp_development)

    install(
        EXPORT storage_grpc_mocks-targets
        DESTINATION
            "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_storage_grpc_mocks"
        COMPONENT google_cloud_cpp_development)

    install(
        TARGETS google_cloud_cpp_storage_grpc_mocks
                # TODO(#13857) - remove backwards compatibility shims
                google_cloud_cpp_experimental_storage_grpc_mocks
        EXPORT storage_grpc_mocks-targets
        COMPONENT google_cloud_cpp_development)

    google_cloud_cpp_add_pkgconfig(
        storage_grpc_mocks "Google Cloud Storage (gRPC) Mocks"
        "Mocks for the Google Cloud Storage (gRPC) C++ Client Library"
        "google_cloud_cpp_storage" " gmock")

    # Create and install the CMake configuration files.
    configure_file("mocks-config.cmake.in"
                   "google_cloud_cpp_storage_grpc_mocks-config.cmake" @ONLY)
    write_basic_package_version_file(
        "google_cloud_cpp_storage_grpc_mocks-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY ExactVersion)

    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_storage_grpc_mocks-config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_storage_grpc_mocks-config-version.cmake"
        DESTINATION
            "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_storage_grpc_mocks"
        COMPONENT google_cloud_cpp_development)
endif ()

if (NOT BUILD_TESTING)
    return()
endif ()

# This is a bit weird, we add an additional link library to
# `storage_client_testing`
target_link_libraries(storage_client_testing
                      PUBLIC google-cloud-cpp::storage_grpc)

set(storage_client_grpc_unit_tests
    # cmake-format: sort
    async/bucket_name_test.cc
    async/client_test.cc
    async/idempotency_policy_test.cc
    async/object_descriptor_test.cc
    async/read_all_test.cc
    async/reader_test.cc
    async/resume_policy_test.cc
    async/retry_policy_test.cc
    async/rewriter_test.cc
    async/token_test.cc
    async/writer_test.cc
    grpc_plugin_test.cc
    internal/async/connection_impl_appendable_upload_test.cc
    internal/async/connection_impl_insert_test.cc
    internal/async/connection_impl_open_test.cc
    internal/async/connection_impl_read_hash_test.cc
    internal/async/connection_impl_read_test.cc
    internal/async/connection_impl_test.cc
    internal/async/connection_impl_upload_hash_test.cc
    internal/async/connection_impl_upload_test.cc
    internal/async/connection_tracing_test.cc
    internal/async/default_options_test.cc
    internal/async/handle_redirect_error_test.cc
    internal/async/insert_object_test.cc
    internal/async/object_descriptor_connection_tracing_test.cc
    internal/async/object_descriptor_impl_test.cc
    internal/async/object_descriptor_reader_test.cc
    internal/async/object_descriptor_reader_tracing_test.cc
    internal/async/open_object_test.cc
    internal/async/open_stream_test.cc
    internal/async/partial_upload_test.cc
    internal/async/read_payload_impl_test.cc
    internal/async/read_range_test.cc
    internal/async/reader_connection_factory_test.cc
    internal/async/reader_connection_impl_test.cc
    internal/async/reader_connection_resume_test.cc
    internal/async/reader_connection_tracing_test.cc
    internal/async/rewriter_connection_impl_test.cc
    internal/async/rewriter_connection_tracing_test.cc
    internal/async/write_object_test.cc
    internal/async/write_payload_impl_test.cc
    internal/async/writer_connection_buffered_test.cc
    internal/async/writer_connection_finalized_test.cc
    internal/async/writer_connection_impl_test.cc
    internal/async/writer_connection_resumed_test.cc
    internal/async/writer_connection_tracing_test.cc
    internal/grpc/bucket_access_control_parser_test.cc
    internal/grpc/bucket_metadata_parser_test.cc
    internal/grpc/bucket_name_test.cc
    internal/grpc/bucket_request_parser_test.cc
    internal/grpc/buffer_read_object_data_test.cc
    internal/grpc/configure_client_context_test.cc
    internal/grpc/default_options_test.cc
    internal/grpc/make_cord_test.cc
    internal/grpc/metrics_exporter_impl_test.cc
    internal/grpc/metrics_exporter_options_test.cc
    internal/grpc/metrics_histograms_test.cc
    internal/grpc/metrics_meter_provider_test.cc
    internal/grpc/monitoring_project_test.cc
    internal/grpc/object_access_control_parser_test.cc
    internal/grpc/object_metadata_parser_test.cc
    internal/grpc/object_read_source_test.cc
    internal/grpc/object_request_parser_test.cc
    internal/grpc/owner_parser_test.cc
    internal/grpc/scale_stall_timeout_test.cc
    internal/grpc/sign_blob_request_parser_test.cc
    internal/grpc/split_write_object_data_test.cc
    internal/grpc/stub_acl_test.cc
    internal/grpc/stub_insert_object_media_test.cc
    internal/grpc/stub_read_object_test.cc
    internal/grpc/stub_test.cc
    internal/grpc/stub_upload_chunk_test.cc
    internal/grpc/synthetic_self_link_test.cc
    internal/storage_stub_factory_test.cc)

foreach (fname ${storage_client_grpc_unit_tests})
    google_cloud_cpp_add_executable(target "storage" "${fname}")
    if (GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND)
        target_compile_definitions(
            ${target} PRIVATE GOOGLE_CLOUD_CPP_ENABLE_CTYPE_CORD_WORKAROUND)
    endif ()
    target_link_libraries(
        ${target}
        PRIVATE storage_client_testing
                google_cloud_cpp_testing
                google_cloud_cpp_testing_grpc
                google-cloud-cpp::storage_grpc
                google-cloud-cpp::storage_grpc_mocks
                google-cloud-cpp::storage
                GTest::gmock_main
                GTest::gmock
                GTest::gtest
                CURL::libcurl
                nlohmann_json::nlohmann_json)
    google_cloud_cpp_add_common_options(${target})
    if ((TARGET gRPC::grpcpp_otel_plugin)
        AND (TARGET google-cloud-cpp::opentelemetry)
        AND (TARGET opentelemetry-cpp::metrics))
        target_compile_definitions(
            ${target} PRIVATE GOOGLE_CLOUD_CPP_STORAGE_WITH_OTEL_METRICS)
    endif ()
    add_test(NAME ${target} COMMAND ${target})
endforeach ()

# Export the list of unit tests so the Bazel BUILD file can pick it up.
export_list_to_bazel("storage_client_grpc_unit_tests.bzl"
                     "storage_client_grpc_unit_tests" YEAR "2018")

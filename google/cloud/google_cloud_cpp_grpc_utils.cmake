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

# the library
add_library(
    google_cloud_cpp_grpc_utils # cmake-format: sort
    async_operation.h
    async_streaming_read_write_rpc.h
    background_threads.h
    completion_queue.cc
    completion_queue.h
    connection_options.cc
    connection_options.h
    grpc_error_delegate.cc
    grpc_error_delegate.h
    grpc_options.cc
    grpc_options.h
    grpc_utils/async_operation.h
    grpc_utils/completion_queue.h
    grpc_utils/grpc_error_delegate.h
    grpc_utils/version.h
    iam_updater.h
    internal/async_connection_ready.cc
    internal/async_connection_ready.h
    internal/async_long_running_operation.h
    internal/async_polling_loop.cc
    internal/async_polling_loop.h
    internal/async_read_stream_impl.h
    internal/async_read_write_stream_auth.h
    internal/async_read_write_stream_impl.h
    internal/async_read_write_stream_logging.h
    internal/async_retry_loop.h
    internal/async_retry_unary_rpc.h
    internal/async_rpc_details.h
    internal/background_threads_impl.cc
    internal/background_threads_impl.h
    internal/completion_queue_impl.h
    internal/default_completion_queue_impl.cc
    internal/default_completion_queue_impl.h
    internal/extract_long_running_result.cc
    internal/extract_long_running_result.h
    internal/grpc_access_token_authentication.cc
    internal/grpc_access_token_authentication.h
    internal/grpc_async_access_token_cache.cc
    internal/grpc_async_access_token_cache.h
    internal/grpc_channel_credentials_authentication.cc
    internal/grpc_channel_credentials_authentication.h
    internal/grpc_impersonate_service_account.cc
    internal/grpc_impersonate_service_account.h
    internal/grpc_service_account_authentication.cc
    internal/grpc_service_account_authentication.h
    internal/log_wrapper.cc
    internal/log_wrapper.h
    internal/minimal_iam_credentials_stub.cc
    internal/minimal_iam_credentials_stub.h
    internal/populate_grpc_options.cc
    internal/populate_grpc_options.h
    internal/resumable_streaming_read_rpc.h
    internal/retry_loop.h
    internal/retry_loop_helpers.cc
    internal/retry_loop_helpers.h
    internal/setup_context.h
    internal/streaming_read_rpc.cc
    internal/streaming_read_rpc.h
    internal/streaming_read_rpc_logging.cc
    internal/streaming_read_rpc_logging.h
    internal/streaming_write_rpc.cc
    internal/streaming_write_rpc.h
    internal/streaming_write_rpc_logging.h
    internal/time_utils.cc
    internal/time_utils.h
    internal/unified_grpc_credentials.cc
    internal/unified_grpc_credentials.h)
target_link_libraries(
    google_cloud_cpp_grpc_utils
    PUBLIC absl::function_ref
           absl::memory
           absl::time
           absl::variant
           google-cloud-cpp::iam_protos
           google-cloud-cpp::longrunning_operations_protos
           google-cloud-cpp::rpc_error_details_protos
           google-cloud-cpp::rpc_status_protos
           google-cloud-cpp::common
           gRPC::grpc++
           gRPC::grpc)
google_cloud_cpp_add_common_options(google_cloud_cpp_grpc_utils)
target_include_directories(
    google_cloud_cpp_grpc_utils PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                       $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_grpc_utils
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(
    google_cloud_cpp_grpc_utils
    PROPERTIES EXPORT_NAME "google-cloud-cpp::grpc_utils"
               VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(google-cloud-cpp::grpc_utils ALIAS google_cloud_cpp_grpc_utils)

create_bazel_config(google_cloud_cpp_grpc_utils YEAR 2019)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS
    EXPORT grpc_utils-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT grpc_utils-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_grpc_utils"
    COMPONENT google_cloud_cpp_development)

install(
    TARGETS google_cloud_cpp_grpc_utils
    EXPORT grpc_utils-targets
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
    TARGETS google_cloud_cpp_grpc_utils
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers(google_cloud_cpp_grpc_utils
                                 include/google/cloud)

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CPP_PC_NAME
    "gRPC Utilities for the Google Cloud C++ Client Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Provides gRPC Utilities for the Google Cloud C++ Client Library.")
string(
    CONCAT GOOGLE_CLOUD_CPP_PC_REQUIRES
           "google_cloud_cpp_common"
           " google_cloud_cpp_iam_protos"
           " google_cloud_cpp_longrunning_operations_protos"
           " google_cloud_cpp_rpc_status_protos"
           " openssl")
string(CONCAT GOOGLE_CLOUD_CPP_PC_LIBS "-lgoogle_cloud_cpp_grpc_utils")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
               "google_cloud_cpp_grpc_utils.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_grpc_utils.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_development)

# Create and install the CMake configuration files.
configure_file("grpc_utils/config.cmake.in"
               "google_cloud_cpp_grpc_utils-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_grpc_utils-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_grpc_utils-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_grpc_utils-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_grpc_utils"
    COMPONENT google_cloud_cpp_development)

function (google_cloud_cpp_grpc_utils_add_test fname labels)
    google_cloud_cpp_add_executable(target "common_grpc_utils" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE google-cloud-cpp::grpc_utils
                google_cloud_cpp_testing_grpc
                google_cloud_cpp_testing
                google-cloud-cpp::common
                google-cloud-cpp::iam_protos
                google-cloud-cpp::bigtable_protos
                google-cloud-cpp::spanner_protos
                absl::variant
                GTest::gmock_main
                GTest::gmock
                GTest::gtest
                gRPC::grpc++
                gRPC::grpc)
    google_cloud_cpp_add_common_options(${target})
    if (MSVC)
        target_compile_options(${target} PRIVATE "/bigobj")
    endif ()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "${labels}")
endfunction ()

if (BUILD_TESTING)
    find_package(benchmark CONFIG REQUIRED)

    # List the unit tests, then setup the targets and dependencies.
    set(google_cloud_cpp_grpc_utils_unit_tests
        # cmake-format: sort
        completion_queue_test.cc
        connection_options_test.cc
        grpc_error_delegate_test.cc
        grpc_options_test.cc
        internal/async_connection_ready_test.cc
        internal/async_long_running_operation_test.cc
        internal/async_polling_loop_test.cc
        internal/async_read_write_stream_auth_test.cc
        internal/async_read_write_stream_impl_test.cc
        internal/async_read_write_stream_logging_test.cc
        internal/async_retry_loop_test.cc
        internal/async_retry_unary_rpc_test.cc
        internal/background_threads_impl_test.cc
        internal/extract_long_running_result_test.cc
        internal/grpc_access_token_authentication_test.cc
        internal/grpc_async_access_token_cache_test.cc
        internal/grpc_channel_credentials_authentication_test.cc
        internal/grpc_service_account_authentication_test.cc
        internal/log_wrapper_test.cc
        internal/minimal_iam_credentials_stub_test.cc
        internal/populate_grpc_options_test.cc
        internal/resumable_streaming_read_rpc_test.cc
        internal/retry_loop_test.cc
        internal/streaming_read_rpc_logging_test.cc
        internal/streaming_read_rpc_test.cc
        internal/streaming_write_rpc_logging_test.cc
        internal/streaming_write_rpc_test.cc
        internal/time_utils_test.cc
        internal/unified_grpc_credentials_test.cc)

    # List the unit tests, then setup the targets and dependencies.
    set(google_cloud_cpp_grpc_utils_integration_tests
        # cmake-format: sort
        internal/grpc_impersonate_service_account_integration_test.cc)

    # Export the list of unit and integration tests so the Bazel BUILD file can
    # pick them up.
    export_list_to_bazel("google_cloud_cpp_grpc_utils_unit_tests.bzl"
                         "google_cloud_cpp_grpc_utils_unit_tests" YEAR "2019")
    export_list_to_bazel(
        "google_cloud_cpp_grpc_utils_integration_tests.bzl"
        "google_cloud_cpp_grpc_utils_integration_tests" YEAR "2021")

    foreach (fname ${google_cloud_cpp_grpc_utils_unit_tests})
        google_cloud_cpp_grpc_utils_add_test("${fname}" "")
    endforeach ()

    foreach (fname ${google_cloud_cpp_grpc_utils_integration_tests})
        google_cloud_cpp_grpc_utils_add_test("${fname}"
                                             "integration-test-production")
    endforeach ()

    set(google_cloud_cpp_grpc_utils_benchmarks # cmake-format: sortable
                                               completion_queue_benchmark.cc)

    # Export the list of benchmarks to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("google_cloud_cpp_grpc_utils_benchmarks.bzl"
                         "google_cloud_cpp_grpc_utils_benchmarks" YEAR "2020")

    # Generate a target for each benchmark.
    foreach (fname ${google_cloud_cpp_grpc_utils_benchmarks})
        google_cloud_cpp_add_executable(target "common" "${fname}")
        add_test(NAME ${target} COMMAND ${target})
        target_link_libraries(
            ${target}
            PRIVATE google-cloud-cpp::grpc_utils google-cloud-cpp::common
                    benchmark::benchmark_main)
        google_cloud_cpp_add_common_options(${target})
    endforeach ()
endif ()

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

find_package(OpenSSL REQUIRED)

# Generate the version information from the CMake values.
configure_file(internal/version_info.h.in
               ${CMAKE_CURRENT_SOURCE_DIR}/internal/version_info.h)

# Create the file that captures build information. Having access to the compiler
# and build flags at runtime allows us to print better benchmark results.
string(TOUPPER "${CMAKE_BUILD_TYPE}" GOOGLE_CLOUD_CPP_BUILD_TYPE_UPPER)
configure_file(internal/build_info.cc.in internal/build_info.cc)

# the client library
add_library(
    google_cloud_cpp_common # cmake-format: sort
    ${CMAKE_CURRENT_BINARY_DIR}/internal/build_info.cc
    access_token.cc
    access_token.h
    backoff_policy.h
    common_options.h
    credentials.cc
    credentials.h
    experimental_tag.h
    future.h
    future_generic.h
    future_void.h
    idempotency.h
    internal/absl_str_cat_quiet.h
    internal/absl_str_join_quiet.h
    internal/absl_str_replace_quiet.h
    internal/algorithm.h
    internal/api_client_header.cc
    internal/api_client_header.h
    internal/attributes.h
    internal/auth_header_error.cc
    internal/auth_header_error.h
    internal/backoff_policy.cc
    internal/backoff_policy.h
    internal/base64_transforms.cc
    internal/base64_transforms.h
    internal/big_endian.h
    internal/build_info.h
    internal/call_context.h
    internal/clock.h
    internal/compiler_info.cc
    internal/compiler_info.h
    internal/compute_engine_util.cc
    internal/compute_engine_util.h
    internal/credentials_impl.cc
    internal/credentials_impl.h
    internal/debug_future_status.cc
    internal/debug_future_status.h
    internal/debug_string.cc
    internal/debug_string.h
    internal/diagnostics_pop.inc
    internal/diagnostics_push.inc
    internal/disable_deprecation_warnings.inc
    internal/disable_msvc_crt_secure_warnings.inc
    internal/error_context.cc
    internal/error_context.h
    internal/filesystem.cc
    internal/filesystem.h
    internal/format_time_point.cc
    internal/format_time_point.h
    internal/future_base.h
    internal/future_coroutines.h
    internal/future_fwd.h
    internal/future_impl.cc
    internal/future_impl.h
    internal/future_then_impl.h
    internal/future_then_meta.h
    internal/getenv.cc
    internal/getenv.h
    internal/group_options.h
    internal/invoke_result.h
    internal/ios_flags_saver.h
    internal/log_impl.cc
    internal/log_impl.h
    internal/make_status.cc
    internal/make_status.h
    internal/noexcept_action.cc
    internal/noexcept_action.h
    internal/non_constructible.h
    internal/opentelemetry.cc
    internal/opentelemetry.h
    internal/opentelemetry_context.cc
    internal/opentelemetry_context.h
    internal/pagination_range.h
    internal/parse_rfc3339.cc
    internal/parse_rfc3339.h
    internal/populate_common_options.cc
    internal/populate_common_options.h
    internal/port_platform.h
    internal/random.cc
    internal/random.h
    internal/retry_loop_helpers.cc
    internal/retry_loop_helpers.h
    internal/retry_policy_impl.cc
    internal/retry_policy_impl.h
    internal/service_endpoint.cc
    internal/service_endpoint.h
    internal/sha256_hash.cc
    internal/sha256_hash.h
    internal/sha256_hmac.cc
    internal/sha256_hmac.h
    internal/sha256_type.h
    internal/status_payload_keys.cc
    internal/status_payload_keys.h
    internal/strerror.cc
    internal/strerror.h
    internal/subject_token.cc
    internal/subject_token.h
    internal/throw_delegate.cc
    internal/throw_delegate.h
    internal/timer_queue.cc
    internal/timer_queue.h
    internal/trace_propagator.cc
    internal/trace_propagator.h
    internal/traced_stream_range.h
    internal/tuple.h
    internal/type_list.h
    internal/type_traits.h
    internal/url_encode.cc
    internal/url_encode.h
    internal/user_agent_prefix.cc
    internal/user_agent_prefix.h
    internal/utility.h
    internal/version_info.h
    kms_key_name.cc
    kms_key_name.h
    location.cc
    location.h
    log.cc
    log.h
    opentelemetry_options.h
    optional.h
    options.cc
    options.h
    polling_policy.h
    project.cc
    project.h
    retry_policy.h
    status.cc
    status.h
    status_or.h
    stream_range.h
    terminate_handler.cc
    terminate_handler.h
    tracing_options.cc
    tracing_options.h
    universe_domain_options.h
    version.cc
    version.h)
target_link_libraries(
    google_cloud_cpp_common
    PUBLIC absl::base
           absl::memory
           absl::optional
           absl::span
           absl::time
           absl::variant
           Threads::Threads
           OpenSSL::Crypto)

if (opentelemetry IN_LIST GOOGLE_CLOUD_CPP_ENABLE)
    find_package(opentelemetry-cpp CONFIG)
    if (opentelemetry-cpp_FOUND)
        target_link_libraries(google_cloud_cpp_common
                              PUBLIC opentelemetry-cpp::api)
        target_compile_definitions(
            google_cloud_cpp_common
            PUBLIC # Enable OpenTelemetry features in google-cloud-cpp
                   GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
                   # TODO(#10975) - install separate opentelemetry .pc file
                   HAVE_ABSEIL)
        set(GOOGLE_CLOUD_CPP_FIND_OPTIONAL_DEPENDENCIES
            "find_dependency(opentelemetry-cpp)")
    endif ()
endif ()
google_cloud_cpp_add_common_options(google_cloud_cpp_common)
target_include_directories(
    google_cloud_cpp_common PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                   $<INSTALL_INTERFACE:include>)

# We're putting generated code into ${PROJECT_BINARY_DIR} (e.g. compiled
# protobufs or build info), so we need it on the include path, however we don't
# want it checked by linters so we mark it as SYSTEM.
target_include_directories(google_cloud_cpp_common SYSTEM
                           PUBLIC $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>)
target_compile_options(google_cloud_cpp_common
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

set_target_properties(
    google_cloud_cpp_common
    PROPERTIES EXPORT_NAME "google-cloud-cpp::common"
               VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(google-cloud-cpp::common ALIAS google_cloud_cpp_common)

create_bazel_config(google_cloud_cpp_common YEAR 2018)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_common-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_common"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_common
    EXPORT google_cloud_cpp_common-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_COMPONENT google_cloud_cpp_development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers(google_cloud_cpp_common include/google/cloud)

google_cloud_cpp_add_pkgconfig(
    "common"
    "Google Cloud C++ Client Library Common Components"
    "Common Components used by the Google Cloud C++ Client Libraries."
    "absl_optional"
    "absl_span"
    "absl_strings"
    "absl_time"
    "absl_time_zone"
    "absl_variant"
    "openssl")

# Create and install the CMake configuration files.
configure_file("config.cmake.in" "google_cloud_cpp_common-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_common-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_common-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_common-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_common"
    COMPONENT google_cloud_cpp_development)

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file).
add_library(google_cloud_cpp_mocks INTERFACE)
set(google_cloud_cpp_mocks_hdrs
    # cmake-format: sort
    mocks/current_options.h mocks/mock_async_streaming_read_write_rpc.h
    mocks/mock_stream_range.h)
export_list_to_bazel("google_cloud_cpp_mocks.bzl" "google_cloud_cpp_mocks_hdrs"
                     YEAR "2022")
target_link_libraries(
    google_cloud_cpp_mocks INTERFACE google-cloud-cpp::common GTest::gmock_main
                                     GTest::gmock GTest::gtest)
set_target_properties(google_cloud_cpp_mocks PROPERTIES EXPORT_NAME
                                                        google-cloud-cpp::mocks)
target_include_directories(
    google_cloud_cpp_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
add_library(google-cloud-cpp::mocks ALIAS google_cloud_cpp_mocks)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_mocks-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_mocks"
    COMPONENT google_cloud_cpp_development)

install(
    TARGETS google_cloud_cpp_mocks
    EXPORT google_cloud_cpp_mocks-targets
    COMPONENT google_cloud_cpp_development)
install(
    FILES ${google_cloud_cpp_mocks_hdrs}
    DESTINATION "include/google/cloud/mocks"
    COMPONENT google_cloud_cpp_development)

google_cloud_cpp_add_pkgconfig(
    "mocks" "Google Cloud C++ Testing Library"
    "Helpers for testing the Google Cloud C++ Client Libraries"
    "google_cloud_cpp_common" "gmock_main")

# Create and install the CMake configuration files.
configure_file("mocks-config.cmake.in" "google_cloud_cpp_mocks-config.cmake"
               @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_mocks-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_mocks-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_mocks-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_mocks"
    COMPONENT google_cloud_cpp_development)

if (BUILD_TESTING)
    include(FindBenchmarkWithWorkarounds)

    set(google_cloud_cpp_common_unit_tests
        # cmake-format: sort
        access_token_test.cc
        common_options_test.cc
        future_coroutines_test.cc
        future_generic_test.cc
        future_generic_then_test.cc
        future_void_test.cc
        future_void_then_test.cc
        internal/algorithm_test.cc
        internal/api_client_header_test.cc
        internal/backoff_policy_test.cc
        internal/base64_transforms_test.cc
        internal/big_endian_test.cc
        internal/call_context_test.cc
        internal/clock_test.cc
        internal/compiler_info_test.cc
        internal/compute_engine_util_test.cc
        internal/credentials_impl_test.cc
        internal/debug_future_status_test.cc
        internal/debug_string_test.cc
        internal/error_context_test.cc
        internal/filesystem_test.cc
        internal/format_time_point_test.cc
        internal/future_impl_test.cc
        internal/group_options_test.cc
        internal/invoke_result_test.cc
        internal/log_impl_test.cc
        internal/make_status_test.cc
        internal/noexcept_action_test.cc
        internal/opentelemetry_context_test.cc
        internal/opentelemetry_test.cc
        internal/pagination_range_test.cc
        internal/parse_rfc3339_test.cc
        internal/populate_common_options_test.cc
        internal/random_test.cc
        internal/retry_loop_helpers_test.cc
        internal/retry_policy_impl_test.cc
        internal/service_endpoint_test.cc
        internal/sha256_hash_test.cc
        internal/sha256_hmac_test.cc
        internal/status_payload_keys_test.cc
        internal/strerror_test.cc
        internal/subject_token_test.cc
        internal/throw_delegate_test.cc
        internal/timer_queue_test.cc
        internal/trace_propagator_test.cc
        internal/traced_stream_range_test.cc
        internal/tuple_test.cc
        internal/type_list_test.cc
        internal/url_encode_test.cc
        internal/user_agent_prefix_test.cc
        internal/utility_test.cc
        kms_key_name_test.cc
        location_test.cc
        log_test.cc
        mocks/current_options_test.cc
        mocks/mock_stream_range_test.cc
        options_test.cc
        polling_policy_test.cc
        project_test.cc
        status_or_test.cc
        status_test.cc
        stream_range_test.cc
        terminate_handler_test.cc
        tracing_options_test.cc)

    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("google_cloud_cpp_common_unit_tests.bzl"
                         "google_cloud_cpp_common_unit_tests" YEAR "2018")

    foreach (fname ${google_cloud_cpp_common_unit_tests})
        google_cloud_cpp_add_executable(target "common" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE google_cloud_cpp_testing
                    google-cloud-cpp::common
                    google-cloud-cpp::mocks
                    absl::variant
                    GTest::gmock_main
                    GTest::gmock
                    GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
        add_test(NAME ${target} COMMAND ${target})
    endforeach ()

    set(google_cloud_cpp_common_benchmarks # cmake-format: sort
                                           options_benchmark.cc)

    # Export the list of benchmarks to a .bzl file so we do not need to maintain
    # the list in two places.
    export_list_to_bazel("google_cloud_cpp_common_benchmarks.bzl"
                         "google_cloud_cpp_common_benchmarks" YEAR "2020")

    # Generate a target for each benchmark.
    foreach (fname ${google_cloud_cpp_common_benchmarks})
        google_cloud_cpp_add_executable(target "common" "${fname}")
        add_test(NAME ${target} COMMAND ${target})
        target_link_libraries(${target} PRIVATE google-cloud-cpp::common
                                                benchmark::benchmark_main)
        google_cloud_cpp_add_common_options(${target})
    endforeach ()
endif ()

if (BUILD_TESTING AND GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    google_cloud_cpp_add_samples_relative("common" "samples/")
endif ()

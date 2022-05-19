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
    internal/backoff_policy.cc
    internal/backoff_policy.h
    internal/base64_transforms.cc
    internal/base64_transforms.h
    internal/big_endian.h
    internal/build_info.h
    internal/compiler_info.cc
    internal/compiler_info.h
    internal/compute_engine_util.cc
    internal/compute_engine_util.h
    internal/credentials_impl.cc
    internal/credentials_impl.h
    internal/diagnostics_pop.inc
    internal/diagnostics_push.inc
    internal/disable_deprecation_warnings.inc
    internal/disable_msvc_crt_secure_warnings.inc
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
    internal/invoke_result.h
    internal/ios_flags_saver.h
    internal/log_impl.cc
    internal/log_impl.h
    internal/non_constructible.h
    internal/pagination_range.h
    internal/parse_rfc3339.cc
    internal/parse_rfc3339.h
    internal/populate_common_options.cc
    internal/populate_common_options.h
    internal/port_platform.h
    internal/random.cc
    internal/random.h
    internal/retry_policy.cc
    internal/retry_policy.h
    internal/sha256_hash.cc
    internal/sha256_hash.h
    internal/status_payload_keys.cc
    internal/status_payload_keys.h
    internal/strerror.cc
    internal/strerror.h
    internal/throw_delegate.cc
    internal/throw_delegate.h
    internal/tuple.h
    internal/type_list.h
    internal/type_traits.h
    internal/user_agent_prefix.cc
    internal/user_agent_prefix.h
    internal/utility.h
    internal/version_info.h
    kms_key_name.cc
    kms_key_name.h
    log.cc
    log.h
    optional.h
    options.cc
    options.h
    polling_policy.h
    project.cc
    project.h
    status.cc
    status.h
    status_or.h
    stream_range.h
    terminate_handler.cc
    terminate_handler.h
    tracing_options.cc
    tracing_options.h
    url_encoded.cc
    url_encoded.h
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
            NAMELINK_SKIP
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)
# With CMake-3.12 and higher we could avoid this separate command (and the
# duplication).
install(
    TARGETS google_cloud_cpp_common
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers(google_cloud_cpp_common include/google/cloud)

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CPP_PC_NAME
    "Google Cloud C++ Client Library Common Components")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Common Components used by the Google Cloud C++ Client Libraries.")
set(GOOGLE_CLOUD_CPP_PC_LIBS "-lgoogle_cloud_cpp_common")
string(CONCAT GOOGLE_CLOUD_CPP_PC_REQUIRES "absl_memory" " absl_optional"
              " absl_time" " openssl")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/config.pc.in"
               "google_cloud_cpp_common.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_common.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_development)

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

if (BUILD_TESTING)
    find_package(benchmark CONFIG REQUIRED)

    set(google_cloud_cpp_common_unit_tests
        # cmake-format: sort
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
        internal/compiler_info_test.cc
        internal/compute_engine_util_test.cc
        internal/credentials_impl_test.cc
        internal/filesystem_test.cc
        internal/format_time_point_test.cc
        internal/future_impl_test.cc
        internal/invoke_result_test.cc
        internal/log_impl_test.cc
        internal/pagination_range_test.cc
        internal/parse_rfc3339_test.cc
        internal/populate_common_options_test.cc
        internal/random_test.cc
        internal/retry_policy_test.cc
        internal/sha256_hash_test.cc
        internal/status_payload_keys_test.cc
        internal/strerror_test.cc
        internal/throw_delegate_test.cc
        internal/tuple_test.cc
        internal/type_list_test.cc
        internal/user_agent_prefix_test.cc
        internal/utility_test.cc
        kms_key_name_test.cc
        log_test.cc
        options_test.cc
        polling_policy_test.cc
        project_test.cc
        status_or_test.cc
        status_test.cc
        stream_range_test.cc
        terminate_handler_test.cc
        tracing_options_test.cc
        url_encoded_test.cc)

    # Export the list of unit tests so the Bazel BUILD file can pick it up.
    export_list_to_bazel("google_cloud_cpp_common_unit_tests.bzl"
                         "google_cloud_cpp_common_unit_tests" YEAR "2018")

    foreach (fname ${google_cloud_cpp_common_unit_tests})
        google_cloud_cpp_add_executable(target "common" "${fname}")
        target_link_libraries(
            ${target}
            PRIVATE google_cloud_cpp_testing google-cloud-cpp::common
                    absl::variant GTest::gmock_main GTest::gmock GTest::gtest)
        google_cloud_cpp_add_common_options(${target})
        if (MSVC)
            target_compile_options(${target} PRIVATE "/bigobj")
        endif ()
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

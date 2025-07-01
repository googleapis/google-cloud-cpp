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
    google_cloud_cpp_rest_protobuf_internal # cmake-format: sort
    internal/async_rest_long_running_operation.h
    internal/async_rest_long_running_operation_custom.h
    internal/async_rest_polling_loop.cc
    internal/async_rest_polling_loop.h
    internal/async_rest_polling_loop_custom.h
    internal/async_rest_polling_loop_impl.h
    internal/async_rest_retry_loop.h
    internal/rest_background_threads_impl.cc
    internal/rest_background_threads_impl.h
    internal/rest_completion_queue_impl.cc
    internal/rest_completion_queue_impl.h
    internal/rest_stub_helpers.cc
    internal/rest_stub_helpers.h)
target_link_libraries(
    google_cloud_cpp_rest_protobuf_internal
    PUBLIC google-cloud-cpp::common google-cloud-cpp::rest_internal
           google-cloud-cpp::grpc_utils)
google_cloud_cpp_add_common_options(google_cloud_cpp_rest_protobuf_internal)
target_include_directories(
    google_cloud_cpp_rest_protobuf_internal
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_rest_protobuf_internal
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(
    google_cloud_cpp_rest_protobuf_internal
    PROPERTIES EXPORT_NAME "google-cloud-cpp::rest_protobuf_internal"
               VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(google-cloud-cpp::rest_protobuf_internal ALIAS
            google_cloud_cpp_rest_protobuf_internal)

create_bazel_config(google_cloud_cpp_rest_protobuf_internal YEAR 2022)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_rest_protobuf_internal-targets
    DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_rest_protobuf_internal"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_rest_protobuf_internal
    EXPORT google_cloud_cpp_rest_protobuf_internal-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_runtime
            NAMELINK_COMPONENT google_cloud_cpp_development
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers(google_cloud_cpp_rest_protobuf_internal
                                 include/google/cloud)

google_cloud_cpp_add_pkgconfig(
    rest_protobuf_internal
    "REST/Protobuf Utilities for the Google Cloud C++ Client Library"
    "Provides REST and Protobuf Utilities for the Google Cloud C++ Client Library."
    "google_cloud_cpp_common"
    "google_cloud_cpp_grpc_utils"
    "google_cloud_cpp_rest_internal")

# Create and install the CMake configuration files.
configure_file("config-rest-protobuf.cmake.in"
               "google_cloud_cpp_rest_protobuf_internal-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_rest_protobuf_internal-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_rest_protobuf_internal-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_rest_protobuf_internal-config-version.cmake"
    DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_rest_protobuf_internal"
    COMPONENT google_cloud_cpp_development)

function (google_cloud_cpp_rest_protobuf_internal_add_test fname labels)
    google_cloud_cpp_add_executable(target "common" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE google-cloud-cpp::rest_protobuf_internal
                google_cloud_cpp_testing_grpc
                google_cloud_cpp_testing
                google-cloud-cpp::common
                absl::variant
                GTest::gmock_main
                GTest::gmock
                GTest::gtest)
    google_cloud_cpp_add_common_options(${target})
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "${labels}")
endfunction ()

if (BUILD_TESTING)
    # List the unit tests, then setup the targets and dependencies.
    set(google_cloud_cpp_rest_protobuf_internal_unit_tests
        # cmake-format: sort
        internal/async_rest_long_running_operation_custom_test.cc
        internal/async_rest_long_running_operation_test.cc
        internal/async_rest_polling_loop_custom_test.cc
        internal/async_rest_polling_loop_test.cc
        internal/async_rest_retry_loop_test.cc
        internal/rest_background_threads_impl_test.cc
        internal/rest_completion_queue_impl_test.cc
        internal/rest_log_wrapper_test.cc
        internal/rest_stub_helpers_test.cc)

    # Export the list of unit tests so the Bazel BUILD file can pick them up.
    export_list_to_bazel(
        "google_cloud_cpp_rest_protobuf_internal_unit_tests.bzl"
        "google_cloud_cpp_rest_protobuf_internal_unit_tests" YEAR "2022")

    foreach (fname ${google_cloud_cpp_rest_protobuf_internal_unit_tests})
        google_cloud_cpp_rest_protobuf_internal_add_test("${fname}" "")
    endforeach ()
endif ()

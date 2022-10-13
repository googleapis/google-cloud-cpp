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
add_library(google_cloud_cpp_protobuf_utils # cmake-format: sort
            internal/log_wrapper_helpers.cc internal/log_wrapper_helpers.h)
target_link_libraries(
    google_cloud_cpp_protobuf_utils
    PUBLIC google-cloud-cpp::common google-cloud-cpp::rpc_error_details_protos
           google-cloud-cpp::rpc_status_protos protobuf::libprotobuf)
google_cloud_cpp_add_common_options(google_cloud_cpp_protobuf_utils)
target_include_directories(
    google_cloud_cpp_protobuf_utils
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_protobuf_utils
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
set_target_properties(
    google_cloud_cpp_protobuf_utils
    PROPERTIES EXPORT_NAME "google-cloud-cpp::protobuf_utils"
               VERSION ${PROJECT_VERSION}
               SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(google-cloud-cpp::protobuf_utils ALIAS
            google_cloud_cpp_protobuf_utils)

create_bazel_config(google_cloud_cpp_protobuf_utils YEAR 2022)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_protobuf_utils-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_protobuf_utils"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_protobuf_utils
    EXPORT google_cloud_cpp_protobuf_utils-targets
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
    TARGETS google_cloud_cpp_protobuf_utils
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers(google_cloud_cpp_protobuf_utils
                                 include/google/cloud)

google_cloud_cpp_add_pkgconfig(
    protobuf_utils "Protobuf Utilities for the Google Cloud C++ Client Library"
    "Provides Protobuf Utilities for the Google Cloud C++ Client Library."
    "google_cloud_cpp_common" " google_cloud_cpp_rpc_status_protos")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config-protobuf.cmake.in"
               "google_cloud_cpp_protobuf_utils-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_protobuf_utils-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_protobuf_utils-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_protobuf_utils-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_protobuf_utils"
    COMPONENT google_cloud_cpp_development)

function (google_cloud_cpp_protobuf_utils_add_test fname labels)
    google_cloud_cpp_add_executable(target "common_protobuf_utils" "${fname}")
    target_link_libraries(
        ${target}
        PRIVATE google-cloud-cpp::protobuf_utils
                google_cloud_cpp_testing
                google-cloud-cpp::common
                google-cloud-cpp::spanner_protos
                absl::variant
                GTest::gmock_main
                GTest::gmock
                GTest::gtest)
    google_cloud_cpp_add_common_options(${target})
    if (MSVC)
        target_compile_options(${target} PRIVATE "/bigobj")
    endif ()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "${labels}")
endfunction ()

if (BUILD_TESTING)
    # List the unit tests, then setup the targets and dependencies.
    set(google_cloud_cpp_protobuf_utils_unit_tests
        # cmake-format: sort
        internal/log_wrapper_helpers_test.cc)

    # Export the list of unit tests so the Bazel BUILD file can pick them up.
    export_list_to_bazel(
        "google_cloud_cpp_protobuf_utils_unit_tests.bzl"
        "google_cloud_cpp_protobuf_utils_unit_tests" YEAR "2022")

    foreach (fname ${google_cloud_cpp_protobuf_utils_unit_tests})
        google_cloud_cpp_protobuf_utils_add_test("${fname}" "")
    endforeach ()
endif ()

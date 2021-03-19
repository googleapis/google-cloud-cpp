# ~~~
# Copyright 2021 Google LLC
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

set(DOXYGEN_PROJECT_NAME "Google Cloud Logging C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Logging")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION} (Beta)")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/samples
                         ${CMAKE_CURRENT_SOURCE_DIR}/quickstart)
set(DOXYGEN_EXCLUDE_PATTERNS "*/*_test.cc")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "logging_internal" "logging_testing"
                            "examples")
set(DOXYGEN_PREDEFINED "GOOGLE_CLOUD_CPP_NS=v1")

include(GoogleCloudCppCommon)

find_package(absl CONFIG REQUIRED)

# configure_file(version_info.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version_info.h)
add_library(
    google_cloud_cpp_logging # cmake-format: sort
    internal/logging_service_v2_logging_decorator.gcpcxx.pb.cc
    internal/logging_service_v2_logging_decorator.gcpcxx.pb.h
    internal/logging_service_v2_metadata_decorator.gcpcxx.pb.cc
    internal/logging_service_v2_metadata_decorator.gcpcxx.pb.h
    internal/logging_service_v2_option_defaults.gcpcxx.pb.cc
    internal/logging_service_v2_option_defaults.gcpcxx.pb.h
    internal/logging_service_v2_stub.gcpcxx.pb.cc
    internal/logging_service_v2_stub.gcpcxx.pb.h
    internal/logging_service_v2_stub_factory.gcpcxx.pb.cc
    internal/logging_service_v2_stub_factory.gcpcxx.pb.h
    logging_service_v2_client.gcpcxx.pb.cc
    logging_service_v2_client.gcpcxx.pb.h
    logging_service_v2_connection.gcpcxx.pb.cc
    logging_service_v2_connection.gcpcxx.pb.h
    logging_service_v2_connection_idempotency_policy.gcpcxx.pb.cc
    logging_service_v2_connection_idempotency_policy.gcpcxx.pb.h
    logging_service_v2_options.gcpcxx.pb.h
    retry_traits.h)
target_include_directories(
    google_cloud_cpp_logging
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_logging
    PUBLIC google-cloud-cpp::grpc_utils google-cloud-cpp::common
           google-cloud-cpp::logging_protos)
google_cloud_cpp_add_common_options(google_cloud_cpp_logging)
set_target_properties(
    google_cloud_cpp_logging
    PROPERTIES EXPORT_NAME google-cloud-cpp::experimental-logging
               VERSION "${PROJECT_VERSION}" SOVERSION
                                            "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_logging
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::experimental-logging ALIAS
            google_cloud_cpp_logging)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_logging YEAR "2021")

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file). Unfortunately INTERFACE libraries are a bit weird in
# that they need absolute paths for their sources.
add_library(google_cloud_cpp_logging_mocks INTERFACE)
target_sources(
    google_cloud_cpp_logging_mocks
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_logging_service_v2_connection.gcpcxx.pb.h
)
target_link_libraries(
    google_cloud_cpp_logging_mocks
    INTERFACE google-cloud-cpp::experimental-logging GTest::gmock_main
              GTest::gmock GTest::gtest)
create_bazel_config(google_cloud_cpp_logging_mocks YEAR "2021")
target_include_directories(
    google_cloud_cpp_logging_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_logging_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_subdirectory(integration_tests)

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_logging-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_logging"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_logging
    EXPORT google_cloud_cpp_logging-targets
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
    TARGETS google_cloud_cpp_logging
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers("google_cloud_cpp_logging"
                                 "include/google/cloud/logging")
google_cloud_cpp_install_headers("google_cloud_cpp_logging_mocks"
                                 "include/google/cloud/logging")

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CONFIG_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(GOOGLE_CLOUD_PC_NAME "The Google Cloud Logging C++ Client Library")
set(GOOGLE_CLOUD_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Logging.")
set(GOOGLE_CLOUD_PC_REQUIRES
    "google_cloud_cpp_grpc_utils google_cloud_cpp_common googleapis_cpp_logging_protos"
)
google_cloud_cpp_absl_pkg_config(absl_pkg_config google_cloud_cpp_logging)
string(CONCAT GOOGLE_CLOUD_PC_LIBS "-lgoogle_cloud_cpp_logging"
              ${absl_pkg_config})

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/logging/config.pc.in"
               "google_cloud_cpp_logging.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_logging.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_development)

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_logging-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_logging-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_logging-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_logging-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_logging"
    COMPONENT google_cloud_cpp_development)

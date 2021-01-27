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

# TODO(#4146) - remove FPHSA_NAME_MISMATCHED manipulation on next absl release
set(FPHSA_NAME_MISMATCHED Threads) # Quiet warning caused by Abseil
find_package(absl CONFIG REQUIRED)
unset(FPHSA_NAME_MISMATCHED)

# configure_file(version_info.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version_info.h)
add_library(
    logging_client # cmake-format: sort
    internal/logging_service_v2_logging_decorator.gcpcxx.pb.cc
    internal/logging_service_v2_logging_decorator.gcpcxx.pb.h
    internal/logging_service_v2_metadata_decorator.gcpcxx.pb.cc
    internal/logging_service_v2_metadata_decorator.gcpcxx.pb.h
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
    retry_traits.h)
target_include_directories(
    logging_client
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    logging_client PUBLIC google_cloud_cpp_grpc_utils google_cloud_cpp_common
                          google-cloud-cpp::logging_protos)
google_cloud_cpp_add_common_options(logging_client)
set_target_properties(
    logging_client PROPERTIES VERSION "${PROJECT_VERSION}"
                              SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(logging_client
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

set_target_properties(logging_client PROPERTIES EXPORT_NAME
                                                googleapis-c++::logging_client)
add_library(googleapis-c++::logging_client ALIAS logging_client)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(logging_client YEAR "2021")

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file). Unfortunately INTERFACE libraries are a bit weird in
# that they need absolute paths for their sources.
add_library(logging_client_mocks INTERFACE)
target_sources(
    logging_client_mocks
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_logging_service_v2_connection.gcpcxx.pb.h
)
target_link_libraries(
    logging_client_mocks
    INTERFACE googleapis-c++::logging_client google_cloud_cpp_testing
              GTest::gmock_main GTest::gmock GTest::gtest)
create_bazel_config(logging_client_mocks YEAR "2021")
target_include_directories(
    logging_client_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(logging_client_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_subdirectory(integration_tests)

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(EXPORT logging-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/logging_client")

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS logging_client
    EXPORT logging-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_logging_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_logging_runtime
            NAMELINK_SKIP
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_logging_development)
# With CMake-3.12 and higher we could avoid this separate command (and the
# duplication).
install(
    TARGETS logging_client
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_logging_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_logging_development)

google_cloud_cpp_install_headers("logging_client"
                                 "include/google/cloud/logging")
google_cloud_cpp_install_headers("logging_client_mocks"
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
google_cloud_cpp_absl_pkg_config(absl_pkg_config logging_client)
string(CONCAT GOOGLE_CLOUD_PC_LIBS "-llogging_client" ${absl_pkg_config})

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/logging/config.pc.in"
               "logging_client.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/logging_client.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_logging_development)

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "logging_client-config.cmake" @ONLY)
write_basic_package_version_file(
    "logging_client-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/logging_client-config.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/logging_client-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/logging_client"
    COMPONENT google_cloud_cpp_logging_development)

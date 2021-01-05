# ~~~
# Copyright 2020 Google LLC
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

set(DOXYGEN_PROJECT_NAME "Google Cloud IAM C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud IAM")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION} (Beta)")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/samples
                         ${CMAKE_CURRENT_SOURCE_DIR}/quickstart)
set(DOXYGEN_EXCLUDE_PATTERNS "*/*_test.cc")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "iam_internal" "iam_testing" "examples")
set(DOXYGEN_PREDEFINED "GOOGLE_CLOUD_CPP_NS=v1")

include(GoogleCloudCppCommon)

# TODO(#4146) - remove FPHSA_NAME_MISMATCHED manipulation on next absl release
set(FPHSA_NAME_MISMATCHED Threads) # Quiet warning caused by Abseil
find_package(absl CONFIG REQUIRED)
unset(FPHSA_NAME_MISMATCHED)

# configure_file(version_info.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version_info.h)
add_library(
    iam_client # cmake-format: sort
    iam_credentials_client.gcpcxx.pb.cc
    iam_credentials_client.gcpcxx.pb.h
    iam_credentials_connection.gcpcxx.pb.cc
    iam_credentials_connection.gcpcxx.pb.h
    iam_credentials_connection_idempotency_policy.gcpcxx.pb.cc
    iam_credentials_connection_idempotency_policy.gcpcxx.pb.h
    internal/iam_credentials_logging_decorator.gcpcxx.pb.cc
    internal/iam_credentials_logging_decorator.gcpcxx.pb.h
    internal/iam_credentials_metadata_decorator.gcpcxx.pb.cc
    internal/iam_credentials_metadata_decorator.gcpcxx.pb.h
    internal/iam_credentials_stub.gcpcxx.pb.cc
    internal/iam_credentials_stub.gcpcxx.pb.h
    internal/iam_credentials_stub_factory.gcpcxx.pb.cc
    internal/iam_credentials_stub_factory.gcpcxx.pb.h
    retry_traits.h)
target_include_directories(
    iam_client
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    iam_client PUBLIC google_cloud_cpp_grpc_utils google_cloud_cpp_common
                      googleapis-c++::iam_protos)
google_cloud_cpp_add_common_options(iam_client)
set_target_properties(
    iam_client PROPERTIES VERSION "${PROJECT_VERSION}"
                          SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(iam_client PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

set_target_properties(iam_client PROPERTIES EXPORT_NAME
                                            googleapis-c++::iam_client)
add_library(googleapis-c++::iam_client ALIAS iam_client)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(iam_client YEAR "2020")

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file). Unfortunately INTERFACE libraries are a bit weird in
# that they need absolute paths for their sources.
add_library(iam_client_mocks INTERFACE)
target_sources(
    iam_client_mocks
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_iam_credentials_connection.gcpcxx.pb.h
)
target_link_libraries(
    iam_client_mocks
    INTERFACE googleapis-c++::iam_client google_cloud_cpp_testing
              GTest::gmock_main GTest::gmock GTest::gtest)
create_bazel_config(iam_client_mocks YEAR "2020")
target_include_directories(
    iam_client_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(iam_client_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_subdirectory(integration_tests)

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(EXPORT iam-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/iam_client")

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS iam_client
    EXPORT iam-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            COMPONENT google_cloud_cpp_iam_runtime
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_iam_runtime
            NAMELINK_SKIP
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_iam_development)
# With CMake-3.12 and higher we could avoid this separate command (and the
# duplication).
install(
    TARGETS iam_client
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_iam_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_iam_development)

google_cloud_cpp_install_headers("iam_client" "include/google/cloud/iam")
google_cloud_cpp_install_headers("iam_client_mocks" "include/google/cloud/iam")

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CONFIG_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(GOOGLE_CLOUD_PC_NAME "The Google Cloud IAM C++ Client Library")
set(GOOGLE_CLOUD_PC_DESCRIPTION "Provides C++ APIs to access Google Cloud IAM.")
set(GOOGLE_CLOUD_PC_REQUIRES
    "google_cloud_cpp_grpc_utils google_cloud_cpp_common googleapis_cpp_iam_protos"
)
google_cloud_cpp_absl_pkg_config(absl_pkg_config iam_client)
string(CONCAT GOOGLE_CLOUD_PC_LIBS "-liam_client" ${absl_pkg_config})

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/iam/config.pc.in"
               "iam_client.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/iam_client.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_iam_development)

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "iam_client-config.cmake" @ONLY)
write_basic_package_version_file(
    "iam_client-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/iam_client-config.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/iam_client-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/iam_client"
    COMPONENT google_cloud_cpp_iam_development)

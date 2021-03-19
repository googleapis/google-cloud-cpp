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

find_package(absl CONFIG REQUIRED)

add_library(
    google_cloud_cpp_iam # cmake-format: sort
    iam_credentials_client.gcpcxx.pb.cc
    iam_credentials_client.gcpcxx.pb.h
    iam_credentials_connection.gcpcxx.pb.cc
    iam_credentials_connection.gcpcxx.pb.h
    iam_credentials_connection_idempotency_policy.gcpcxx.pb.cc
    iam_credentials_connection_idempotency_policy.gcpcxx.pb.h
    iam_credentials_options.gcpcxx.pb.h
    internal/iam_credentials_logging_decorator.gcpcxx.pb.cc
    internal/iam_credentials_logging_decorator.gcpcxx.pb.h
    internal/iam_credentials_metadata_decorator.gcpcxx.pb.cc
    internal/iam_credentials_metadata_decorator.gcpcxx.pb.h
    internal/iam_credentials_option_defaults.gcpcxx.pb.cc
    internal/iam_credentials_option_defaults.gcpcxx.pb.h
    internal/iam_credentials_stub.gcpcxx.pb.cc
    internal/iam_credentials_stub.gcpcxx.pb.h
    internal/iam_credentials_stub_factory.gcpcxx.pb.cc
    internal/iam_credentials_stub_factory.gcpcxx.pb.h
    retry_traits.h)
target_include_directories(
    google_cloud_cpp_iam
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_iam
    PUBLIC google-cloud-cpp::grpc_utils google-cloud-cpp::common
           google-cloud-cpp::iam_protos)
google_cloud_cpp_add_common_options(google_cloud_cpp_iam)
set_target_properties(
    google_cloud_cpp_iam
    PROPERTIES EXPORT_NAME google-cloud-cpp::experimental-iam
               VERSION "${PROJECT_VERSION}" SOVERSION
                                            "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_iam
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::experimental-iam ALIAS google_cloud_cpp_iam)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_iam YEAR "2020")

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file). Unfortunately INTERFACE libraries are a bit weird in
# that they need absolute paths for their sources.
add_library(google_cloud_cpp_iam_mocks INTERFACE)
target_sources(
    google_cloud_cpp_iam_mocks
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_iam_credentials_connection.gcpcxx.pb.h
)
target_link_libraries(
    google_cloud_cpp_iam_mocks
    INTERFACE google-cloud-cpp::experimental-iam_mocks
              google-cloud-cpp::experimental-iam GTest::gmock_main GTest::gmock
              GTest::gtest)
create_bazel_config(google_cloud_cpp_iam_mocks YEAR "2020")
target_include_directories(
    google_cloud_cpp_iam_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_iam_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_subdirectory(integration_tests)

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_iam-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_iam"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_iam
    EXPORT google_cloud_cpp_iam-targets
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
    TARGETS google_cloud_cpp_iam
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_headers("google_cloud_cpp_iam"
                                 "include/google/cloud/iam")
google_cloud_cpp_install_headers("google_cloud_cpp_iam_mocks"
                                 "include/google/cloud/iam")

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CONFIG_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(GOOGLE_CLOUD_PC_NAME "The Google Cloud IAM C++ Client Library")
set(GOOGLE_CLOUD_PC_DESCRIPTION "Provides C++ APIs to access Google Cloud IAM.")
set(GOOGLE_CLOUD_PC_REQUIRES
    "google_cloud_cpp_grpc_utils google_cloud_cpp_common googleapis_cpp_iam_protos"
)
google_cloud_cpp_absl_pkg_config(absl_pkg_config google_cloud_cpp_iam)
string(CONCAT GOOGLE_CLOUD_PC_LIBS "-lgoogle_cloud_cpp_iam" ${absl_pkg_config})

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/iam/config.pc.in"
               "google_cloud_cpp_iam.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_iam.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_development)

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_iam-config.cmake" @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_iam-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_iam-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_iam-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_iam"
    COMPONENT google_cloud_cpp_development)

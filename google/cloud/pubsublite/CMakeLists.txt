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

set(DOXYGEN_PROJECT_NAME "Cloud Pub/Sub Lite C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Cloud Pub/Sub Lite")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION} (Beta)")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "pubsublite_internal"
                            "pubsublite_testing" "examples")

# Creates the proto headers needed by doxygen.
set(GOOGLE_CLOUD_CPP_DOXYGEN_DEPS google-cloud-cpp::pubsublite_protos)

find_package(gRPC REQUIRED)
find_package(ProtobufWithTargets REQUIRED)
find_package(absl CONFIG REQUIRED)

include(GoogleCloudCppCommon)

set(EXTERNAL_GOOGLEAPIS_SOURCE
    "${PROJECT_BINARY_DIR}/external/googleapis/src/googleapis_download")
find_path(PROTO_INCLUDE_DIR google/protobuf/descriptor.proto)
if (PROTO_INCLUDE_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${PROTO_INCLUDE_DIR}")
endif ()

include(CompileProtos)
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_pubsublite_protos
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/pubsublite/v1/topic_stats.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/pubsublite/v1/subscriber.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/pubsublite/v1/publisher.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/pubsublite/v1/cursor.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/pubsublite/v1/common.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/cloud/pubsublite/v1/admin.proto
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(pubsublite_protos)
target_link_libraries(
    google_cloud_cpp_pubsublite_protos
    PUBLIC google-cloud-cpp::longrunning_operations_protos
           google-cloud-cpp::rpc_status_protos
           google-cloud-cpp::api_resource_protos
           google-cloud-cpp::api_field_behavior_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_http_protos)

add_library(
    google_cloud_cpp_pubsublite # cmake-format: sort
    admin_client.cc
    admin_client.h
    admin_connection.cc
    admin_connection.h
    admin_connection_idempotency_policy.cc
    admin_connection_idempotency_policy.h
    admin_options.h
    internal/admin_auth_decorator.cc
    internal/admin_auth_decorator.h
    internal/admin_logging_decorator.cc
    internal/admin_logging_decorator.h
    internal/admin_metadata_decorator.cc
    internal/admin_metadata_decorator.h
    internal/admin_option_defaults.cc
    internal/admin_option_defaults.h
    internal/admin_stub.cc
    internal/admin_stub.h
    internal/admin_stub_factory.cc
    internal/admin_stub_factory.h
    retry_traits.h)
target_include_directories(
    google_cloud_cpp_pubsublite
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_pubsublite
    PUBLIC google-cloud-cpp::grpc_utils google-cloud-cpp::common
           google-cloud-cpp::pubsublite_protos)
google_cloud_cpp_add_common_options(google_cloud_cpp_pubsublite)
set_target_properties(
    google_cloud_cpp_pubsublite
    PROPERTIES EXPORT_NAME google-cloud-cpp::experimental-pubsublite
               VERSION "${PROJECT_VERSION}" SOVERSION
                                            "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_pubsublite
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::experimental-pubsublite ALIAS
            google_cloud_cpp_pubsublite)

# To avoid maintaining the list of files for the library, export them to a .bzl
# file.
include(CreateBazelConfig)
create_bazel_config(google_cloud_cpp_pubsublite YEAR "2021")

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file). Unfortunately INTERFACE libraries are a bit weird in
# that they need absolute paths for their sources.
add_library(google_cloud_cpp_pubsublite_mocks INTERFACE)
target_sources(
    google_cloud_cpp_pubsublite_mocks
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_admin_connection.h)
target_link_libraries(
    google_cloud_cpp_pubsublite_mocks
    INTERFACE google-cloud-cpp::experimental-pubsublite GTest::gmock_main
              GTest::gmock GTest::gtest)
create_bazel_config(google_cloud_cpp_pubsublite_mocks YEAR "2021")
target_include_directories(
    google_cloud_cpp_pubsublite_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_pubsublite_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_pubsublite-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_pubsublite"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_pubsublite google_cloud_cpp_pubsublite_protos
    EXPORT google_cloud_cpp_pubsublite-targets
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
    TARGETS google_cloud_cpp_pubsublite google_cloud_cpp_pubsublite_protos
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_proto_library_protos(
    "google_cloud_cpp_pubsublite_protos" "${EXTERNAL_GOOGLEAPIS_SOURCE}")
google_cloud_cpp_install_proto_library_headers(
    "google_cloud_cpp_pubsublite_protos")
google_cloud_cpp_install_headers("google_cloud_cpp_pubsublite"
                                 "include/google/cloud/pubsublite")
google_cloud_cpp_install_headers("google_cloud_cpp_pubsublite_mocks"
                                 "include/google/cloud/pubsublite")

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CONFIG_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(GOOGLE_CLOUD_PC_NAME "The Cloud Pub/Sub Lite C++ Client Library")
set(GOOGLE_CLOUD_PC_DESCRIPTION
    "Provides C++ APIs to access Cloud Pub/Sub Lite.")
set(GOOGLE_CLOUD_PC_LIBS "-lgoogle_cloud_cpp_pubsublite")
string(CONCAT GOOGLE_CLOUD_PC_REQUIRES "google_cloud_cpp_grpc_utils"
              " google_cloud_cpp_common" " google_cloud_cpp_pubsublite_protos")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/pubsublite/config.pc.in"
               "google_cloud_cpp_pubsublite.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_pubsublite.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_development)

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_pubsublite-config.cmake"
               @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_pubsublite-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_pubsublite-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_pubsublite-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_pubsublite"
    COMPONENT google_cloud_cpp_development)

external_googleapis_install_pc("google_cloud_cpp_pubsublite_protos"
                               "${CMAKE_CURRENT_LIST_DIR}")

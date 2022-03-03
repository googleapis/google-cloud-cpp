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

include(GoogleapisConfig)
set(DOXYGEN_PROJECT_NAME "App Engine Admin API C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for the App Engine Admin API")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "appengine_internal" "appengine_testing"
                            "examples")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/quickstart)

# Creates the proto headers needed by doxygen.
set(GOOGLE_CLOUD_CPP_DOXYGEN_DEPS google-cloud-cpp::appengine_protos)

include(GoogleCloudCppCommon)

set(EXTERNAL_GOOGLEAPIS_SOURCE
    "${PROJECT_BINARY_DIR}/external/googleapis/src/googleapis_download")
find_path(PROTO_INCLUDE_DIR google/protobuf/descriptor.proto)
if (PROTO_INCLUDE_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${PROTO_INCLUDE_DIR}")
endif ()

include(CompileProtos)
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_appengine_protos
    # cmake-format: sort
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/legacy/audit_data.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/logging/v1/request_log.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/app_yaml.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/appengine.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/application.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/audit_data.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/certificate.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/deploy.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/deployed_files.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/domain.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/domain_mapping.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/firewall.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/instance.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/location.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/network_settings.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/operation.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/service.proto
    ${EXTERNAL_GOOGLEAPIS_SOURCE}/google/appengine/v1/version.proto
    PROTO_PATH_DIRECTORIES
    "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(appengine_protos)
target_link_libraries(
    google_cloud_cpp_appengine_protos
    PUBLIC #
           google-cloud-cpp::api_annotations_protos
           google-cloud-cpp::api_client_protos
           google-cloud-cpp::api_field_behavior_protos
           google-cloud-cpp::api_http_protos
           google-cloud-cpp::api_resource_protos
           google-cloud-cpp::logging_type_type_protos
           google-cloud-cpp::longrunning_operations_protos
           google-cloud-cpp::rpc_status_protos)

file(
    GLOB source_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "*.h" "*.cc" "internal/*.h" "internal/*.cc")
list(SORT source_files)
add_library(google_cloud_cpp_appengine ${source_files})
target_include_directories(
    google_cloud_cpp_appengine
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_appengine
    PUBLIC google-cloud-cpp::grpc_utils google-cloud-cpp::common
           google-cloud-cpp::appengine_protos)
google_cloud_cpp_add_common_options(google_cloud_cpp_appengine)
set_target_properties(
    google_cloud_cpp_appengine
    PROPERTIES EXPORT_NAME google-cloud-cpp::appengine VERSION
                                                       "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_appengine
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::appengine ALIAS google_cloud_cpp_appengine)

# Create a header-only library for the mocks. We use a CMake `INTERFACE` library
# for these, a regular library would not work on macOS (where the library needs
# at least one .o file). Unfortunately INTERFACE libraries are a bit weird in
# that they need absolute paths for their sources.
file(
    GLOB relative_mock_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "mocks/*.h")
list(SORT relative_mock_files)
set(mock_files)
foreach (file IN LISTS relative_mock_files)
    list(APPEND mock_files "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
endforeach ()
add_library(google_cloud_cpp_appengine_mocks INTERFACE)
target_sources(google_cloud_cpp_appengine_mocks INTERFACE ${mock_files})
target_link_libraries(
    google_cloud_cpp_appengine_mocks
    INTERFACE google-cloud-cpp::appengine GTest::gmock_main GTest::gmock
              GTest::gtest)
set_target_properties(google_cloud_cpp_appengine_mocks
                      PROPERTIES EXPORT_NAME google-cloud-cpp::appengine_mocks)
target_include_directories(
    google_cloud_cpp_appengine_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_appengine_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

include(CTest)
if (BUILD_TESTING)
    add_executable(appengine_quickstart "quickstart/quickstart.cc")
    target_link_libraries(appengine_quickstart
                          PRIVATE google-cloud-cpp::appengine)
    google_cloud_cpp_add_common_options(appengine_quickstart)
    add_test(
        NAME appengine_quickstart
        COMMAND cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $<TARGET_FILE:appengine_quickstart> GOOGLE_CLOUD_PROJECT)
    set_tests_properties(appengine_quickstart
                         PROPERTIES LABELS "integration-test;quickstart")
endif ()

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_appengine-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_appengine"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_appengine google_cloud_cpp_appengine_protos
    EXPORT google_cloud_cpp_appengine-targets
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
    TARGETS google_cloud_cpp_appengine google_cloud_cpp_appengine_protos
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_proto_library_protos(
    "google_cloud_cpp_appengine_protos" "${EXTERNAL_GOOGLEAPIS_SOURCE}")
google_cloud_cpp_install_proto_library_headers(
    "google_cloud_cpp_appengine_protos")
google_cloud_cpp_install_headers("google_cloud_cpp_appengine"
                                 "include/google/cloud/appengine")
google_cloud_cpp_install_headers("google_cloud_cpp_appengine_mocks"
                                 "include/google/cloud/appengine")

# Setup global variables used in the following *.in files.
set(GOOGLE_CLOUD_CONFIG_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(GOOGLE_CLOUD_CONFIG_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(GOOGLE_CLOUD_PC_NAME "The App Engine Admin API C++ Client Library")
set(GOOGLE_CLOUD_PC_DESCRIPTION
    "Provides C++ APIs to use the App Engine Admin API.")
set(GOOGLE_CLOUD_PC_LIBS "-lgoogle_cloud_cpp_appengine")
string(CONCAT GOOGLE_CLOUD_PC_REQUIRES "google_cloud_cpp_grpc_utils"
              " google_cloud_cpp_common" " google_cloud_cpp_appengine_protos")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/google/cloud/appengine/config.pc.in"
               "google_cloud_cpp_appengine.pc" @ONLY)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_appengine.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    COMPONENT google_cloud_cpp_development)

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_appengine-config.cmake"
               @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_appengine-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_appengine-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_appengine-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_appengine"
    COMPONENT google_cloud_cpp_development)

external_googleapis_install_pc("google_cloud_cpp_appengine_protos"
                               "${PROJECT_SOURCE_DIR}/external/googleapis")

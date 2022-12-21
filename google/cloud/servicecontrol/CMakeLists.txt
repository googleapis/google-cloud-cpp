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
set(DOXYGEN_PROJECT_NAME "Service Control API C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for the Service Control API")
set(DOXYGEN_PROJECT_NUMBER "${PROJECT_VERSION}")
set(DOXYGEN_EXCLUDE_SYMBOLS "internal" "servicecontrol_internal"
                            "servicecontrol_testing" "examples")
set(DOXYGEN_EXAMPLE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/samples
                         ${CMAKE_CURRENT_SOURCE_DIR}/quickstart)

# Creates the proto headers needed by doxygen.
set(GOOGLE_CLOUD_CPP_DOXYGEN_DEPS google-cloud-cpp::servicecontrol_protos)

include(GoogleCloudCppCommon)

find_path(PROTO_INCLUDE_DIR google/protobuf/descriptor.proto)
if (PROTO_INCLUDE_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${PROTO_INCLUDE_DIR}")
endif ()

include(CompileProtos)
google_cloud_cpp_load_protolist(
    proto_list
    "${PROJECT_SOURCE_DIR}/external/googleapis/protolists/servicecontrol.list")
google_cloud_cpp_load_protodeps(
    proto_deps
    "${PROJECT_SOURCE_DIR}/external/googleapis/protodeps/servicecontrol.deps")
google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_servicecontrol_protos # cmake-format: sort
    ${proto_list} PROTO_PATH_DIRECTORIES "${EXTERNAL_GOOGLEAPIS_SOURCE}"
    "${PROTO_INCLUDE_DIR}")
external_googleapis_set_version_and_alias(servicecontrol_protos)
target_link_libraries(google_cloud_cpp_servicecontrol_protos
                      PUBLIC ${proto_deps})

file(
    GLOB source_files
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "*.h" "*.cc" "internal/*.h" "internal/*.cc")
list(SORT source_files)
add_library(google_cloud_cpp_servicecontrol ${source_files})
target_include_directories(
    google_cloud_cpp_servicecontrol
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
           $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
           $<INSTALL_INTERFACE:include>)
target_link_libraries(
    google_cloud_cpp_servicecontrol
    PUBLIC google-cloud-cpp::grpc_utils google-cloud-cpp::common
           google-cloud-cpp::servicecontrol_protos)
google_cloud_cpp_add_common_options(google_cloud_cpp_servicecontrol)
set_target_properties(
    google_cloud_cpp_servicecontrol
    PROPERTIES EXPORT_NAME google-cloud-cpp::servicecontrol
               VERSION "${PROJECT_VERSION}"
               SOVERSION "${PROJECT_VERSION_MAJOR}")
target_compile_options(google_cloud_cpp_servicecontrol
                       PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

add_library(google-cloud-cpp::servicecontrol ALIAS
            google_cloud_cpp_servicecontrol)

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
add_library(google_cloud_cpp_servicecontrol_mocks INTERFACE)
target_sources(google_cloud_cpp_servicecontrol_mocks INTERFACE ${mock_files})
target_link_libraries(
    google_cloud_cpp_servicecontrol_mocks
    INTERFACE google-cloud-cpp::servicecontrol GTest::gmock_main GTest::gmock
              GTest::gtest)
set_target_properties(
    google_cloud_cpp_servicecontrol_mocks
    PROPERTIES EXPORT_NAME google-cloud-cpp::servicecontrol_mocks)
target_include_directories(
    google_cloud_cpp_servicecontrol_mocks
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
              $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
              $<INSTALL_INTERFACE:include>)
target_compile_options(google_cloud_cpp_servicecontrol_mocks
                       INTERFACE ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})

include(CTest)
if (BUILD_TESTING)
    add_executable(servicecontrol_quickstart "quickstart/quickstart.cc")
    target_link_libraries(servicecontrol_quickstart
                          PRIVATE google-cloud-cpp::servicecontrol)
    google_cloud_cpp_add_common_options(servicecontrol_quickstart)
    add_test(
        NAME servicecontrol_quickstart
        COMMAND cmake -P "${PROJECT_SOURCE_DIR}/cmake/quickstart-runner.cmake"
                $<TARGET_FILE:servicecontrol_quickstart> GOOGLE_CLOUD_PROJECT)
    set_tests_properties(
        servicecontrol_quickstart
        PROPERTIES LABELS "integration-test;quickstart" PASS_REGULAR_EXPRESSION
                   "Error in non-idempotent.*operation Check:")
endif ()

# Get the destination directories based on the GNU recommendations.
include(GNUInstallDirs)

# Export the CMake targets to make it easy to create configuration files.
install(
    EXPORT google_cloud_cpp_servicecontrol-targets
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_servicecontrol"
    COMPONENT google_cloud_cpp_development)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(
    TARGETS google_cloud_cpp_servicecontrol
            google_cloud_cpp_servicecontrol_protos
    EXPORT google_cloud_cpp_servicecontrol-targets
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
    TARGETS google_cloud_cpp_servicecontrol
            google_cloud_cpp_servicecontrol_protos
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development
            NAMELINK_ONLY
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            COMPONENT google_cloud_cpp_development)

google_cloud_cpp_install_proto_library_protos(
    "google_cloud_cpp_servicecontrol_protos" "${EXTERNAL_GOOGLEAPIS_SOURCE}")
google_cloud_cpp_install_proto_library_headers(
    "google_cloud_cpp_servicecontrol_protos")
google_cloud_cpp_install_headers("google_cloud_cpp_servicecontrol"
                                 "include/google/cloud/servicecontrol")
google_cloud_cpp_install_headers("google_cloud_cpp_servicecontrol_mocks"
                                 "include/google/cloud/servicecontrol")

google_cloud_cpp_add_pkgconfig(
    servicecontrol
    "The Service Control API C++ Client Library"
    "Provides C++ APIs to use the Service Control API."
    "google_cloud_cpp_grpc_utils"
    " google_cloud_cpp_common"
    " google_cloud_cpp_servicecontrol_protos")

# Create and install the CMake configuration files.
include(CMakePackageConfigHelpers)
configure_file("config.cmake.in" "google_cloud_cpp_servicecontrol-config.cmake"
               @ONLY)
write_basic_package_version_file(
    "google_cloud_cpp_servicecontrol-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_servicecontrol-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/google_cloud_cpp_servicecontrol-config-version.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/google_cloud_cpp_servicecontrol"
    COMPONENT google_cloud_cpp_development)

external_googleapis_install_pc("google_cloud_cpp_servicecontrol_protos")

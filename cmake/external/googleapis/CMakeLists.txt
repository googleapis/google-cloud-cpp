# ~~~
# Copyright 2019 Google LLC
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

cmake_minimum_required(VERSION 3.5)

# Define the project name and where to report bugs.
set(PACKAGE_BUGREPORT "https://github.com/googleapis/google-cloud-cpp/issues")
project(googleapis-cpp-protos CXX C)

set(GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR 0)
set(GOOGLEAPIS_CPP_PROTOS_VERSION_MINOR 1)
set(GOOGLEAPIS_CPP_PROTOS_VERSION_PATCH 0)

# Configure the compiler options, we will be using C++11 features.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GOOGLE_CLOUD_CPP_USE_LIBCXX "Build with libcxx and libcxxabi" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}")
find_package(ProtobufTargets REQUIRED)
find_package(gRPC REQUIRED)

# Configure the location of proto files, particulary the googleapis protos.
list(APPEND PROTOBUF_IMPORT_DIRS "${PROJECT_SOURCE_DIR}")

# Sometimes (this happens often with vcpkg) protobuf is installed in a non-
# standard directory. We need to find out where, and then add that directory to
# the search path for protos.
find_path(PROTO_INCLUDE_DIR google/protobuf/descriptor.proto)
if (PROTO_INCLUDE_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${PROTO_INCLUDE_DIR}")
endif ()

# Include the functions to compile proto files.
include(CompileProtos)

add_library(googleapis_cpp_common_flags INTERFACE)
if (GOOGLE_CLOUD_CPP_USE_LIBCXX)
    target_compile_options(googleapis_cpp_common_flags
                           INTERFACE "-stdlib=libc++")
    target_link_libraries(googleapis_cpp_common_flags
                          INTERFACE "-stdlib=libc++" "c++abi")
endif ()

include(SelectMSVCRuntime)

# TODO(#2283) - refactor common (non-bigtable) protos to a library
protobuf_generate_cpp(
    PROTO_SOURCES
    PROTO_HDRS
    ${PROJECT_SOURCE_DIR}/google/bigtable/admin/v2/bigtable_instance_admin.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/admin/v2/bigtable_table_admin.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/admin/v2/common.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/admin/v2/instance.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/admin/v2/table.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/v2/bigtable.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/v2/data.proto
    ${PROJECT_SOURCE_DIR}/google/iam/v1/iam_policy.proto
    ${PROJECT_SOURCE_DIR}/google/iam/v1/policy.proto
    ${PROJECT_SOURCE_DIR}/google/longrunning/operations.proto
    ${PROJECT_SOURCE_DIR}/google/rpc/status.proto
    ${PROJECT_SOURCE_DIR}/google/rpc/error_details.proto
    ${PROJECT_SOURCE_DIR}/google/api/annotations.proto
    ${PROJECT_SOURCE_DIR}/google/api/auth.proto
    ${PROJECT_SOURCE_DIR}/google/api/http.proto)
grpc_generate_cpp(
    GRPCPP_SOURCES
    GRPCPP_HDRS
    ${PROJECT_SOURCE_DIR}/google/bigtable/admin/v2/bigtable_instance_admin.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/admin/v2/bigtable_table_admin.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/v2/bigtable.proto
    ${PROJECT_SOURCE_DIR}/google/longrunning/operations.proto)

# Create a library with the generated files from the relevant protos. When
# adding new services (e.g. Spanner) please create a new library and refactor
# common protos
add_library(googleapis_cpp_bigtable_protos
            ${PROTO_SOURCES}
            ${PROTO_HDRS}
            ${GRPCPP_SOURCES}
            ${GRPCPP_HDRS})
target_link_libraries(googleapis_cpp_bigtable_protos
                      PUBLIC gRPC::grpc++ gRPC::grpc protobuf::libprotobuf
                      PRIVATE googleapis_cpp_common_flags)
target_include_directories(googleapis_cpp_bigtable_protos
                           PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<INSTALL_INTERFACE:include>)
set_target_properties(
    googleapis_cpp_bigtable_protos
    PROPERTIES
        VERSION
        "${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_MINOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_PATCH}"
        SOVERSION
        ${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR})
add_library(
    googleapis-c++::bigtable_protos ALIAS googleapis_cpp_bigtable_protos)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
include(GNUInstallDirs)

install(TARGETS googleapis_cpp_bigtable_protos googleapis_cpp_common_flags
        EXPORT googleapis-targets
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Install proto generated files into include/google.
foreach (header ${PROTO_HDRS} ${GRPCPP_HDRS})
    string(REPLACE "${CMAKE_CURRENT_BINARY_DIR}/"
                   ""
                   relative
                   "${header}")
    get_filename_component(dir "${relative}" DIRECTORY)
    install(FILES "${header}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${dir}")
endforeach ()

# Export the CMake targets to make it easy to create configuration files.
install(EXPORT googleapis-targets
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/googleapis")

# Setup global variables used in the following *.in files.
set(
    GOOGLE_CLOUD_CPP_CONFIG_VERSION_MAJOR ${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR}
    )
set(
    GOOGLE_CLOUD_CPP_CONFIG_VERSION_MINOR ${GOOGLEAPIS_CPP_PROTOS_VERSION_MINOR}
    )
set(
    GOOGLE_CLOUD_CPP_CONFIG_VERSION_PATCH ${GOOGLEAPIS_CPP_PROTOS_VERSION_PATCH}
    )
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google APIS C++ Proto Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Platforms.")
set(GOOGLE_CLOUD_CPP_PC_REQUIRES "grpc++ grpc openssl protobuf zlib libcares")
set(GOOGLE_CLOUD_CPP_PC_LIBS "-lgoogleapis_cpp_bigtable_protos")

# Create and install the pkg-config files.
configure_file("${PROJECT_SOURCE_DIR}/config.pc.in" "googleapis.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/googleapis.pc" DESTINATION
              "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# Create and install the CMake configuration files.
configure_file("config.cmake.in" "googleapis-config.cmake" @ONLY)
configure_file(
    "config-version.cmake.in" "googleapis-config-version.cmake" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/googleapis-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/googleapis-config-version.cmake"
              "${PROJECT_SOURCE_DIR}/FindgRPC.cmake"
              "${PROJECT_SOURCE_DIR}/FindProtobufTargets.cmake"
              "${PROJECT_SOURCE_DIR}/PkgConfigHelper.cmake"
              DESTINATION
              "${CMAKE_INSTALL_LIBDIR}/cmake/googleapis")

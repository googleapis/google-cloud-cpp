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
set(GOOGLEAPIS_CPP_PROTOS_VERSION_MINOR 2)
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

protobuf_generate_cpp(API_HTTP_PROTO_SOURCES API_HTTP_PROTO_HDRS
                      ${PROJECT_SOURCE_DIR}/google/api/http.proto)
grpc_generate_cpp(API_HTTP_GRPCPP_SOURCES API_HTTP_GRPCPP_HDRS
                  ${PROJECT_SOURCE_DIR}/google/api/http.proto)
add_library(googleapis_cpp_api_http_protos
            ${API_HTTP_PROTO_SOURCES}
            ${API_HTTP_PROTO_HDRS}
            ${API_HTTP_GRPCPP_SOURCES}
            ${API_HTTP_GRPCPP_HDRS})
target_link_libraries(googleapis_cpp_api_http_protos
                      PUBLIC gRPC::grpc++ gRPC::grpc protobuf::libprotobuf
                      PRIVATE googleapis_cpp_common_flags)
target_include_directories(googleapis_cpp_api_http_protos
                           PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<INSTALL_INTERFACE:include>)
set_target_properties(
    googleapis_cpp_api_http_protos
    PROPERTIES
        VERSION
        "${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_MINOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_PATCH}"
        SOVERSION
        ${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR})
add_library(
    googleapis-c++::api_http_protos ALIAS googleapis_cpp_api_http_protos)

protobuf_generate_cpp(API_ANNOTATIONS_PROTO_SOURCES API_ANNOTATIONS_PROTO_HDRS
                      ${PROJECT_SOURCE_DIR}/google/api/annotations.proto)
grpc_generate_cpp(API_ANNOTATIONS_GRPCPP_SOURCES API_ANNOTATIONS_GRPCPP_HDRS
                  ${PROJECT_SOURCE_DIR}/google/api/annotations.proto)
add_library(googleapis_cpp_api_annotations_protos
            ${API_ANNOTATIONS_PROTO_SOURCES}
            ${API_ANNOTATIONS_PROTO_HDRS}
            ${API_ANNOTATIONS_GRPCPP_SOURCES}
            ${API_ANNOTATIONS_GRPCPP_HDRS})
target_link_libraries(googleapis_cpp_api_annotations_protos
                      PUBLIC googleapis-c++::api_http_protos
                             gRPC::grpc++
                             gRPC::grpc
                             protobuf::libprotobuf
                      PRIVATE googleapis_cpp_common_flags)
target_include_directories(googleapis_cpp_api_annotations_protos
                           PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<INSTALL_INTERFACE:include>)
set_target_properties(
    googleapis_cpp_api_annotations_protos
    PROPERTIES
        VERSION
        "${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_MINOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_PATCH}"
        SOVERSION
        ${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR})
add_library(googleapis-c++::api_annotations_protos ALIAS
            googleapis_cpp_api_annotations_protos)

protobuf_generate_cpp(API_AUTH_PROTO_SOURCES API_AUTH_PROTO_HDRS
                      ${PROJECT_SOURCE_DIR}/google/api/auth.proto)
grpc_generate_cpp(API_AUTH_GRPCPP_SOURCES API_AUTH_GRPCPP_HDRS
                  ${PROJECT_SOURCE_DIR}/google/api/auth.proto)
add_library(googleapis_cpp_api_auth_protos
            ${API_AUTH_PROTO_SOURCES}
            ${API_AUTH_PROTO_HDRS}
            ${API_AUTH_GRPCPP_SOURCES}
            ${API_AUTH_GRPCPP_HDRS})
target_link_libraries(googleapis_cpp_api_auth_protos
                      PUBLIC googleapis-c++::api_annotations_protos
                             gRPC::grpc++
                             gRPC::grpc
                             protobuf::libprotobuf
                      PRIVATE googleapis_cpp_common_flags)
target_include_directories(googleapis_cpp_api_auth_protos
                           PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<INSTALL_INTERFACE:include>)
set_target_properties(
    googleapis_cpp_api_auth_protos
    PROPERTIES
        VERSION
        "${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_MINOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_PATCH}"
        SOVERSION
        ${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR})
add_library(
    googleapis-c++::api_auth_protos ALIAS googleapis_cpp_api_auth_protos)

protobuf_generate_cpp(RPC_ERROR_DETAILS_PROTO_SOURCES
                      RPC_ERROR_DETAILS_PROTO_HDRS
                      ${PROJECT_SOURCE_DIR}/google/rpc/error_details.proto)
grpc_generate_cpp(RPC_ERROR_DETAILS_GRPCPP_SOURCES RPC_ERROR_DETAILS_GRPCPP_HDRS
                  ${PROJECT_SOURCE_DIR}/google/rpc/error_details.proto)
add_library(googleapis_cpp_rpc_error_details_protos
            ${RPC_ERROR_DETAILS_PROTO_SOURCES}
            ${RPC_ERROR_DETAILS_PROTO_HDRS}
            ${RPC_ERROR_DETAILS_GRPCPP_SOURCES}
            ${RPC_ERROR_DETAILS_GRPCPP_HDRS})
target_link_libraries(googleapis_cpp_rpc_error_details_protos
                      PUBLIC gRPC::grpc++ gRPC::grpc protobuf::libprotobuf
                      PRIVATE googleapis_cpp_common_flags)
target_include_directories(googleapis_cpp_rpc_error_details_protos
                           PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<INSTALL_INTERFACE:include>)
set_target_properties(
    googleapis_cpp_rpc_error_details_protos
    PROPERTIES
        VERSION
        "${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_MINOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_PATCH}"
        SOVERSION
        ${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR})
add_library(googleapis-c++::rpc_error_details_protos ALIAS
            googleapis_cpp_rpc_error_details_protos)

protobuf_generate_cpp(RPC_STATUS_PROTO_SOURCES RPC_STATUS_PROTO_HDRS
                      ${PROJECT_SOURCE_DIR}/google/rpc/status.proto)
grpc_generate_cpp(RPC_STATUS_GRPCPP_SOURCES RPC_STATUS_GRPCPP_HDRS
                  ${PROJECT_SOURCE_DIR}/google/rpc/status.proto)
add_library(googleapis_cpp_rpc_status_protos
            ${RPC_STATUS_PROTO_SOURCES}
            ${RPC_STATUS_PROTO_HDRS}
            ${RPC_STATUS_GRPCPP_SOURCES}
            ${RPC_STATUS_GRPCPP_HDRS})
target_link_libraries(googleapis_cpp_rpc_status_protos
                      PUBLIC googleapis-c++::rpc_error_details_protos
                             gRPC::grpc++
                             gRPC::grpc
                             protobuf::libprotobuf
                      PRIVATE googleapis_cpp_common_flags)
target_include_directories(googleapis_cpp_rpc_status_protos
                           PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<INSTALL_INTERFACE:include>)
set_target_properties(
    googleapis_cpp_rpc_status_protos
    PROPERTIES
        VERSION
        "${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_MINOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_PATCH}"
        SOVERSION
        ${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR})
add_library(googleapis-c++::rpc_status_protos ALIAS
            googleapis_cpp_rpc_status_protos)

protobuf_generate_cpp(IAM_V1_POLICY_PROTO_SOURCES IAM_V1_POLICY_PROTO_HDRS
                      ${PROJECT_SOURCE_DIR}/google/iam/v1/policy.proto)
grpc_generate_cpp(IAM_V1_POLICY_GRPCPP_SOURCES IAM_V1_POLICY_GRPCPP_HDRS
                  ${PROJECT_SOURCE_DIR}/google/iam/v1/policy.proto)
add_library(googleapis_cpp_iam_v1_policy_protos
            ${IAM_V1_POLICY_PROTO_SOURCES}
            ${IAM_V1_POLICY_PROTO_HDRS}
            ${IAM_V1_POLICY_GRPCPP_SOURCES}
            ${IAM_V1_POLICY_GRPCPP_HDRS})
target_link_libraries(googleapis_cpp_iam_v1_policy_protos
                      PUBLIC googleapis-c++::api_annotations_protos
                             gRPC::grpc++
                             gRPC::grpc
                             protobuf::libprotobuf
                      PRIVATE googleapis_cpp_common_flags)
target_include_directories(googleapis_cpp_iam_v1_policy_protos
                           PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<INSTALL_INTERFACE:include>)
set_target_properties(
    googleapis_cpp_iam_v1_policy_protos
    PROPERTIES
        VERSION
        "${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_MINOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_PATCH}"
        SOVERSION
        ${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR})
add_library(googleapis-c++::iam_v1_policy_protos ALIAS
            googleapis_cpp_iam_v1_policy_protos)

protobuf_generate_cpp(IAM_V1_IAM_POLICY_PROTO_SOURCES
                      IAM_V1_IAM_POLICY_PROTO_HDRS
                      ${PROJECT_SOURCE_DIR}/google/iam/v1/iam_policy.proto)
grpc_generate_cpp(IAM_V1_IAM_POLICY_GRPCPP_SOURCES IAM_V1_IAM_POLICY_GRPCPP_HDRS
                  ${PROJECT_SOURCE_DIR}/google/iam/v1/iam_policy.proto)
add_library(googleapis_cpp_iam_v1_iam_policy_protos
            ${IAM_V1_IAM_POLICY_PROTO_SOURCES}
            ${IAM_V1_IAM_POLICY_PROTO_HDRS}
            ${IAM_V1_IAM_POLICY_GRPCPP_SOURCES}
            ${IAM_V1_IAM_POLICY_GRPCPP_HDRS})
target_link_libraries(googleapis_cpp_iam_v1_iam_policy_protos
                      PUBLIC googleapis-c++::api_annotations_protos
                             googleapis-c++::iam_v1_policy_protos
                             gRPC::grpc++
                             gRPC::grpc
                             protobuf::libprotobuf
                      PRIVATE googleapis_cpp_common_flags)
target_include_directories(googleapis_cpp_iam_v1_iam_policy_protos
                           PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<INSTALL_INTERFACE:include>)
set_target_properties(
    googleapis_cpp_iam_v1_iam_policy_protos
    PROPERTIES
        VERSION
        "${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_MINOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_PATCH}"
        SOVERSION
        ${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR})
add_library(googleapis-c++::iam_v1_iam_policy_protos ALIAS
            googleapis_cpp_iam_v1_iam_policy_protos)

protobuf_generate_cpp(LONGRUNNING_OPERATIONS_PROTO_SOURCES
                      LONGRUNNING_OPERATIONS_PROTO_HDRS
                      ${PROJECT_SOURCE_DIR}/google/longrunning/operations.proto)
grpc_generate_cpp(LONGRUNNING_OPERATIONS_GRPCPP_SOURCES
                  LONGRUNNING_OPERATIONS_GRPCPP_HDRS
                  ${PROJECT_SOURCE_DIR}/google/longrunning/operations.proto)
add_library(googleapis_cpp_longrunning_operations_protos
            ${LONGRUNNING_OPERATIONS_PROTO_SOURCES}
            ${LONGRUNNING_OPERATIONS_PROTO_HDRS}
            ${LONGRUNNING_OPERATIONS_GRPCPP_SOURCES}
            ${LONGRUNNING_OPERATIONS_GRPCPP_HDRS})
target_link_libraries(googleapis_cpp_longrunning_operations_protos
                      PUBLIC googleapis-c++::api_annotations_protos
                             googleapis-c++::rpc_status_protos
                             gRPC::grpc++
                             gRPC::grpc
                             protobuf::libprotobuf
                      PRIVATE googleapis_cpp_common_flags)
target_include_directories(googleapis_cpp_longrunning_operations_protos
                           PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<INSTALL_INTERFACE:include>)
set_target_properties(
    googleapis_cpp_longrunning_operations_protos
    PROPERTIES
        VERSION
        "${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_MINOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_PATCH}"
        SOVERSION
        ${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR})
add_library(googleapis-c++::longrunning_operations_protos ALIAS
            googleapis_cpp_longrunning_operations_protos)

protobuf_generate_cpp(
    BIGTABLE_PROTO_SOURCES
    BIGTABLE_PROTO_HDRS
    ${PROJECT_SOURCE_DIR}/google/bigtable/admin/v2/bigtable_instance_admin.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/admin/v2/bigtable_table_admin.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/admin/v2/common.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/admin/v2/instance.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/admin/v2/table.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/v2/bigtable.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/v2/data.proto)
grpc_generate_cpp(
    BIGTABLE_GRPCPP_SOURCES
    BIGTABLE_GRPCPP_HDRS
    ${PROJECT_SOURCE_DIR}/google/bigtable/admin/v2/bigtable_instance_admin.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/admin/v2/bigtable_table_admin.proto
    ${PROJECT_SOURCE_DIR}/google/bigtable/v2/bigtable.proto
    ${PROJECT_SOURCE_DIR}/google/longrunning/operations.proto)

# Create a library with the generated files from the relevant protos. When
# adding new services (e.g. Spanner) please create a new library and refactor
# common protos
add_library(googleapis_cpp_bigtable_protos
            ${BIGTABLE_PROTO_SOURCES}
            ${BIGTABLE_PROTO_HDRS}
            ${BIGTABLE_GRPCPP_SOURCES}
            ${BIGTABLE_GRPCPP_HDRS})
target_link_libraries(googleapis_cpp_bigtable_protos
                      PUBLIC googleapis-c++::api_annotations_protos
                             googleapis-c++::api_auth_protos
                             googleapis-c++::longrunning_operations_protos
                             googleapis-c++::rpc_status_protos
                             googleapis-c++::iam_v1_iam_policy_protos
                             gRPC::grpc++
                             gRPC::grpc
                             protobuf::libprotobuf
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

protobuf_generate_cpp(
    SPANNER_PROTO_SOURCES
    SPANNER_PROTO_HDRS
    ${PROJECT_SOURCE_DIR}/google/spanner/admin/database/v1/spanner_database_admin.proto
    ${PROJECT_SOURCE_DIR}/google/spanner/admin/instance/v1/spanner_instance_admin.proto
    ${PROJECT_SOURCE_DIR}/google/spanner/v1/keys.proto
    ${PROJECT_SOURCE_DIR}/google/spanner/v1/mutation.proto
    ${PROJECT_SOURCE_DIR}/google/spanner/v1/query_plan.proto
    ${PROJECT_SOURCE_DIR}/google/spanner/v1/result_set.proto
    ${PROJECT_SOURCE_DIR}/google/spanner/v1/spanner.proto
    ${PROJECT_SOURCE_DIR}/google/spanner/v1/transaction.proto
    ${PROJECT_SOURCE_DIR}/google/spanner/v1/type.proto)
grpc_generate_cpp(
    SPANNER_GRPCPP_SOURCES
    SPANNER_GRPCPP_HDRS
    ${PROJECT_SOURCE_DIR}/google/spanner/admin/database/v1/spanner_database_admin.proto
    ${PROJECT_SOURCE_DIR}/google/spanner/admin/instance/v1/spanner_instance_admin.proto
    ${PROJECT_SOURCE_DIR}/google/spanner/v1/spanner.proto)
add_library(googleapis_cpp_spanner_protos
            ${SPANNER_PROTO_SOURCES}
            ${SPANNER_PROTO_HDRS}
            ${SPANNER_GRPCPP_SOURCES}
            ${SPANNER_GRPCPP_HDRS})
target_link_libraries(googleapis_cpp_spanner_protos
                      PUBLIC googleapis-c++::api_annotations_protos
                             googleapis-c++::longrunning_operations_protos
                             googleapis-c++::rpc_status_protos
                             googleapis-c++::iam_v1_iam_policy_protos
                             gRPC::grpc++
                             gRPC::grpc
                             protobuf::libprotobuf
                      PRIVATE googleapis_cpp_common_flags)
target_include_directories(googleapis_cpp_spanner_protos
                           PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
                                  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
                                  $<INSTALL_INTERFACE:include>)
set_target_properties(
    googleapis_cpp_spanner_protos
    PROPERTIES
        VERSION
        "${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_MINOR}.${GOOGLEAPIS_CPP_PROTOS_VERSION_PATCH}"
        SOVERSION
        ${GOOGLEAPIS_CPP_PROTOS_VERSION_MAJOR})
add_library(googleapis-c++::spanner_protos ALIAS googleapis_cpp_spanner_protos)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
include(GNUInstallDirs)

set(googleapis_cpp_installed_libraries_list
    googleapis_cpp_bigtable_protos
    googleapis_cpp_spanner_protos
    googleapis_cpp_longrunning_operations_protos
    googleapis_cpp_api_http_protos
    googleapis_cpp_api_annotations_protos
    googleapis_cpp_api_auth_protos
    googleapis_cpp_iam_v1_policy_protos
    googleapis_cpp_iam_v1_iam_policy_protos
    googleapis_cpp_rpc_error_details_protos
    googleapis_cpp_rpc_status_protos)

install(TARGETS ${googleapis_cpp_installed_libraries_list}
                googleapis_cpp_common_flags
        EXPORT googleapis-targets
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Install proto generated files into include/google.
function (googleapis_cpp_install_headers target)
    get_target_property(target_sources ${target} SOURCES)
    foreach (header ${target_sources})
        # Skip anything that is not a header file.
        if (NOT "${header}" MATCHES "\\.h$")
            continue()
        endif ()
        string(REPLACE "${CMAKE_CURRENT_BINARY_DIR}/"
                       ""
                       relative
                       "${header}")
        get_filename_component(dir "${relative}" DIRECTORY)
        install(FILES "${header}" DESTINATION
                      "${CMAKE_INSTALL_INCLUDEDIR}/${dir}")
    endforeach ()
endfunction ()

foreach (target ${googleapis_cpp_installed_libraries_list})
    googleapis_cpp_install_headers("${target}")
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

# Use a function to create a scope for the variables.
function (googleapis_cpp_install_pc target)
    string(REPLACE "googleapis_cpp_"
                   ""
                   _short_name
                   ${target})
    string(REPLACE "_protos"
                   ""
                   _short_name
                   ${_short_name})
    set(GOOGLE_CLOUD_CPP_PC_NAME
        "The Google APIS C++ ${_short_name} Proto Library")
    set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION "Compiled proto for C++.")
    # Examine the target LINK_LIBRARIES property, use that to pull the
    # dependencies between the googleapis-c++::* libraries.
    set(_target_pc_requires)
    get_target_property(_target_deps ${target} LINK_LIBRARIES)
    foreach (dep ${_target_deps})
        if ("${dep}" MATCHES "^googleapis-c\\+\\+::")
            string(REPLACE "googleapis-c++::"
                           "googleapis_cpp_"
                           dep
                           "${dep}")
            list(APPEND _target_pc_requires " " "${dep}")
        endif ()
    endforeach ()
    # These dependencies are required for all the googleapis-c++::* libraries.
    list(APPEND _target_pc_requires
                " grpc++"
                " grpc"
                " openssl"
                " protobuf"
                " zlib"
                " libcares")
    string(CONCAT GOOGLE_CLOUD_CPP_PC_REQUIRES ${_target_pc_requires})
    set(GOOGLE_CLOUD_CPP_PC_LIBS "-l${target}")
    configure_file("${PROJECT_SOURCE_DIR}/config.pc.in" "${target}.pc" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${target}.pc" DESTINATION
                  "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endfunction ()

# Create and install the pkg-config files.
foreach (target ${googleapis_cpp_installed_libraries_list})
    googleapis_cpp_install_pc("${target}")
endforeach ()

# Create and install the googleapis pkg-config file for backwards compatibility.
set(GOOGLE_CLOUD_CPP_PC_NAME "The Google APIS C++ Proto Library")
set(GOOGLE_CLOUD_CPP_PC_DESCRIPTION
    "Provides C++ APIs to access Google Cloud Platforms.")
# Note the use of spaces, `string(JOIN)` is not available in cmake-3.5, so we
# need to add the separator ourselves.
string(CONCAT GOOGLE_CLOUD_CPP_PC_REQUIRES
              "googleapis_cpp_bigtable_protos"
              " googleapis_cpp_iam_v1_iam_policy_protos"
              " googleapis_cpp_iam_v1_policy_protos"
              " googleapis_cpp_longrunning_operations_protos"
              " googleapis_cpp_api_auth_protos"
              " googleapis_cpp_api_annotations_protos"
              " googleapis_cpp_api_http_protos"
              " googleapis_cpp_rpc_status_protos"
              " googleapis_cpp_rpc_error_details_protos"
              " grpc++"
              " grpc"
              " openssl"
              " protobuf"
              " zlib"
              " libcares")
set(GOOGLE_CLOUD_CPP_PC_LIBS "")
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

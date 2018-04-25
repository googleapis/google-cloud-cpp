# Copyright 2017 Google Inc.
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

# This should be included from the top level CMakeLists file
set(BIGTABLE_CLIENT_VERSION_MAJOR 0)
set(BIGTABLE_CLIENT_VERSION_MINOR 2)
set(BIGTABLE_CLIENT_VERSION_PATCH 0)

# Configure the Compiler options, we will be using C++11 features.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DOXYGEN_PROJECT_NAME "Google Cloud Bigtable C++ Client")
set(DOXYGEN_PROJECT_BRIEF "A C++ Client Library for Google Cloud Bigtable")
set(DOXYGEN_PROJECT_NUMBER
    "${BIGTABLE_CLIENT_VERSION_MAJOR}.${BIGTABLE_CLIENT_VERSION_MINOR}.${BIGTABLE_CLIENT_VERSION_PATCH}")
set(DOXYGEN_EXAMPLE_PATH examples)
set(DOXYGEN_PREDEFINED "BIGTABLE_CLIENT_NS=v${BIGTABLE_CLIENT_VERSION_MAJOR}")
set(DOXYGEN_EXCLUDE_PATTERNS "*/client/internal/*;*/client/testing/*;*/*_test.cc")

include(GoogleCloudCppCommon)

# Define an interface library, i.e., a library that really has no sources, and
# add public target options to it.  The targets then use the library via
# target_link_libraries(target PRIVATE bigtable_common_options), which
# implicitly adds the options to those targets, but does *not* add the options
# to any targets that depend on them.  This is a CMake idiom to add options to
# targets without having to painfully set special variables, it also allows
# users to add more common options without having to touch the code.
add_library(bigtable_common_options INTERFACE)
google_cloud_cpp_add_common_options(bigtable_common_options)

# Configure the location of proto files, particulary the googleapis protos.
set(PROTOBUF_IMPORT_DIRS "${PROJECT_THIRD_PARTY_DIR}/googleapis" "${PROJECT_SOURCE_DIR}")
if(GRPC_ROOT_DIR)
    list(INSERT PROTOBUF_IMPORT_DIRS 0 "${GRPC_ROOT_DIR}/third_party/protobuf/src")
endif(GRPC_ROOT_DIR)

# Include the functions to compile proto files.
include(CompileProtos)
PROTOBUF_GENERATE_CPP(PROTO_SOURCES PROTO_HDRS
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_instance_admin.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_table_admin.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/common.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/instance.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/table.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/v2/bigtable.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/v2/data.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/iam/v1/iam_policy.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/iam/v1/policy.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/longrunning/operations.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/rpc/status.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/rpc/error_details.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/api/annotations.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/api/auth.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/api/http.proto)
GRPC_GENERATE_CPP(GRPCPP_SOURCES GRPCPP_HDRS
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_instance_admin.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_table_admin.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/v2/bigtable.proto
    ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/longrunning/operations.proto)

# Create a library with the generated files from the relevant protos.
add_library(bigtable_protos ${PROTO_SOURCES} ${PROTO_HDRS} ${GRPCPP_SOURCES} ${GRPCPP_HDRS})
target_link_libraries(bigtable_protos gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)
target_include_directories(bigtable_protos PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>)
target_compile_options(bigtable_protos PUBLIC ${GOOGLE_CLOUD_CPP_EXCEPTIONS_FLAG})
add_library(bigtable::protos ALIAS bigtable_protos)

# Enable unit tests
enable_testing()

# Capture the compiler version and the git revision into variables, then
# generate a config file with the values.
if (IS_DIRECTORY ${PROJECT_SOURCE_DIR}/.git)
    execute_process(COMMAND git rev-parse --short HEAD
            OUTPUT_VARIABLE GIT_HEAD_LOG ERROR_VARIABLE GIT_HEAD_LOG)
else ()
    set(GIT_HEAD_LOG "(unknown-git-commit)")
endif ()
string(REPLACE "\n" "" GIT_HEAD ${GIT_HEAD_LOG})
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
configure_file(client/build_info.cc.in client/build_info.cc)

include(IncludeGMock)

add_dependencies(skip-scanbuild-targets gmock bigtable_protos)

# All tests get added to this target below.
add_subdirectory(client)
add_subdirectory(tests)

if (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)
    add_subdirectory(benchmarks)
    add_subdirectory(examples)
endif (GOOGLE_CLOUD_CPP_ENABLE_CXX_EXCEPTIONS)

# Install the libraries and headers in the locations determined by
# GNUInstallDirs
install(TARGETS bigtable_protos bigtable_common_options
    EXPORT bigtable-targets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

# Install proto generated files into bigtable/google.
install(DIRECTORY ${CMAKE_BINARY_DIR}/bigtable/google/ DESTINATION include/google FILES_MATCHING PATTERN "*.h")

# The exports can only be installed if all the dependencies are installed.
# CMake needs to know where the submodules will be installed from,
if (NOT ${GOOGLE_CLOUD_CPP_GRPC_PROVIDER} STREQUAL "module")
    # Export the CMake targets to make it easy to create configuration files.
    install(EXPORT bigtable-targets DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake)
endif ()

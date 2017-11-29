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

# Configure the Compiler options, we will be using C++11 features.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Turn on all available warnings and turn warnings into errors to
# stop the build if any warning is emitted.  We do this in each
# subdirectory because setting this for the top-level CMakeLists
# breaks submodules (such as gRPC) that do not conform to such an
# strict setting.
include(CheckCXXCompilerFlag)
if(NOT MSVC)
  CHECK_CXX_COMPILER_FLAG(-Werror COMPILER_SUPPORTS_WERROR)
  if(COMPILER_SUPPORTS_WERROR)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
  endif()
  CHECK_CXX_COMPILER_FLAG(-Wall COMPILER_SUPPORTS_WALL)
  if(COMPILER_SUPPORTS_WALL)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
  endif()
endif()

include_directories(${GRPCPP_INCLUDE_DIRS} ${GRPC_INCLUDE_DIRS} ${PROTOBUF_INCLUDE_DIRS})
set(PROTOBUF_IMPORT_DIRS "${PROJECT_THIRD_PARTY_DIR}/googleapis" "${PROJECT_SOURCE_DIR}")
if(GRPC_ROOT_DIR)
  list(INSERT PROTOBUF_IMPORT_DIRS 0 "${GRPC_ROOT_DIR}/third_party/protobuf/src")
  include_directories("${GRPC_ROOT_DIR}/third_party/protobuf/src")
  include_directories("${GRPC_ROOT_DIR}/third_party/boringssl/third_party/googletest")
endif(GRPC_ROOT_DIR)

# We use abseil.io .
include_directories(${PROJECT_THIRD_PARTY_DIR}/abseil)

################################################################
# Create targets here ...

# Include the functions to compile proto files.
include(${PROJECT_SOURCE_DIR}/cmake/CompileProtos.cmake)

set(PROTOBUF_IMPORT_DIRS "${PROJECT_THIRD_PARTY_DIR}/googleapis" "${PROJECT_SOURCE_DIR}")
if(GRPC_ROOT_DIR)
  list(INSERT PROTOBUF_IMPORT_DIRS 0 "${GRPC_ROOT_DIR}/third_party/protobuf/src")
  include_directories("${GRPC_ROOT_DIR}/third_party/protobuf/src")
  include_directories("${GRPC_ROOT_DIR}/third_party/boringssl/third_party/googletest")
endif(GRPC_ROOT_DIR)
PROTOBUF_GENERATE_CPP(PROTO_SOURCES PROTO_HDRS
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_instance_admin.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_table_admin.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/common.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/instance.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/table.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/v2/bigtable.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/v2/data.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/longrunning/operations.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/rpc/status.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/rpc/error_details.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/api/annotations.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/api/auth.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/api/http.proto)
GRPC_GENERATE_CPP_MOCKS(GRPCPP_SOURCES GRPCPP_HDRS GRPC_MOCK_HDRS
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_instance_admin.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/admin/v2/bigtable_table_admin.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/bigtable/v2/bigtable.proto
        ${PROJECT_THIRD_PARTY_DIR}/googleapis/google/longrunning/operations.proto)
include_directories("${PROJECT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}")

# Create a library with the generated files from the relevant protos.
add_library(googleapis ${PROTO_SOURCES} ${PROTO_HDRS} ${GRPCPP_SOURCES} ${GRPCPP_HDRS})
target_link_libraries(googleapis ${GRPCPP_LIBRARIES} ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})

# Enable unit tests
enable_testing()

# the client library
add_library(bigtable_data_api
  client/client_options.h
  client/client_options.cc
  client/data.h
  client/data.cc
  client/rpc_retry_policy.h
  client/rpc_retry_policy.cc
  client/mutations.h
  client/mutations.cc
  )
target_link_libraries(bigtable_data_api googleapis ${GRPCPP_LIBRARIES}
  ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})

# List the unit tests, then setup the targets and dependencies.
set(all_unit_tests
        bigtable_client_client_options_test
        bigtable_client_mutations_test
        bigtable_client_table_apply_test
        bigtable_client_rpc_retry_policy_test)
foreach(target ${all_unit_tests})
    string(REGEX MATCH ".*_test$" is_ut ${target})
    if(is_ut)
        string(REGEX REPLACE "^bigtable_client_" "client/" fname ${target})
        add_executable(${target} ${fname}.cc
          ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/src/gtest_main.cc
          ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/src/gtest-all.cc
          ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock/src/gmock-all.cc
          )
        get_target_property(tname ${target} NAME)
        target_include_directories(${target}
          PRIVATE ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest/include
          PRIVATE ${PROJECT_THIRD_PARTY_DIR}/googletest/googletest
          PRIVATE ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock/include
          PRIVATE ${PROJECT_THIRD_PARTY_DIR}/googletest/googlemock
        )
        target_link_libraries(${target} bigtable_data_api ${GRPCPP_LIBRARIES}
          ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})
        add_test(${tname} ${target})
        get_target_property(sources ${target} SOURCES)
    endif()
endforeach()

# Copyright 2018 Google Inc.
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

cmake_minimum_required(VERSION 3.5)
project(grpc-crash CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("/std:c++latest" _cpp_latest_flag_supported)
    if (_cpp_latest_flag_supported)
        add_compile_options("/std:c++latest")
    endif ()
endif ()

if (NOT GOOGLE_CLOUD_CPP_GRPC_PROVIDER)
    if (NOT GRPC_ROOT_DIR)
        set(GRPC_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/grpc)
    endif ()
    if (NOT EXISTS "${GRPC_ROOT_DIR}/CMakeLists.txt")
        message(ERROR "GRPC_ROOT_DIR does not have a CMake file.")
    endif ()
    add_subdirectory(${GRPC_ROOT_DIR} ext/grpc EXCLUDE_FROM_ALL)
    add_library(gRPC::grpc++ ALIAS grpc++)
    add_library(gRPC::grpc ALIAS grpc)
    add_library(protobuf::libprotobuf ALIAS libprotobuf)

    # The binary name is different on some platforms, use CMake magic to get it.
    set(PROTOBUF_PROTOC_EXECUTABLE $<TARGET_FILE:protoc>)
    mark_as_advanced(PROTOBUF_PROTOC_EXECUTABLE)
    set(PROTOC_GRPCPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
    mark_as_advanced(PROTOC_GRPCPP_PLUGIN_EXECUTABLE)
endif ()

# Create a library with the protos for the test.
add_custom_command(
        OUTPUT "echo.pb.h" "echo.pb.cc"
        COMMAND "${PROTOBUF_PROTOC_EXECUTABLE}"
        ARGS
        --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
        -I${CMAKE_CURRENT_SOURCE_DIR}
        echo.proto
        DEPENDS
        echo.proto
        "${PROTOBUF_PROTOC_EXECUTABLE}"
        COMMENT "Running (local) C++ protocol buffer compiler on ${FIL}"
        VERBATIM )
add_custom_command(
        OUTPUT "echo.grpc.pb.h" "echo.grpc.pb.cc"
        COMMAND "${PROTOBUF_PROTOC_EXECUTABLE}"
        ARGS
            --plugin=protoc-gen-grpc=${PROTOC_GRPCPP_PLUGIN_EXECUTABLE}
            --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
            --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
            -I${CMAKE_CURRENT_SOURCE_DIR}
            echo.proto
        DEPENDS
            echo.proto
            "${PROTOBUF_PROTOC_EXECUTABLE}" "${PROTOC_GRPCPP_PLUGIN_EXECUTABLE}"
        COMMENT "Running (local) C++ protocol buffer compiler on ${FIL}"
        VERBATIM )
add_library(grpc_round_robin_freeze_protos
        "echo.pb.h" "echo.pb.cc"
        "echo.grpc.pb.h" "echo.grpc.pb.cc")
target_link_libraries(grpc_round_robin_freeze_protos gRPC::grpc++ gRPC::grpc protobuf::libprotobuf)
target_include_directories(grpc_round_robin_freeze_protos PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

add_executable(grpc_round_robin_freeze_server server.cc)
target_link_libraries(grpc_round_robin_freeze_server grpc_round_robin_freeze_protos)

add_executable(grpc_round_robin_freeze_client client.cc)
target_link_libraries(grpc_round_robin_freeze_client grpc_round_robin_freeze_protos)

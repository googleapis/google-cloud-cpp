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

cmake_minimum_required(VERSION 3.18)
project(hello-world-grpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(gRPC REQUIRED)

add_executable(
    hello_world_grpc
    "${CMAKE_CURRENT_BINARY_DIR}/hello_world.grpc.pb.cc"
    "${CMAKE_CURRENT_BINARY_DIR}/hello_world.pb.cc" hello_world_grpc.cc)
target_include_directories(hello_world_grpc
                           PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(hello_world_grpc PRIVATE gRPC::grpc++)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/hello_world.grpc.pb.cc"
           "${CMAKE_CURRENT_BINARY_DIR}/hello_world.grpc.pb.h"
           "${CMAKE_CURRENT_BINARY_DIR}/hello_world.pb.cc"
           "${CMAKE_CURRENT_BINARY_DIR}/hello_world.pb.h"
    COMMAND
        $<TARGET_FILE:protobuf::protoc> ARGS
        --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
        "--grpc_out=${CMAKE_CURRENT_BINARY_DIR}"
        "--cpp_out=${CMAKE_CURRENT_BINARY_DIR}"
        "--proto_path=${CMAKE_SOURCE_DIR}" "hello_world.proto"
    DEPENDS "hello_world.proto" protobuf::protoc gRPC::grpc_cpp_plugin
    VERBATIM)
set_source_files_properties(
    "${CMAKE_CURRENT_BINARY_DIR}/hello_world.grpc.pb.cc"
    "${CMAKE_CURRENT_BINARY_DIR}/hello_world.grpc.pb.h"
    "${CMAKE_CURRENT_BINARY_DIR}/hello_world.pb.cc"
    "${CMAKE_CURRENT_BINARY_DIR}/hello_world.pb.h"
    PROPERTIES GENERATED TRUE)

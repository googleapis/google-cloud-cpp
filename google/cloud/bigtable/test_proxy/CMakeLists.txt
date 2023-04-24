# Copyright 2023 Google LLC
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

include(CompileProtos)
google_cloud_cpp_find_proto_include_dir(PROTO_INCLUDE_DIR)

google_cloud_cpp_grpcpp_library(
    google_cloud_cpp_bigtable_test_proxy_protos
    test_proxy.proto
    PROTO_PATH_DIRECTORIES
    ${PROTO_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/google/cloud/bigtable/test_proxy
    ${PROJECT_BINARY_DIR}/external/googleapis/src/googleapis_download)
target_link_libraries(
    google_cloud_cpp_bigtable_test_proxy_protos
    PUBLIC google-cloud-cpp::api_client_protos
           google-cloud-cpp::bigtable_protos
           google-cloud-cpp::rpc_status_protos)
set_target_properties(google_cloud_cpp_bigtable_test_proxy_protos
                      PROPERTIES CXX_CLANG_TIDY "")

add_library(
    cbt_test_proxy
    cbt_test_proxy.cc
    cbt_test_proxy.h)
target_link_libraries(
    cbt_test_proxy
    google_cloud_cpp_bigtable_test_proxy_protos
    google-cloud-cpp::bigtable
    google-cloud-cpp::bigtable_protos
    google-cloud-cpp::common
    google-cloud-cpp::grpc_utils
    gRPC::grpc++
    gRPC::grpc
    protobuf::libprotobuf)

google_cloud_cpp_add_executable(target "bigtable_test_proxy" "cbt_test_proxy_main.cc")
target_link_libraries(
    ${target}
    PRIVATE cbt_test_proxy
            google-cloud-cpp::bigtable
            google-cloud-cpp::bigtable_protos
            google-cloud-cpp::common
            google-cloud-cpp::grpc_utils
            gRPC::grpc++
            gRPC::grpc
            protobuf::libprotobuf)
google_cloud_cpp_add_common_options(${target})
if (MSVC)
    target_compile_options(${target} PRIVATE "/bigobj")
endif ()

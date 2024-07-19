# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.


add_library(cbt_test_proxy cbt_test_proxy.cc cbt_test_proxy.h)
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

google_cloud_cpp_add_executable(target "bigtable_test_proxy"
                                "cbt_test_proxy_main.cc")
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

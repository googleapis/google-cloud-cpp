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

find_package(gRPC)

add_executable(common_install_test common_install_test.cc)
target_link_libraries(
    common_install_test PRIVATE google_cloud_cpp_grpc_utils
                                google_cloud_cpp_common gRPC::grpc++ gRPC::grpc)
add_test(NAME common_install_test COMMAND common_install_test)

# ~~~
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
# ~~~

find_package(google_cloud_cpp_common CONFIG REQUIRED)
find_package(google_cloud_cpp_grpc_utils CONFIG REQUIRED)

# Pick the right MSVC runtime libraries.
include(SelectMSVCRuntime)

add_executable(gcs2cbt gcs2cbt.cc)
target_link_libraries(
    gcs2cbt bigtable_client storage_client google_cloud_cpp_grpc_utils
    google_cloud_cpp_common_options)

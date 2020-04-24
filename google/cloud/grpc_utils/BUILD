# Copyright 2020 Google LLC
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

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

# TODO(googleapis/google-cloud-cpp-common#171) - remove the backwards
#     compatibility target.
cc_library(
    name = "google_cloud_cpp_grpc_utils",
    srcs = [],
    hdrs = [
        "async_operation.h",
        "completion_queue.h",
        "grpc_error_delegate.h",
        "version.h",
    ],
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
    ],
)

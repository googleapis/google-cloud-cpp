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

load(":logging_client.bzl", "logging_client_hdrs", "logging_client_srcs")

cc_library(
    name = "logging_client",
    srcs = logging_client_srcs,
    hdrs = logging_client_hdrs,
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//google/logging/v2:logging_cc_grpc",
    ],
)

load(":logging_client_mocks.bzl", "logging_client_mocks_hdrs", "logging_client_mocks_srcs")

cc_library(
    name = "logging_client_mocks",
    srcs = logging_client_mocks_srcs,
    hdrs = logging_client_mocks_hdrs,
    deps = [
        ":logging_client",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
    ],
)

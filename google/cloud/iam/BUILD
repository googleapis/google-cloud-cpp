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

load(":iam_client.bzl", "iam_client_hdrs", "iam_client_srcs")

cc_library(
    name = "iam_client",
    srcs = iam_client_srcs,
    hdrs = iam_client_hdrs,
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//google/iam/credentials/v1:credentials_cc_grpc",
    ],
)

load(":iam_client_mocks.bzl", "iam_client_mocks_hdrs", "iam_client_mocks_srcs")

cc_library(
    name = "iam_client_mocks",
    srcs = iam_client_mocks_srcs,
    hdrs = iam_client_mocks_hdrs,
    deps = [
        ":iam_client",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//google/iam/credentials/v1:credentials_cc_grpc",
    ],
)

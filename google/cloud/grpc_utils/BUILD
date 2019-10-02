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

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

load(":google_cloud_cpp_grpc_utils.bzl", "google_cloud_cpp_grpc_utils_hdrs", "google_cloud_cpp_grpc_utils_srcs")

cc_library(
    name = "google_cloud_cpp_grpc_utils",
    srcs = google_cloud_cpp_grpc_utils_srcs,
    hdrs = google_cloud_cpp_grpc_utils_hdrs,
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googleapis//:grpc_utils_protos",
    ],
)

load(":google_cloud_cpp_grpc_utils_unit_tests.bzl", "google_cloud_cpp_grpc_utils_unit_tests")

[cc_test(
    name = "google_cloud_cpp_grpc_utils_" + test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":google_cloud_cpp_grpc_utils",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googleapis//:grpc_utils_protos",
        "@com_google_googletest//:gtest",
    ],
) for test in google_cloud_cpp_grpc_utils_unit_tests]

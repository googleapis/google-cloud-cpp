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

load(":google_cloud_cpp_testing.bzl", "google_cloud_cpp_testing_hdrs", "google_cloud_cpp_testing_srcs")

cc_library(
    name = "google_cloud_cpp_testing",
    srcs = google_cloud_cpp_testing_srcs,
    hdrs = google_cloud_cpp_testing_hdrs,
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "@com_google_googletest//:gtest_main",
    ],
)

load(":google_cloud_cpp_testing_unit_tests.bzl", "google_cloud_cpp_testing_unit_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":google_cloud_cpp_testing",
        "//google/cloud:google_cloud_cpp_common",
        "@com_google_googletest//:gtest_main",
    ],
) for test in google_cloud_cpp_testing_unit_tests]

load(":google_cloud_cpp_testing_grpc.bzl", "google_cloud_cpp_testing_grpc_hdrs", "google_cloud_cpp_testing_grpc_srcs")

cc_library(
    name = "google_cloud_cpp_testing_grpc",
    srcs = google_cloud_cpp_testing_grpc_srcs,
    hdrs = google_cloud_cpp_testing_grpc_hdrs,
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

load(":google_cloud_cpp_testing_grpc_unit_tests.bzl", "google_cloud_cpp_testing_grpc_unit_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":google_cloud_cpp_testing_grpc",
        "//google/cloud:google_cloud_cpp_common",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
) for test in google_cloud_cpp_testing_grpc_unit_tests]

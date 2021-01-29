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

# TODO(#1415) Change this visibility to "//:__subpackages__" so that users are
# required to use the top-level BUILD file rather than reaching down into this
# one.
package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

load(":google_cloud_cpp_spanner.bzl", "google_cloud_cpp_spanner_hdrs", "google_cloud_cpp_spanner_srcs")

cc_library(
    name = "google_cloud_cpp_spanner",
    srcs = google_cloud_cpp_spanner_srcs,
    hdrs = google_cloud_cpp_spanner_hdrs,
    visibility = [
        "//:__pkg__",
        "//google/cloud/spanner:__subpackages__",
    ],
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_absl//absl/container:fixed_array",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/longrunning:longrunning_cc_grpc",
        "@com_google_googleapis//google/spanner/admin/database/v1:database_cc_grpc",
        "@com_google_googleapis//google/spanner/admin/instance/v1:instance_cc_grpc",
        "@com_google_googleapis//google/spanner/v1:spanner_cc_grpc",
    ],
)

cc_library(
    name = "spanner_client",
    deprecation = "this target will be removed on or around 2022-02-15, please use //:spanner instead.",
    tags = ["manual"],
    deps = [
        ":google_cloud_cpp_spanner",
    ],
)

load(":google_cloud_cpp_spanner_mocks.bzl", "google_cloud_cpp_spanner_mocks_hdrs", "google_cloud_cpp_spanner_mocks_srcs")

cc_library(
    name = "google_cloud_cpp_spanner_mocks",
    srcs = google_cloud_cpp_spanner_mocks_srcs,
    hdrs = google_cloud_cpp_spanner_mocks_hdrs,
    visibility = [
        "//:__pkg__",
        "//google/cloud/spanner:__subpackages__",
    ],
    deps = [
        ":google_cloud_cpp_spanner",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "spanner_client_mocks",
    deprecation = "this target will be removed on or around 2022-02-15, please use //:spanner_mocks instead.",
    tags = ["manual"],
    deps = [
        ":google_cloud_cpp_spanner_mocks",
    ],
)

load(":spanner_client_testing.bzl", "spanner_client_testing_hdrs", "spanner_client_testing_srcs")

cc_library(
    name = "spanner_client_testing",
    srcs = spanner_client_testing_srcs,
    hdrs = spanner_client_testing_hdrs,
    deps = [
        ":google_cloud_cpp_spanner",
        ":google_cloud_cpp_spanner_mocks",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest_main",
    ],
)

load(":spanner_client_unit_tests.bzl", "spanner_client_unit_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":google_cloud_cpp_spanner",
        ":google_cloud_cpp_spanner_mocks",
        ":spanner_client_testing",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_googletest//:gtest_main",
    ],
) for test in spanner_client_unit_tests]

load(":spanner_client_benchmarks.bzl", "spanner_client_benchmarks")

[cc_test(
    name = benchmark.replace("/", "_").replace(".cc", ""),
    srcs = [benchmark],
    tags = ["benchmark"],
    deps = [
        "//:spanner",
        "//google/cloud:google_cloud_cpp_common",
        "@com_google_benchmark//:benchmark_main",
    ],
) for benchmark in spanner_client_benchmarks]

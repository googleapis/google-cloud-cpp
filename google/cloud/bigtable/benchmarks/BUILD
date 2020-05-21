# Copyright 2018 Google LLC
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

load(":bigtable_benchmark_common.bzl", "bigtable_benchmark_common_hdrs", "bigtable_benchmark_common_srcs")

cc_library(
    name = "bigtable_benchmark_common",
    srcs = bigtable_benchmark_common_srcs,
    hdrs = bigtable_benchmark_common_hdrs,
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/bigtable:bigtable_client",
    ],
)

load(":bigtable_benchmark_programs.bzl", "bigtable_benchmark_programs")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    linkopts = select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": ["-lpthread"],
    }),
    tags = [
        "integration-test",
    ],
    deps = [
        ":bigtable_benchmark_common",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/bigtable:bigtable_client",
    ],
) for test in bigtable_benchmark_programs]

load(":bigtable_benchmarks_unit_tests.bzl", "bigtable_benchmarks_unit_tests")

[cc_test(
    name = "bigtable_benchmark_" + test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    linkopts = select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": ["-lpthread"],
    }),
    deps = [
        ":bigtable_benchmark_common",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/bigtable:bigtable_client",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest_main",
    ],
) for test in bigtable_benchmarks_unit_tests]

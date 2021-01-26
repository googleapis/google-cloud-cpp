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

load(":spanner_client_benchmarks.bzl", "spanner_client_benchmarks_hdrs", "spanner_client_benchmarks_srcs")
load(":spanner_client_benchmark_programs.bzl", "spanner_client_benchmark_programs")

cc_library(
    name = "spanner_client_benchmarks",
    srcs = spanner_client_benchmarks_srcs,
    hdrs = spanner_client_benchmarks_hdrs,
    deps = [
        "//:spanner",
        "//:spanner_mocks",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/spanner:spanner_client_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest_main",
    ],
)

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    timeout = "long",
    srcs = [test],
    tags = [
        "integration-test",
    ],
    deps = [
        ":spanner_client_benchmarks",
        "//:spanner",
        "//:spanner_mocks",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/spanner:spanner_client_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest_main",
    ],
) for test in spanner_client_benchmark_programs]

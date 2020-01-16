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

load(":storage_benchmarks.bzl", "storage_benchmarks_hdrs", "storage_benchmarks_srcs")

cc_library(
    name = "storage_benchmarks",
    srcs = storage_benchmarks_srcs,
    hdrs = storage_benchmarks_hdrs,
    deps = [
        "//google/cloud/storage:nlohmann_json",
        "//google/cloud/storage:storage_client",
        "//google/cloud/storage:storage_client_testing",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@com_github_curl_curl//:curl",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
    ],
)

load(":storage_benchmark_programs.bzl", "storage_benchmark_programs")

[cc_binary(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    linkopts = select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": ["-lpthread"],
    }),
    deps = [
        ":storage_benchmarks",
        "//google/cloud/storage:nlohmann_json",
        "//google/cloud/storage:storage_client",
        "//google/cloud/storage:storage_client_testing",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@com_github_curl_curl//:curl",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
    ],
) for test in storage_benchmark_programs]

load(":storage_benchmarks_unit_tests.bzl", "storage_benchmarks_unit_tests")

[cc_test(
    name = "storage_benchmark_" + test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    linkopts = select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": ["-lpthread"],
    }),
    deps = [
        ":storage_benchmarks",
        "//google/cloud/storage:storage_client",
        "//google/cloud/storage:storage_client_testing",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@com_github_curl_curl//:curl",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
) for test in storage_benchmarks_unit_tests]

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

genrule(
    name = "nlohmann_json_include_hierarchy",
    srcs = ["@com_github_nlohmann_json_single_header//file"],
    outs = ["google/cloud/storage/internal/nlohmann_json.hpp"],
    cmd = "sed '20213,20247d' $< >$@",
)

cc_library(
    name = "nlohmann_json",
    hdrs = [":nlohmann_json_include_hierarchy"],
    includes = ["."],
    deps = [],
)

load(":storage_client.bzl", "storage_client_hdrs", "storage_client_srcs")

cc_library(
    name = "storage_client",
    srcs = storage_client_srcs,
    hdrs = storage_client_hdrs,
    deps = [
        ":nlohmann_json",
        "//google/cloud:google_cloud_cpp_common",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@com_github_curl_curl//:curl",
        "@com_github_google_crc32c//:crc32c",
    ],
)

load(":storage_client_testing.bzl", "storage_client_testing_hdrs", "storage_client_testing_srcs")

cc_library(
    name = "storage_client_testing",
    srcs = storage_client_testing_srcs,
    hdrs = storage_client_testing_hdrs,
    deps = [
        ":nlohmann_json",
        ":storage_client",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@com_github_curl_curl//:curl",
        "@com_google_googletest//:gtest",
    ],
)

load(":storage_client_unit_tests.bzl", "storage_client_unit_tests")

[cc_test(
    name = "storage_client_" + test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    # TODO(#664 / #666) - use the right condition when porting Bazel builds
    linkopts = ["-lpthread"],
    deps = [
        ":nlohmann_json",
        ":storage_client",
        ":storage_client_testing",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@com_github_curl_curl//:curl",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
) for test in storage_client_unit_tests]

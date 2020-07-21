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

# TODO(#3701) Change this visibility to "//:__subpackages__" so that users are
# required to use the top-level BUILD file rather than reaching down into this
# one.
package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

GOOGLE_CLOUD_STORAGE_WIN_COPTS = [
    "/DWIN32_LEAN_AND_MEAN",
]

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
load(":storage_client_grpc.bzl", "storage_client_grpc_hdrs", "storage_client_grpc_srcs")

cc_library(
    name = "storage_client_grpc",
    srcs = storage_client_grpc_srcs,
    hdrs = storage_client_grpc_hdrs,
    copts = select({
        "@bazel_tools//src/conditions:windows": GOOGLE_CLOUD_STORAGE_WIN_COPTS,
        "//conditions:default": [],
    }),
    defines = ["GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC"],
    deps = [
        ":nlohmann_json",
        ":storage_client",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@com_github_curl_curl//:curl",
        "@com_github_google_crc32c//:crc32c",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/storage/v1:storage_cc_grpc",
        "@com_google_googleapis//google/storage/v1:storage_cc_proto",
    ],
)

cc_library(
    name = "storage_client",
    srcs = storage_client_srcs,
    hdrs = storage_client_hdrs,
    copts = select({
        "@bazel_tools//src/conditions:windows": GOOGLE_CLOUD_STORAGE_WIN_COPTS,
        "//conditions:default": [],
    }),
    deps = [
        ":nlohmann_json",
        "//google/cloud:google_cloud_cpp_common",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@com_github_curl_curl//:curl",
        "@com_github_google_crc32c//:crc32c",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

load(":storage_client_testing.bzl", "storage_client_testing_hdrs", "storage_client_testing_srcs")

cc_library(
    name = "storage_client_testing",
    srcs = storage_client_testing_srcs,
    hdrs = storage_client_testing_hdrs,
    copts = select({
        "@bazel_tools//src/conditions:windows": GOOGLE_CLOUD_STORAGE_WIN_COPTS,
        "//conditions:default": [],
    }),
    deps = [
        ":nlohmann_json",
        ":storage_client",
        ":storage_client_grpc",
        "//google/cloud:google_cloud_cpp_common",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@com_github_curl_curl//:curl",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ],
)

load(":storage_client_unit_tests.bzl", "storage_client_unit_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    copts = select({
        "@bazel_tools//src/conditions:windows": GOOGLE_CLOUD_STORAGE_WIN_COPTS,
        "//conditions:default": [],
    }),
    linkopts = select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": ["-lpthread"],
    }),
    deps = [
        ":nlohmann_json",
        ":storage_client",
        ":storage_client_testing",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@com_github_curl_curl//:curl",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ],
) for test in storage_client_unit_tests]

load(":storage_client_grpc_unit_tests.bzl", "storage_client_grpc_unit_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    copts = select({
        "@bazel_tools//src/conditions:windows": GOOGLE_CLOUD_STORAGE_WIN_COPTS,
        "//conditions:default": [],
    }),
    linkopts = select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": ["-lpthread"],
    }),
    deps = [
        ":nlohmann_json",
        ":storage_client",
        ":storage_client_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@com_github_curl_curl//:curl",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
) for test in storage_client_grpc_unit_tests]

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

load(":google_cloud_cpp_storage.bzl", "google_cloud_cpp_storage_hdrs", "google_cloud_cpp_storage_srcs")
load(":google_cloud_cpp_storage_grpc.bzl", "google_cloud_cpp_storage_grpc_hdrs", "google_cloud_cpp_storage_grpc_srcs")

proto_library(
    name = "google_cloud_cpp_storage_internal_grpc_resumable_session_url_proto",
    srcs = [
        "internal/grpc_resumable_upload_session_url.proto",
    ],
)

cc_proto_library(
    name = "google_cloud_cpp_storage_internal_grpc_resumable_session_url_cc_proto",
    deps = [":google_cloud_cpp_storage_internal_grpc_resumable_session_url_proto"],
)

cc_library(
    name = "google_cloud_cpp_storage_grpc",
    srcs = google_cloud_cpp_storage_grpc_srcs,
    hdrs = google_cloud_cpp_storage_grpc_hdrs,
    copts = select({
        "@bazel_tools//src/conditions:windows": GOOGLE_CLOUD_STORAGE_WIN_COPTS,
        "//conditions:default": [],
    }),
    defines = ["GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC"],
    deps = [
        ":google_cloud_cpp_storage",
        ":google_cloud_cpp_storage_internal_grpc_resumable_session_url_cc_proto",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@com_github_curl_curl//:curl",
        "@com_github_google_crc32c//:crc32c",
        "@com_github_nlohmann_json//:nlohmann_json",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/storage/v1:storage_cc_grpc",
        "@com_google_googleapis//google/storage/v1:storage_cc_proto",
    ],
)

cc_library(
    name = "google_cloud_cpp_storage",
    srcs = google_cloud_cpp_storage_srcs,
    hdrs = google_cloud_cpp_storage_hdrs,
    copts = select({
        "@bazel_tools//src/conditions:windows": GOOGLE_CLOUD_STORAGE_WIN_COPTS,
        "//conditions:default": [],
    }),
    visibility = [
        "//:__pkg__",
        "//google/cloud/storage:__subpackages__",
    ],
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@com_github_curl_curl//:curl",
        "@com_github_google_crc32c//:crc32c",
        "@com_github_nlohmann_json//:nlohmann_json",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_library(
    name = "storage_client",
    deprecation = "this target will be removed on or around 2022-02-15, please use //:storage instead.",
    tags = ["manual"],
    deps = [
        "//google/cloud/storage:google_cloud_cpp_storage",
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
        ":google_cloud_cpp_storage",
        ":google_cloud_cpp_storage_grpc",
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
        ":google_cloud_cpp_storage",
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
        ":google_cloud_cpp_storage",
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

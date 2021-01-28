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

load(":google_cloud_cpp_pubsub.bzl", "google_cloud_cpp_pubsub_hdrs", "google_cloud_cpp_pubsub_srcs")

cc_library(
    name = "google_cloud_cpp_pubsub",
    srcs = google_cloud_cpp_pubsub_srcs,
    hdrs = google_cloud_cpp_pubsub_hdrs,
    visibility = [
        "//:__pkg__",
        "//google/cloud/pubsub:__subpackages__",
    ],
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googleapis//google/pubsub/v1:pubsub_cc_grpc",
    ],
)

cc_library(
    name = "pubsub_client",
    deprecation = "this target will be removed on or around 2022-02-15, please use //:pubsub instead.",
    tags = ["manual"],
    deps = [
        "//google/cloud/pubsub:google_cloud_cpp_pubsub",
    ],
)

load(":pubsub_client_testing.bzl", "pubsub_client_testing_hdrs", "pubsub_client_testing_srcs")

cc_library(
    name = "pubsub_client_testing",
    srcs = pubsub_client_testing_srcs,
    hdrs = pubsub_client_testing_hdrs,
    deps = [
        ":google_cloud_cpp_pubsub",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//google/pubsub/v1:pubsub_cc_grpc",
        "@com_google_googletest//:gtest",
    ],
)

load(":google_cloud_cpp_pubsub_mocks.bzl", "google_cloud_cpp_pubsub_mocks_hdrs", "google_cloud_cpp_pubsub_mocks_srcs")

cc_library(
    name = "google_cloud_cpp_pubsub_mocks",
    srcs = google_cloud_cpp_pubsub_mocks_srcs,
    hdrs = google_cloud_cpp_pubsub_mocks_hdrs,
    visibility = [
        "//:__pkg__",
        "//google/cloud/pubsub:__subpackages__",
    ],
    deps = [
        ":google_cloud_cpp_pubsub",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//google/pubsub/v1:pubsub_cc_grpc",
    ],
)

cc_library(
    name = "pubsub_client_mocks",
    deprecation = "this target will be removed on or around 2022-02-15, please use //:pubsub_mocks instead.",
    tags = ["manual"],
    deps = [
        "//google/cloud/pubsub:google_cloud_cpp_pubsub_mocks",
    ],
)

load(":pubsub_client_unit_tests.bzl", "pubsub_client_unit_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":google_cloud_cpp_pubsub",
        ":google_cloud_cpp_pubsub_mocks",
        ":pubsub_client_testing",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
    ],
) for test in pubsub_client_unit_tests]

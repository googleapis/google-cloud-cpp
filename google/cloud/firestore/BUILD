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

package(default_visibility = ["//visibility:private"])

licenses(["notice"])  # Apache 2.0

load(":google_cloud_cpp_firestore.bzl", "google_cloud_cpp_firestore_hdrs", "google_cloud_cpp_firestore_srcs")

cc_library(
    name = "google_cloud_cpp_firestore",
    srcs = google_cloud_cpp_firestore_srcs,
    hdrs = google_cloud_cpp_firestore_hdrs,
    visibility = ["//:__pkg__"],
    deps = [
        "//google/cloud:google_cloud_cpp_common",
    ],
)

cc_library(
    name = "firestore_client",
    deprecation = "this target will be removed on or around 2022-02-15, please use //:experimental_firestore instead.",
    tags = ["manual"],
    visibility = ["//visibility:public"],
    deps = [
        ":google_cloud_cpp_firestore",
    ],
)

load(":firestore_client_unit_tests.bzl", "firestore_client_unit_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    linkopts = select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": ["-lpthread"],
    }),
    deps = [
        ":google_cloud_cpp_firestore",
        "@com_google_googletest//:gtest_main",
    ],
) for test in firestore_client_unit_tests]

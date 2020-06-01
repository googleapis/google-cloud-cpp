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

load(":firestore_client.bzl", "firestore_client_hdrs", "firestore_client_srcs")

cc_library(
    name = "firestore_client",
    srcs = firestore_client_srcs,
    hdrs = firestore_client_hdrs,
    deps = [],
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
        ":firestore_client",
        "@com_google_googletest//:gtest_main",
    ],
) for test in firestore_client_unit_tests]

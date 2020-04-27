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

load(":pubsub_client.bzl", "pubsub_client_hdrs", "pubsub_client_srcs")

cc_library(
    name = "pubsub_client",
    srcs = pubsub_client_srcs,
    hdrs = pubsub_client_hdrs,
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//google/pubsub/v1:pubsub_cc_grpc",
    ],
)

load(":pubsub_client_unit_tests.bzl", "pubsub_client_unit_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":pubsub_client",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_google_googletest//:gtest_main",
    ],
) for test in pubsub_client_unit_tests]

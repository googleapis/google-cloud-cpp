# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

package(default_visibility = ["//visibility:private"])

licenses(["notice"])  # Apache 2.0

SOURCE_REGEX = "**/*.cc"
TEST_SOURCE_REGEX = "**/*_test.cc"
MOCK_SOURCE_REGEX = "mocks/*.cc"

HEADER_REGEX = "**/*.h"
MOCK_HEADER_REGEX = "mocks/*.h"


cc_library(
    name = "google_cloud_cpp_pubsublite",
    srcs = glob(include=[SOURCE_REGEX], exclude=[TEST_SOURCE_REGEX, MOCK_SOURCE_REGEX]),
    hdrs = glob(include=[HEADER_REGEX], exclude=[MOCK_HEADER_REGEX]),
    visibility = ["//:__pkg__"],
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//google/cloud/pubsublite/v1:pubsublite_cc_grpc",
    ],
)

load(":google_cloud_cpp_pubsublite_mocks.bzl", "google_cloud_cpp_pubsublite_mocks_hdrs", "google_cloud_cpp_pubsublite_mocks_srcs")

cc_library(
    name = "google_cloud_cpp_pubsublite_mocks",
    srcs = glob([MOCK_SOURCE_REGEX]),
    hdrs = glob([MOCK_HEADER_REGEX]),
    visibility = ["//:__pkg__"],
    deps = [
        ":google_cloud_cpp_pubsublite",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
    ],
)

[
    cc_test(
        name = test.replace("/", "_").replace(".cc", ""),
        srcs = [test],
    deps = [
        ":google_cloud_cpp_pubsublite",
        ":google_cloud_cpp_pubsublite_mocks",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_google_googletest//:gtest_main",
    ],
    ) for test in glob([TEST_SOURCE_REGEX])
]

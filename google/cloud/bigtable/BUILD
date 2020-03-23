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

load(":bigtable_client.bzl", "bigtable_client_hdrs", "bigtable_client_srcs")

cc_library(
    name = "bigtable_client",
    srcs = bigtable_client_srcs,
    hdrs = bigtable_client_hdrs,
    deps = [
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//:bigtable_protos",
    ],
)

load(":bigtable_client_testing.bzl", "bigtable_client_testing_hdrs", "bigtable_client_testing_srcs")

cc_library(
    name = "bigtable_client_testing",
    srcs = bigtable_client_testing_srcs,
    hdrs = bigtable_client_testing_hdrs,
    deps = [
        ":bigtable_client",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_google_googletest//:gtest",
    ],
)

load(":bigtable_client_unit_tests.bzl", "bigtable_client_unit_tests")

[cc_test(
    name = "bigtable_client_" + test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":bigtable_client",
        ":bigtable_client_testing",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_google_googletest//:gtest",
    ],
) for test in bigtable_client_unit_tests]

cc_test(
    name = "bigtable_client_internal_readrowsparser_test",
    srcs = [
        "internal/readrowsparser_acceptance_tests.inc",
        "internal/readrowsparser_test.cc",
    ],
    deps = [
        ":bigtable_client",
        ":bigtable_client_testing",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_google_googletest//:gtest",
    ],
)

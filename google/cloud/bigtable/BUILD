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

load(":bigtable_client.bzl", "bigtable_client_hdrs", "bigtable_client_srcs")

cc_library(
    name = "bigtable_client",
    srcs = bigtable_client_srcs,
    hdrs = bigtable_client_hdrs,
    # Do not sort: grpc++ must come last
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//google/bigtable/admin/v2:admin_cc_grpc",
        "@com_google_googleapis//google/bigtable/v2:bigtable_cc_grpc",
        "@com_google_googleapis//google/longrunning:longrunning_cc_grpc",
        "@com_google_googleapis//google/rpc:error_details_cc_proto",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

load(":bigtable_client_testing.bzl", "bigtable_client_testing_hdrs", "bigtable_client_testing_srcs")

cc_library(
    name = "bigtable_client_testing",
    srcs = bigtable_client_testing_srcs,
    hdrs = bigtable_client_testing_hdrs,
    deps = [
        ":bigtable_client",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_google_googletest//:gtest_main",
    ],
)

load(":bigtable_client_unit_tests.bzl", "bigtable_client_unit_tests")

[cc_test(
    name = "bigtable_client_" + test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":bigtable_client",
        ":bigtable_client_testing",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_google_googletest//:gtest_main",
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
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_google_googletest//:gtest_main",
    ],
)

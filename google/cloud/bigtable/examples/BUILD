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

load(":bigtable_examples_common.bzl", "bigtable_examples_common_hdrs", "bigtable_examples_common_srcs")

cc_library(
    name = "bigtable_examples_common",
    srcs = bigtable_examples_common_srcs,
    hdrs = bigtable_examples_common_hdrs,
    deps = [
        "//:bigtable",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googleapis//google/bigtable/admin/v2:admin_cc_grpc",
        "@com_google_googleapis//google/bigtable/v2:bigtable_cc_grpc",
    ],
)

load(":bigtable_examples_unit_tests.bzl", "bigtable_examples_unit_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":bigtable_examples_common",
        "//:bigtable",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "//google/cloud/bigtable:bigtable_client_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest_main",
    ],
) for test in bigtable_examples_unit_tests]

load(":bigtable_examples.bzl", "bigtable_examples")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    tags = [
        "integration-test",
    ],
    deps = [
        ":bigtable_examples_common",
        "//:bigtable",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_google_googletest//:gtest_main",
    ],
) for test in bigtable_examples]

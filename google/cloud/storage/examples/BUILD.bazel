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

load(":storage_examples_common.bzl", "storage_examples_common_hdrs", "storage_examples_common_srcs")

cc_library(
    name = "storage_examples_common",
    srcs = storage_examples_common_srcs,
    hdrs = storage_examples_common_hdrs,
    deps = [
        "//:storage",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/storage:storage_client_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
    ],
)

load(":storage_examples_unit_tests.bzl", "storage_examples_unit_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":storage_examples_common",
        "//:storage",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/storage:storage_client_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest_main",
    ],
) for test in storage_examples_unit_tests]

load(":storage_examples.bzl", "storage_examples")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    tags = [
        "integration-test",
    ],
    deps = [
        ":storage_examples_common",
        "//:storage",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/storage:google_cloud_cpp_storage_grpc",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest_main",
    ],
) for test in storage_examples]

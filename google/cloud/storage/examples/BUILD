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
        "//google/cloud/storage:storage_client",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
    ],
)

load(":storage_examples_unit_tests.bzl", "storage_examples_unit_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":storage_examples_common",
        "//google/cloud/storage:storage_client",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest",
    ],
) for test in storage_examples_unit_tests]

load(":storage_examples.bzl", "storage_examples")

# TODO(#3523) - add auto-runnable examples to this list until all
# examples are auto-runnable and then cleanup this code.
auto_runnable_storage_examples = []

auto_runnable_tags = [
    "storage-integration-tests",
    "integration-tests",
]

integration_test_tags = [
    "integration-tests",
]

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    tags = auto_runnable_tags if test in auto_runnable_storage_examples else integration_test_tags,
    deps = [
        ":storage_examples_common",
        "//google/cloud/storage:storage_client",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest",
    ],
) for test in storage_examples]

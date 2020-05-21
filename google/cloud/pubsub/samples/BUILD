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

"""Examples for the Cloud Pub/Sub C++ client library."""

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

load(":pubsub_samples_common.bzl", "pubsub_samples_common_hdrs", "pubsub_samples_common_srcs")

cc_library(
    name = "pubsub_samples_common",
    srcs = pubsub_samples_common_srcs,
    hdrs = pubsub_samples_common_hdrs,
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/pubsub:pubsub_client",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
    ],
)

load(":pubsub_samples_unit_tests.bzl", "pubsub_samples_unit_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":pubsub_samples_common",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/pubsub:pubsub_client",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest_main",
    ],
) for test in pubsub_samples_unit_tests]

load(":pubsub_client_integration_samples.bzl", "pubsub_client_integration_samples")
load(":pubsub_client_unit_samples.bzl", "pubsub_client_unit_samples")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    timeout = "long",
    srcs = [test],
    tags = [
        "integration-test",
    ],
    deps = [
        ":pubsub_samples_common",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/pubsub:pubsub_client",
        "@com_google_googletest//:gtest_main",
    ],
) for test in pubsub_client_integration_samples]

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":pubsub_samples_common",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/pubsub:pubsub_client",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest_main",
    ],
) for test in pubsub_client_unit_samples]

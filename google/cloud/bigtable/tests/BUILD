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

load(
    ":bigtable_client_integration_tests.bzl",
    "bigtable_client_integration_tests",
)

[cc_binary(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_testing",
        "//google/cloud/bigtable:bigtable_client",
        "//google/cloud/bigtable:bigtable_client_testing",
        "@com_google_googletest//:gtest",
    ],
) for test in bigtable_client_integration_tests]

cc_binary(
    name = "instance_admin_emulator",
    srcs = ["instance_admin_emulator.cc"],
    deps = [
        "//google/cloud/bigtable:bigtable_client",
    ],
)

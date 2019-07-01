# Copyright 2019 Google LLC
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

load(":spanner_client.bzl", "spanner_client_hdrs", "spanner_client_srcs")

cc_library(
    name = "spanner_client",
    srcs = spanner_client_srcs,
    hdrs = spanner_client_hdrs,
    deps = [
        "@com_github_googleapis_google_cloud_cpp//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp//google/cloud/grpc_utils:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//:bigtable_protos",
        "@com_google_googleapis//:spanner_protos",
    ],
)

load(":spanner_client_unit_tests.bzl", "spanner_client_unit_tests")

[cc_test(
    name = "spanner_client_" + test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":spanner_client",
        "@com_github_googleapis_google_cloud_cpp//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest",
    ],
) for test in spanner_client_unit_tests]

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

load(":bigquery_client.bzl", "bigquery_client_hdrs", "bigquery_client_srcs")

cc_library(
    name = "bigquery_client",
    srcs = bigquery_client_srcs,
    hdrs = bigquery_client_hdrs,
    deps = [
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/grpc_utils:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//:grpc_utils_protos",
        "@com_google_googleapis//google/cloud/bigquery/storage/v1beta1:storage_cc_grpc",
        "@com_google_googleapis//google/cloud/bigquery/storage/v1beta1:storage_cc_proto",
    ],
)

load(":bigquery_client_testing.bzl", "bigquery_client_testing_hdrs", "bigquery_client_testing_srcs")

cc_library(
    name = "bigquery_client_testing",
    srcs = bigquery_client_testing_srcs,
    hdrs = bigquery_client_testing_hdrs,
    deps = [
        ":bigquery_client",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/grpc_utils:google_cloud_cpp_grpc_utils",
        "@com_google_googletest//:gtest",
    ],
)

load(":bigquery_client_unit_tests.bzl", "bigquery_client_unit_tests")

[cc_test(
    name = "bigquery_client_" + test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":bigquery_client",
        ":bigquery_client_testing",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/grpc_utils:google_cloud_cpp_grpc_utils",
        "@com_google_googletest//:gtest_main",
    ],
) for test in bigquery_client_unit_tests]

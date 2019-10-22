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

# TODO(#17): Use CMake-generated lists of files for this rule.
cc_library(
    name = "bigquery_client",
    srcs = [
        "client.cc",
        "connection_options.cc",
        "internal/bigquerystorage_stub.cc",
        "internal/bigquerystorage_stub.h",
        "internal/connection_impl.cc",
        "internal/connection_impl.h",
        "internal/stream_reader.h",
        "internal/streaming_read_result_source.cc",
        "internal/streaming_read_result_source.h",
        "read_stream.cc",
        "version.cc",
    ],
    hdrs = [
        "client.h",
        "connection.h",
        "connection_options.h",
        "read_result.h",
        "read_stream.h",
        "row.h",
        "row_set.h",
        "version.h",
        "version_info.h",
    ],
    deps = [
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/grpc_utils:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//:bigquery_protos",
    ],
)

# TODO(#17): Use CMake-generated lists of files for this rule.
cc_library(
    name = "bigquery_client_mocks",
    srcs = [],
    hdrs = [
        "testing/mock_bigquerystorage_stub.h",
    ],
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
        ":bigquery_client_mocks",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/grpc_utils:google_cloud_cpp_grpc_utils",
        "@com_google_googletest//:gtest_main",
    ],
) for test in bigquery_client_unit_tests]

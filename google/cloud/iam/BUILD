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

package(default_visibility = ["//visibility:private"])

licenses(["notice"])  # Apache 2.0

load(":google_cloud_cpp_iam.bzl", "google_cloud_cpp_iam_hdrs", "google_cloud_cpp_iam_srcs")

cc_library(
    name = "google_cloud_cpp_iam",
    srcs = google_cloud_cpp_iam_srcs,
    hdrs = google_cloud_cpp_iam_hdrs,
    visibility = ["//:__pkg__"],
    # Do not sort: grpc++ must come last
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//google/iam/admin/v1:admin_cc_grpc",
        "@com_google_googleapis//google/iam/credentials/v1:credentials_cc_grpc",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

load(":google_cloud_cpp_iam_mocks.bzl", "google_cloud_cpp_iam_mocks_hdrs", "google_cloud_cpp_iam_mocks_srcs")

cc_library(
    name = "google_cloud_cpp_iam_mocks",
    srcs = google_cloud_cpp_iam_mocks_srcs,
    hdrs = google_cloud_cpp_iam_mocks_hdrs,
    visibility = ["//:__pkg__"],
    deps = [
        ":google_cloud_cpp_iam",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//google/iam/admin/v1:admin_cc_grpc",
        "@com_google_googleapis//google/iam/credentials/v1:credentials_cc_grpc",
    ],
)

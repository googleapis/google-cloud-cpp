# Copyright 2021 Google LLC
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

load(":google_cloud_cpp_logging.bzl", "google_cloud_cpp_logging_hdrs", "google_cloud_cpp_logging_srcs")

cc_library(
    name = "google_cloud_cpp_logging",
    srcs = google_cloud_cpp_logging_srcs,
    hdrs = google_cloud_cpp_logging_hdrs,
    visibility = ["//:__pkg__"],
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//google/logging/v2:logging_cc_grpc",
    ],
)

load(":google_cloud_cpp_logging_mocks.bzl", "google_cloud_cpp_logging_mocks_hdrs", "google_cloud_cpp_logging_mocks_srcs")

cc_library(
    name = "google_cloud_cpp_logging_mocks",
    srcs = google_cloud_cpp_logging_mocks_srcs,
    hdrs = google_cloud_cpp_logging_mocks_hdrs,
    visibility = ["//:__pkg__"],
    deps = [
        ":google_cloud_cpp_logging",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
    ],
)

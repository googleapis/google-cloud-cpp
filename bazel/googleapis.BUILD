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

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

cc_library(
    name = "grpc_utils_protos",
    includes = [
        ".",
    ],
    deps = [
        "//google/rpc:status_cc_proto",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

cc_library(
    name = "bigquery_protos",
    # Do not sort: grpc++ must come last
    deps = [
        "@com_google_googleapis//google/cloud/bigquery/storage/v1beta1:storage_cc_grpc",
        "@com_google_googleapis//google/cloud/bigquery/storage/v1beta1:storage_cc_proto",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

cc_proto_library(
    name = "bigtableadmin_cc_proto",
    visibility = ["//visibility:private"],
    deps = ["//google/bigtable/admin/v2:admin_proto"],
)

cc_grpc_library(
    name = "bigtableadmin_cc_grpc",
    srcs = [
        "//google/bigtable/admin/v2:admin_proto",
    ],
    grpc_only = True,
    visibility = ["//visibility:private"],
    deps = [
        ":bigtableadmin_cc_proto",
        "//google/longrunning:longrunning_cc_grpc",
    ],
)

cc_library(
    name = "bigtable_protos",
    # Do not sort: grpc++ must come last
    deps = [
        ":bigtableadmin_cc_grpc",
        "//google/bigtable/v2:bigtable_cc_grpc",
        "//google/rpc:error_details_cc_proto",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

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

cc_grpc_library(
    name = "google_api_http",
    srcs = ["google/api/http.proto"],
    deps = [],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_grpc_library(
    name = "google_api_annotations",
    srcs = ["google/api/annotations.proto"],
    deps = [":google_api_http"],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_grpc_library(
    name = "google_api_auth",
    srcs = ["google/api/auth.proto"],
    deps = [":google_api_annotations"],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_grpc_library(
    name = "google_rpc_error_details",
    srcs = ["google/rpc/error_details.proto"],
    deps = [],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_grpc_library(
    name = "google_rpc_status",
    srcs = ["google/rpc/status.proto"],
    deps = [],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_grpc_library(
    name = "google_longrunning_operations",
    srcs = ["google/longrunning/operations.proto"],
    deps = [":google_rpc_status", ":google_api_annotations"],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_grpc_library(
    name = "google_iam_v1_policy",
    srcs = ["google/iam/v1/policy.proto"],
    deps = [":google_api_annotations"],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_grpc_library(
    name = "google_iam_v1_iam_policy",
    srcs = ["google/iam/v1/iam_policy.proto"],
    deps = [":google_api_annotations", ":google_iam_v1_policy"],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_grpc_library(
    name = "google_bigtable_v2_data",
    srcs = ["google/bigtable/v2/data.proto"],
    deps = [],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_grpc_library(
    name = "google_bigtable_v2_bigtable",
    srcs = ["google/bigtable/v2/bigtable.proto"],
    deps = [":google_api_annotations", "google_bigtable_v2_data", ":google_rpc_status"],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_grpc_library(
    name = "google_bigtable_admin_v2_table",
    srcs = ["google/bigtable/admin/v2/table.proto"],
    deps = [":google_api_annotations"],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_grpc_library(
    name = "google_bigtable_admin_v2_instance",
    srcs = ["google/bigtable/admin/v2/instance.proto"],
    deps = [":google_api_annotations", ":google_bigtable_admin_v2_common"],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_grpc_library(
    name = "google_bigtable_admin_v2_common",
    srcs = ["google/bigtable/admin/v2/common.proto"],
    deps = [":google_api_annotations"],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_grpc_library(
    name = "google_bigtable_admin_v2_bigtable_table_admin",
    srcs = ["google/bigtable/admin/v2/bigtable_table_admin.proto"],
    deps = [
        ":google_api_annotations",
        ":google_longrunning_operations",
        ":google_bigtable_admin_v2_table",
    ],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_grpc_library(
    name = "google_bigtable_admin_v2_bigtable_instance_admin",
    srcs = ["google/bigtable/admin/v2/bigtable_instance_admin.proto"],
    deps = [
        ":google_api_annotations",
        ":google_longrunning_operations",
        ":google_iam_v1_iam_policy",
        ":google_bigtable_admin_v2_instance",
    ],
    proto_only = False,
    well_known_protos = True,
    use_external = True,
)

cc_library(
    name = "bigtable_protos",
    deps = [
        ":google_api_http",
        ":google_api_auth",
        ":google_api_annotations",
        ":google_rpc_error_details",
        ":google_rpc_status",
        ":google_longrunning_operations",
        ":google_iam_v1_policy",
        ":google_iam_v1_iam_policy",
        ":google_bigtable_v2_data",
        ":google_bigtable_v2_bigtable",
        ":google_bigtable_admin_v2_common",
        ":google_bigtable_admin_v2_table",
        ":google_bigtable_admin_v2_instance",
        ":google_bigtable_admin_v2_bigtable_table_admin",
        ":google_bigtable_admin_v2_bigtable_instance_admin",
        "@com_github_grpc_grpc//:grpc++",
    ],
    includes = ["."],
)

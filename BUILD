# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files([
    "LICENSE",
])

cc_library(
    name = "bigtable",
    deps = [
        "//google/cloud/bigtable:bigtable_client_internal",
    ],
)

cc_library(
    name = "bigtable_client",
    deprecation = "this target will be removed on or around 2022-02-15, please use //:bigtable instead.",
    tags = ["manual"],
    deps = [
        "//google/cloud/bigtable:bigtable_client_internal",
    ],
)

cc_library(
    name = "pubsub",
    deps = [
        "//google/cloud/pubsub:google_cloud_cpp_pubsub",
    ],
)

cc_library(
    name = "pubsub_mocks",
    deps = [
        "//google/cloud/pubsub:google_cloud_cpp_pubsub_mocks",
    ],
)

cc_library(
    name = "experimental-pubsublite",
    deps = [
        "//google/cloud/pubsublite:google_cloud_cpp_pubsublite",
    ],
)

cc_library(
    name = "experimental-pubsublite_mocks",
    deps = [
        "//google/cloud/pubsublite:google_cloud_cpp_pubsublite_mocks",
    ],
)

cc_library(
    name = "pubsub_client",
    deprecation = "this target will be removed on or around 2022-02-15, please use //:pubsub instead.",
    tags = ["manual"],
    deps = [
        "//google/cloud/pubsub:google_cloud_cpp_pubsub",
    ],
)

cc_library(
    name = "experimental-secretmanager",
    deps = [
        "//google/cloud/secretmanager:google_cloud_cpp_secretmanager",
    ],
)

cc_library(
    name = "experimental-secretmanager_mocks",
    deps = [
        "//google/cloud/secretmanager:google_cloud_cpp_secretmanager_mocks",
    ],
)

cc_library(
    name = "spanner",
    deps = [
        "//google/cloud/spanner:google_cloud_cpp_spanner",
    ],
)

cc_library(
    name = "spanner_mocks",
    deps = [
        "//google/cloud/spanner:google_cloud_cpp_spanner_mocks",
    ],
)

cc_library(
    name = "spanner_client",
    deprecation = "this target will be removed on or around 2022-02-15, please use //:spanner instead.",
    tags = ["manual"],
    deps = [
        "//google/cloud/spanner:google_cloud_cpp_spanner",
    ],
)

cc_library(
    name = "storage",
    deps = [
        "//google/cloud/storage:google_cloud_cpp_storage",
    ],
)

cc_library(
    name = "experimental-storage-grpc",
    tags = ["manual"],
    deps = [
        "//google/cloud/storage:google_cloud_cpp_storage_grpc",
    ],
)

cc_library(
    name = "storage_client",
    deprecation = "this target will be removed on or around 2022-02-15, please use //:storage instead.",
    tags = ["manual"],
    deps = [
        "//google/cloud/storage:google_cloud_cpp_storage",
    ],
)

cc_library(
    name = "iam",
    deps = [
        "//google/cloud/iam:google_cloud_cpp_iam",
    ],
)

cc_library(
    name = "iam-mocks",
    deps = [
        "//google/cloud/iam:google_cloud_cpp_iam_mocks",
    ],
)

cc_library(
    name = "experimental-logging",
    deps = [
        "//google/cloud/logging:google_cloud_cpp_logging",
    ],
)

cc_library(
    name = "experimental-logging-mocks",
    deps = [
        "//google/cloud/logging:google_cloud_cpp_logging_mocks",
    ],
)

cc_library(
    name = "bigquery",
    deps = [
        "//google/cloud/bigquery:google_cloud_cpp_bigquery",
    ],
)

cc_library(
    name = "bigquery-mocks",
    deps = [
        "//google/cloud/bigquery:google_cloud_cpp_bigquery_mocks",
    ],
)

cc_library(
    name = "experimental-tasks",
    deps = [
        "//google/cloud/tasks:google_cloud_cpp_tasks",
    ],
)

cc_library(
    name = "experimental-tasks_mocks",
    deps = [
        "//google/cloud/tasks:google_cloud_cpp_tasks_mocks",
    ],
)

cc_library(
    name = "experimental-workflows",
    deps = [
        "//google/cloud/workflows:google_cloud_cpp_workflows",
    ],
)

cc_library(
    name = "experimental-workflows_mocks",
    deps = [
        "//google/cloud/workflows:google_cloud_cpp_workflows_mocks",
    ],
)

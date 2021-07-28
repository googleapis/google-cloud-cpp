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

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

CURRENT_TARGETS = [
    ":bigquery",
    ":bigtable",
    ":experimental-firestore",
    ":iam",
    ":experimental-logging",
    ":experimental-logging-mocks",
    ":pubsub",
    ":spanner",
    ":storage",
    ":experimental-storage-grpc",
]

[cc_test(
    name = "verify_" + target.replace("/", "_").replace(":", "_"),
    srcs = [
        "verify_current_targets.cc",
    ],
    deps = [
        "@com_github_googleapis_google_cloud_cpp//" + target,
    ],
) for target in CURRENT_TARGETS]

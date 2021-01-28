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

# TODO(#5726) - DO NOT CHANGE THESE TARGET NAMES These package names are
# expected to remain usable until we have retired them. They are *deprecated*
# (that is, we no longer recommend their use), but *NOT* retired (that is, they
# remain available). This is a test for backwards compatibility. If you need to
# change the test, there is good chance you are breaking something.
DEPRECATED_TARGETS = [
    "google/cloud:google_cloud_cpp_common",
    "google/cloud:google_cloud_cpp_grpc_utils",
    "google/cloud/bigtable:bigtable_client",
    "google/cloud/firestore:firestore_client",
    "google/cloud/pubsub:pubsub_client",
    "google/cloud/spanner:spanner_client",
    "google/cloud/storage:storage_client",
    ":bigtable_client",
    ":pubsub_client",
    ":spanner_client",
    ":storage_client",
]

[cc_test(
    name = "verify_" + target.replace("/", "_").replace(":", "_"),
    srcs = [
        "verify_deprecated_targets.cc",
    ],
    deps = [
        "@com_github_googleapis_google_cloud_cpp//" + target,
    ],
) for target in DEPRECATED_TARGETS]

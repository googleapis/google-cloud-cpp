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

# This library defines main(). It's declared as a library so it can be part of
# a cc_test and cc_binary target.
cc_library(
    name = "gcs2cbt_lib",
    srcs = ["gcs2cbt.cc"],
    deps = [
        "//google/cloud/bigtable:bigtable_client",
        "//google/cloud/storage:storage_client",
    ],
)

cc_binary(
    name = "gcs2cbt",
    deps = [":gcs2cbt_lib"],
)

cc_test(
    name = "gcs2cbt_test",
    tags = [
        "integration-test",
        "integration-test-production",
    ],
    deps = [":gcs2cbt_lib"],
)

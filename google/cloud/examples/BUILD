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

load(
    "@com_google_googleapis_imports//:imports.bzl",
    "cc_grpc_library",
    "cc_proto_library",
)

# This library defines main(). It's declared as a library so it can be part of
# a cc_test and cc_binary target.
cc_library(
    name = "gcs2cbt_lib",
    srcs = ["gcs2cbt.cc"],
    deps = [
        "//:bigtable",
        "//:storage",
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

proto_library(
    name = "hello_world_protos",
    srcs = [
        "hello_world_grpc/hello_world.proto",
    ],
    visibility = [
        "//visibility:private",
    ],
    deps = [],
)

cc_proto_library(
    name = "hello_world_cc_proto",
    visibility = [
        "//visibility:private",
    ],
    deps = [":hello_world_protos"],
)

cc_grpc_library(
    name = "hello_world_cc_grpc",
    srcs = [":hello_world_protos"],
    grpc_only = True,
    visibility = [
        "//visibility:private",
    ],
    deps = [":hello_world_cc_proto"],
)

cc_test(
    name = "grpc_credential_types",
    timeout = "long",
    srcs = ["grpc_credential_types.cc"],
    copts = [
        "-I$(GENDIR)/google/cloud/examples",
    ],
    tags = [
        "integration-test",
        "integration-test-production",
    ],
    deps = [
        ":hello_world_cc_grpc",
        ":hello_world_cc_proto",
        "//:experimental-iam",
        "//:spanner",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_github_curl_curl//:curl",
    ],
)

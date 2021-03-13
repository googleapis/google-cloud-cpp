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

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
    name = "google_cloud_cpp_generator_testing_proto",
    srcs = [
        "backup.proto",
        "common.proto",
        "test.proto",
    ],
    deps = [
        "@com_google_googleapis//google/api:annotations_proto",
        "@com_google_googleapis//google/api:client_proto",
        "@com_google_googleapis//google/api:field_behavior_proto",
        "@com_google_googleapis//google/api:resource_proto",
        "@com_google_googleapis//google/iam/v1:iam_policy_proto",
        "@com_google_googleapis//google/iam/v1:policy_proto",
        "@com_google_googleapis//google/longrunning:operations_proto",
        "@com_google_googleapis//google/rpc:status_proto",
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:empty_proto",
        "@com_google_protobuf//:field_mask_proto",
        "@com_google_protobuf//:struct_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

load(
    "@com_google_googleapis_imports//:imports.bzl",
    "cc_grpc_library",
    "cc_proto_library",
)

cc_proto_library(
    name = "google_cloud_cpp_generator_testing_cc_proto",
    deps = [":google_cloud_cpp_generator_testing_proto"],
)

cc_grpc_library(
    name = "google_cloud_cpp_generator_testing_cc_grpc",
    srcs = [":google_cloud_cpp_generator_testing_proto"],
    grpc_only = True,
    deps = [":google_cloud_cpp_generator_testing_cc_proto"],
)

load(":google_cloud_cpp_generator_integration_tests.bzl", "google_cloud_cpp_generator_integration_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    timeout = "long",
    srcs = [test],
    data = [
        "//generator/integration_tests/golden:golden_filegroup",
        "//generator/integration_tests/golden:golden_internal_filegroup",
        "//generator/integration_tests/golden:golden_mocks_filegroup",
    ],
    tags = [
        "integration-test",
    ],
    deps = [
        "//generator:google_cloud_cpp_generator",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest_main",
    ],
) for test in google_cloud_cpp_generator_integration_tests]

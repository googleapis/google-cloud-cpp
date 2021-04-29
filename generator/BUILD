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

load(":google_cloud_cpp_generator.bzl", "google_cloud_cpp_generator_hdrs", "google_cloud_cpp_generator_srcs")

cc_library(
    name = "google_cloud_cpp_generator",
    srcs = google_cloud_cpp_generator_srcs,
    hdrs = google_cloud_cpp_generator_hdrs,
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/longrunning:longrunning_cc_grpc",
        "@com_google_protobuf//:protoc_lib",
    ],
)

load(":google_cloud_cpp_generator_testing.bzl", "google_cloud_cpp_generator_testing_hdrs", "google_cloud_cpp_generator_testing_srcs")

cc_library(
    name = "google_cloud_cpp_generator_testing",
    srcs = google_cloud_cpp_generator_testing_hdrs,
    hdrs = google_cloud_cpp_generator_testing_srcs,
    deps = [
        ":google_cloud_cpp_generator",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest_main",
    ],
)

load(":google_cloud_cpp_generator_unit_tests.bzl", "google_cloud_cpp_generator_unit_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":google_cloud_cpp_generator",
        ":google_cloud_cpp_generator_testing",
        "@com_google_googletest//:gtest_main",
    ],
) for test in google_cloud_cpp_generator_unit_tests]

cc_binary(
    name = "protoc-gen-cpp_codegen",
    srcs = ["plugin_main.cc"],
    visibility = ["//visibility:public"],
    deps = [":google_cloud_cpp_generator"],
)

proto_library(
    name = "generator_config_proto",
    srcs = ["generator_config.proto"],
)

cc_proto_library(
    name = "generator_config_cc_proto",
    deps = [":generator_config_proto"],
)

cc_binary(
    name = "google-cloud-cpp-codegen",
    srcs = ["standalone_main.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":generator_config_cc_proto",
        ":google_cloud_cpp_generator",
        "@com_google_absl//absl/flags:commandlineflag",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
    ],
)

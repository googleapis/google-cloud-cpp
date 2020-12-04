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

load(":google_cloud_cpp_generator_golden_files.bzl", "google_cloud_cpp_generator_golden_files")

filegroup(
    name = "golden_filegroup",
    srcs = glob([
        "*gcpcxx*",
        "retry_traits.h",
    ]),
)

filegroup(
    name = "golden_internal_filegroup",
    srcs = glob(["internal/*gcpcxx*"]),
)

filegroup(
    name = "golden_mocks_filegroup",
    srcs = glob(["mocks/*gcpcxx.pb.h"]),
)

cc_library(
    name = "google_cloud_cpp_generator_golden",
    srcs = [
        ":golden_filegroup",
        ":golden_internal_filegroup",
    ],
    copts = [
        "-I$(BINDIR)/generator/integration_tests/golden",
        "-I$(BINDIR)/generator/integration_tests/golden/internal",
        "-I$(BINDIR)",
    ],
    deps = [
        "//generator/integration_tests:google_cloud_cpp_generator_testing_cc_grpc",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/longrunning:longrunning_cc_grpc",
        "@com_google_protobuf//:protoc_lib",
    ],
)

load(":google_cloud_cpp_generator_golden_tests.bzl", "google_cloud_cpp_generator_golden_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [
        test,
        ":golden_mocks_filegroup",
    ],
    copts = [
        "-I$(BINDIR)/generator/integration_tests/golden",
        "-I$(BINDIR)/generator/integration_tests/golden/internal",
        "-I$(BINDIR)",
    ],
    deps = [
        ":google_cloud_cpp_generator_golden",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_google_googletest//:gtest_main",
    ],
) for test in google_cloud_cpp_generator_golden_tests]

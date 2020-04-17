# Copyright 2019 Google LLC
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

# TODO(#1415) Change this visibility to "//:__subpackages__" so that users are
# required to use the top-level BUILD file rather than reaching down into this
# one.
package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

genrule(
    name = "generate_build_info",
    srcs = ["internal/build_info.cc.in"],
    outs = ["internal/build_info.cc"],
    cmd = """
V="unknown";
sed -e "s;@CMAKE_CXX_FLAGS@;$(CC_FLAGS);" \
    -e "s;\\$${CMAKE_CXX_FLAGS_.*};-c $(COMPILATION_MODE);" \
    -e "s;@GOOGLE_CLOUD_CPP_SPANNER_BUILD_METADATA@;$${V};" < $< > $@
  """,
    toolchains = [
        "@bazel_tools//tools/cpp:current_cc_toolchain",
        "@bazel_tools//tools/cpp:cc_flags",
    ],
)

load(":spanner_client.bzl", "spanner_client_hdrs", "spanner_client_srcs")

cc_library(
    name = "spanner_client",
    srcs = spanner_client_srcs + ["internal/build_info.cc"],
    hdrs = spanner_client_hdrs,
    data = [
        ":generate_build_info",
    ],
    deps = [
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//google/longrunning:longrunning_cc_grpc",
        "@com_google_googleapis//google/spanner/admin/database/v1:database_cc_grpc",
        "@com_google_googleapis//google/spanner/admin/instance/v1:instance_cc_grpc",
        "@com_google_googleapis//google/spanner/v1:spanner_cc_grpc",
    ],
)

load(":spanner_client_mocks.bzl", "spanner_client_mocks_hdrs", "spanner_client_mocks_srcs")

cc_library(
    name = "spanner_client_mocks",
    srcs = spanner_client_mocks_srcs,
    hdrs = spanner_client_mocks_hdrs,
    deps = [
        ":spanner_client",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest",
    ],
)

load(":spanner_client_testing.bzl", "spanner_client_testing_hdrs", "spanner_client_testing_srcs")

cc_library(
    name = "spanner_client_testing",
    srcs = spanner_client_testing_srcs,
    hdrs = spanner_client_testing_hdrs,
    deps = [
        ":spanner_client",
        ":spanner_client_mocks",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest",
    ],
)

load(":spanner_client_unit_tests.bzl", "spanner_client_unit_tests")

[cc_test(
    name = "spanner_client_" + test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":spanner_client",
        ":spanner_client_mocks",
        ":spanner_client_testing",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_google_googletest//:gtest",
    ],
) for test in spanner_client_unit_tests]

load(":spanner_client_benchmarks.bzl", "spanner_client_benchmarks")

[cc_test(
    name = "spanner_client_" + benchmark.replace("/", "_").replace(".cc", ""),
    srcs = [benchmark],
    tags = ["benchmark"],
    deps = [
        ":spanner_client",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_google_benchmark//:benchmark_main",
    ],
) for benchmark in spanner_client_benchmarks]

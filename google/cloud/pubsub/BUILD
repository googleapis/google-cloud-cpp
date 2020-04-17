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

genrule(
    name = "generate_build_info",
    srcs = ["internal/build_info.cc.in"],
    outs = ["internal/build_info.cc"],
    cmd = """
V="unknown";
sed -e "s;@CMAKE_CXX_FLAGS@;$(CC_FLAGS);" \
    -e "s;\\$${CMAKE_CXX_FLAGS_.*};-c $(COMPILATION_MODE);" \
    -e "s;@GOOGLE_CLOUD_CPP_PUBSUB_BUILD_METADATA@;$${V};" < $< > $@
  """,
    toolchains = [
        "@bazel_tools//tools/cpp:current_cc_toolchain",
        "@bazel_tools//tools/cpp:cc_flags",
    ],
)

load(":googleapis_cpp_pubsub_client.bzl", "googleapis_cpp_pubsub_client_hdrs", "googleapis_cpp_pubsub_client_srcs")

cc_library(
    name = "pubsub_client",
    srcs = googleapis_cpp_pubsub_client_srcs + ["internal/build_info.cc"],
    hdrs = googleapis_cpp_pubsub_client_hdrs,
    data = [
        ":generate_build_info",
    ],
    deps = [
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_grpc_utils",
        "@com_google_googleapis//google/pubsub/v1:pubsub_cc_grpc",
    ],
)

load(":googleapis_cpp_pubsub_client_unit_tests.bzl", "googleapis_cpp_pubsub_client_unit_tests")

[cc_test(
    name = test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":pubsub_client",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_github_googleapis_google_cloud_cpp_common//google/cloud/testing_util:google_cloud_cpp_testing_grpc",
        "@com_google_googletest//:gtest",
    ],
) for test in googleapis_cpp_pubsub_client_unit_tests]

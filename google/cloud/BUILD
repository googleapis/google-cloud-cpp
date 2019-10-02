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

genrule(
    name = "generate_build_info",
    srcs = ["internal/build_info.cc.in"],
    outs = ["internal/build_info.cc"],
    cmd = """
V=$$(git rev-parse --short HEAD 2>/dev/null || echo "unknown");
sed -e "s;@CMAKE_CXX_FLAGS@;$(CC_FLAGS);" \
    -e "s;\\$${CMAKE_CXX_FLAGS_.*};$(COMPILATION_MODE);" \
    -e "s;@GOOGLE_CLOUD_CPP_GIT_HEAD@;$${V};" < $< > $@
  """,
    toolchains = [
        "@bazel_tools//tools/cpp:current_cc_toolchain",
        "@bazel_tools//tools/cpp:cc_flags",
    ],
)

load(":google_cloud_cpp_common.bzl", "google_cloud_cpp_common_hdrs", "google_cloud_cpp_common_srcs")

cc_library(
    name = "google_cloud_cpp_common",
    srcs = google_cloud_cpp_common_srcs + ["internal/build_info.cc"],
    hdrs = google_cloud_cpp_common_hdrs,
    deps = [],
)

load(
    ":google_cloud_cpp_common_unit_tests.bzl",
    "google_cloud_cpp_common_unit_tests",
)

[cc_test(
    name = "google_cloud_cpp_common_" + test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    linkopts = select({
        "@bazel_tools//src/conditions:windows": [],
        "//conditions:default": ["-lpthread"],
    }),
    deps = [
        ":google_cloud_cpp_common",
        "//google/cloud/testing_util:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
) for test in google_cloud_cpp_common_unit_tests]

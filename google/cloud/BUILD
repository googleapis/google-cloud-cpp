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
    ":google_cloud_cpp_common_version.bzl",
    "GOOGLE_CLOUD_CPP_IS_RELEASE",
    "GOOGLE_CLOUD_CPP_VERSION_MAJOR",
    "GOOGLE_CLOUD_CPP_VERSION_MINOR",
    "GOOGLE_CLOUD_CPP_VERSION_PATCH",
)

genrule(
    name = "generate_build_info",
    srcs = ["internal/build_info.cc.in"],
    outs = ["internal/build_info.cc"],
    cmd = """
CC_COMPILER_VERSION=$$($(CC) --version | head -1);
V=$$(git rev-parse --short HEAD 2>/dev/null || echo "unknown");
sed -e "s;@CMAKE_CXX_COMPILER_ID@;$(C_COMPILER);" \
    -e "s;@CMAKE_CXX_COMPILER_VERSION@;$${CC_COMPILER_VERSION};" \
    -e "s;@CMAKE_CXX_FLAGS@;$(CC_FLAGS);" \
    -e "s;\\$${CMAKE_CXX_FLAGS_.*};$(COMPILATION_MODE);" \
    -e "s;@GOOGLE_CLOUD_CPP_IS_RELEASE@;%s;" \
    -e "s;@GOOGLE_CLOUD_CPP_GIT_HEAD@;$${V};" < $< > $@
  """ % (GOOGLE_CLOUD_CPP_IS_RELEASE),
    toolchains = [
        "@bazel_tools//tools/cpp:current_cc_toolchain",
        "@bazel_tools//tools/cpp:cc_flags",
    ],
)

genrule(
    name = "generate_version_info",
    srcs = ["internal/version_info.h.in"],
    outs = ["internal/version_info.h"],
    cmd = """
sed -e 's/@GOOGLE_CLOUD_CPP_VERSION_MAJOR@/%s/' \
    -e 's/@GOOGLE_CLOUD_CPP_VERSION_MINOR@/%s/' \
    -e 's/@GOOGLE_CLOUD_CPP_VERSION_PATCH@/%s/' < $< > $@
""" % (
        GOOGLE_CLOUD_CPP_VERSION_MAJOR,
        GOOGLE_CLOUD_CPP_VERSION_MINOR,
        GOOGLE_CLOUD_CPP_VERSION_PATCH,
    ),
)

load(":google_cloud_cpp_common.bzl", "google_cloud_cpp_common_hdrs", "google_cloud_cpp_common_srcs")

cc_library(
    name = "google_cloud_cpp_common",
    srcs = google_cloud_cpp_common_srcs + ["internal/build_info.cc"],
    hdrs = google_cloud_cpp_common_hdrs + ["internal/version_info.h"],
    deps = [],
)

load(":google_cloud_cpp_testing.bzl", "google_cloud_cpp_testing_hdrs", "google_cloud_cpp_testing_srcs")

cc_library(
    name = "google_cloud_cpp_testing",
    srcs = google_cloud_cpp_testing_srcs,
    hdrs = google_cloud_cpp_testing_hdrs,
    deps = [
        ":google_cloud_cpp_common",
        "@com_google_googletest//:gtest",
    ],
)

load(
    ":google_cloud_cpp_common_unit_tests.bzl",
    "google_cloud_cpp_common_unit_tests",
)

[cc_test(
    name = "google_cloud_cpp_common_" + test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    # TODO(#664 / #666) - use the right condition when porting Bazel builds
    linkopts = ["-lpthread"],
    deps = [
        ":google_cloud_cpp_common",
        ":google_cloud_cpp_testing",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
) for test in google_cloud_cpp_common_unit_tests]

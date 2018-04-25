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
    srcs = ["build_info.cc.in"],
    outs = ["build_info.cc"],
    cmd = """
sed -e "s;@CMAKE_CXX_COMPILER_ID@;Clang;" \
    -e "s;@CMAKE_CXX_COMPILER_VERSION@;4.0.1;" \
    -e "s;@CMAKE_CXX_FLAGS@;$(CC_FLAGS);" \
    -e "s;@CMAKE_CXX_FLAGS_.*}@;$(CC_FLAGS);" \
    -e "s;@GIT_HEAD@;piper;" < $< > $@
  """,
)

genrule(
    name = "generate_version_info",
    srcs = ["version_info.h.in"],
    outs = ["version_info.h"],
    cmd = """
sed -e 's/@BIGTABLE_CLIENT_VERSION_MAJOR@/0/' \
    -e 's/@BIGTABLE_CLIENT_VERSION_MINOR@/2/' \
    -e 's/@BIGTABLE_CLIENT_VERSION_PATCH@/0/' < $< > $@
""",
)

load(":bigtable_client.bzl", "bigtable_client_SRCS", "bigtable_client_HDRS")
cc_library(
    name = "bigtable_client",
    srcs = bigtable_client_SRCS + [ "build_info.cc" ],
    hdrs = bigtable_client_HDRS + [ "version_info.h" ],
    deps = [
         "@com_github_googleapis_googleapis//:bigtable_protos",
    ],
)

load(":bigtable_client_testing.bzl", "bigtable_client_testing_SRCS", "bigtable_client_testing_HDRS")
cc_library(
    name = "bigtable_client_testing",
    srcs = bigtable_client_testing_SRCS,
    hdrs = bigtable_client_testing_HDRS,
    deps = [
      ":bigtable_client",
      "@com_github_googleapis_googleapis//:bigtable_protos",
      "@com_google_googletest//:gtest",
      "@com_google_googletest//:gtest_main",
    ]
)

load(":bigtable_client_unit_tests.bzl", "bigtable_client_unit_tests")
[cc_test(
    name = "bigtable_client_" + test.replace('/', '_').replace('.cc', ''),
    srcs = [test],
    deps = [
      ":bigtable_client_testing",
      ":bigtable_client",
      "@com_google_googletest//:gtest",
      "@com_google_googletest//:gtest_main",
    ],
) for test in bigtable_client_unit_tests]

cc_test(
    name = "bigtable_client_internal_readrowsparser_test",
    srcs = [
        "internal/readrowsparser_test.cc",
        "internal/readrowsparser_acceptance_tests.inc"
    ],
    deps = [
        ":bigtable_client_testing",
        ":bigtable_client",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

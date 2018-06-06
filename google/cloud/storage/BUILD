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
    name = "generate_version_info",
    srcs = ["version_info.h.in"],
    outs = ["version_info.h"],
    cmd = """
sed -e 's/@STORAGE_CLIENT_VERSION_MAJOR@/0/' \
    -e 's/@STORAGE_CLIENT_VERSION_MINOR@/2/' \
    -e 's/@STORAGE_CLIENT_VERSION_PATCH@/0/' < $< > $@
""",
)

cc_library(
    name = "libcurl",
    hdrs = [],
    srcs = [],
    linkopts = [
        # TODO() - will not work with MSVC.
        "-lcurl",
    ],
)

genrule(
    name = "nlohmann_json_include_hierarchy",
    srcs = [ "@com_github_nlohmann_json_single_header//file" ],
    outs = [ "nlohmann/json.hpp", ],
    cmd = "cp $< $@ && echo GENDIR=$(GENDIR) @=$(@) S=$<",
)

cc_library(
    name = "nlohmann_json",
    hdrs = [ ":nlohmann_json_include_hierarchy" ],
    includes = [ "." ],
    deps = [],
)

load(":storage_client.bzl", "storage_client_SRCS", "storage_client_HDRS")
cc_library(
    name = "storage_client",
    srcs = storage_client_SRCS,
    hdrs = storage_client_HDRS + [ "version_info.h" ],
    deps = [
        ":nlohmann_json",
        ":libcurl",
        "//google/cloud:google_cloud_cpp_common",
    ],
)

load(":storage_client_testing.bzl", "storage_client_testing_SRCS", "storage_client_testing_HDRS")
cc_library(
    name = "storage_client_testing",
    srcs = storage_client_testing_SRCS,
    hdrs = storage_client_testing_HDRS,
    deps = [
        ":storage_client",
        ":nlohmann_json",
        ":libcurl",
        "//google/cloud:google_cloud_cpp_common",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

load(":storage_client_unit_tests.bzl", "storage_client_unit_tests")
[cc_test(
    name = "storage_client_" + test.replace('/', '_').replace('.cc', ''),
    srcs = [test],
    deps = [
        ":storage_client_testing",
        ":storage_client",
        ":nlohmann_json",
        ":libcurl",
        "//google/cloud:google_cloud_cpp_common",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
    # TODO(#664 / #666) - use the right condition when porting Bazel builds
    linkopts = [ "-lpthread" ],
) for test in storage_client_unit_tests]

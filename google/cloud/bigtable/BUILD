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
    ":bigtable_client_version.bzl",
    "BIGTABLE_CLIENT_VERSION_MAJOR",
    "BIGTABLE_CLIENT_VERSION_MINOR",
    "BIGTABLE_CLIENT_VERSION_PATCH",
)

genrule(
    name = "generate_version_info",
    srcs = ["version_info.h.in"],
    outs = ["version_info.h"],
    cmd = """
sed -e 's/@BIGTABLE_CLIENT_VERSION_MAJOR@/%s/' \
    -e 's/@BIGTABLE_CLIENT_VERSION_MINOR@/%s/' \
    -e 's/@BIGTABLE_CLIENT_VERSION_PATCH@/%s/' < $< > $@
""" % (
        BIGTABLE_CLIENT_VERSION_MAJOR,
        BIGTABLE_CLIENT_VERSION_MINOR,
        BIGTABLE_CLIENT_VERSION_PATCH,
    ),
)

load(":bigtable_client.bzl", "bigtable_client_HDRS", "bigtable_client_SRCS")

cc_library(
    name = "bigtable_client",
    srcs = bigtable_client_SRCS,
    hdrs = bigtable_client_HDRS + ["version_info.h"],
    deps = [
        "//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_googleapis//:bigtable_protos",
    ],
)

load(":bigtable_client_testing.bzl", "bigtable_client_testing_HDRS", "bigtable_client_testing_SRCS")

cc_library(
    name = "bigtable_client_testing",
    srcs = bigtable_client_testing_SRCS,
    hdrs = bigtable_client_testing_HDRS,
    deps = [
        ":bigtable_client",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_testing",
        "@com_github_googleapis_googleapis//:bigtable_protos",
        "@com_google_googletest//:gtest",
    ],
)

load(":bigtable_client_unit_tests.bzl", "bigtable_client_unit_tests")

[cc_test(
    name = "bigtable_client_" + test.replace("/", "_").replace(".cc", ""),
    srcs = [test],
    deps = [
        ":bigtable_client",
        ":bigtable_client_testing",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest",
    ],
) for test in bigtable_client_unit_tests]

cc_test(
    name = "bigtable_client_internal_readrowsparser_test",
    srcs = [
        "internal/readrowsparser_acceptance_tests.inc",
        "internal/readrowsparser_test.cc",
    ],
    deps = [
        ":bigtable_client",
        ":bigtable_client_testing",
        "//google/cloud:google_cloud_cpp_common",
        "//google/cloud:google_cloud_cpp_testing",
        "@com_google_googletest//:gtest",
    ],
)

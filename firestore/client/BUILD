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
sed -e 's/@FIRESTORE_CLIENT_VERSION_MAJOR@/0/' \
    -e 's/@FIRESTORE_CLIENT_VERSION_MINOR@/2/' \
    -e 's/@FIRESTORE_CLIENT_VERSION_PATCH@/0/' < $< > $@
""",
)

load(":firestore_client.bzl", "firestore_client_SRCS", "firestore_client_HDRS")
cc_library(
    name = "firestore_client",
    srcs = firestore_client_SRCS,
    hdrs = firestore_client_HDRS + [ "version_info.h" ],
    deps = [
        ":nlohmann_json",
        ":libcurl",
        "//google/cloud:google_cloud_cpp_common",
    ],
)

load(":firestore_client_unit_tests.bzl", "firestore_client_unit_tests")
[cc_test(
    name = "firestore_client_" + test.replace('/', '_').replace('.cc', ''),
    srcs = [test],
    deps = [
        ":firestore_client",
        "//google/cloud:google_cloud_cpp_common",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
    linkopts = [ "-lpthread" ],
) for test in firestore_client_unit_tests]

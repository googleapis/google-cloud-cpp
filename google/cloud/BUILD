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
sed -e "s;@CMAKE_CXX_COMPILER_ID@;$(C_COMPILER);" \
    -e "s;@CMAKE_CXX_COMPILER_VERSION@;unknown-bazel;" \
    -e "s;@CMAKE_CXX_FLAGS@;$(CC_FLAGS);" \
    -e "s;\\$${CMAKE_CXX_FLAGS_.*};$(COMPILATION_MODE);" \
    -e "s;@GIT_HEAD@;unknown;" < $< > $@
  """,
)

genrule(
    name = "generate_version_info",
    srcs = ["internal/version_info.h.in"],
    outs = ["internal/version_info.h"],
    cmd = """
sed -e 's/@GOOGLE_CLOUD_CPP_VERSION_MAJOR@/0/' \
    -e 's/@GOOGLE_CLOUD_CPP_VERSION_MINOR@/1/' \
    -e 's/@GOOGLE_CLOUD_CPP_VERSION_PATCH@/0/' < $< > $@
""",
)

load(":google_cloud_cpp_common.bzl", "google_cloud_cpp_common_SRCS", "google_cloud_cpp_common_HDRS")
cc_library(
    name = "google_cloud_cpp_common",
    srcs = google_cloud_cpp_common_SRCS + [ "internal/build_info.cc" ],
    hdrs = google_cloud_cpp_common_HDRS + [ "internal/version_info.h" ],
    deps = [],
)

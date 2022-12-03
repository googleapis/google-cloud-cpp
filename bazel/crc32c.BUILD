# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # BSD

crc32c_arm64_HDRS = [
    "src/crc32c_arm64.h",
]

crc32c_arm64_SRCS = [
    "src/crc32c_arm64.cc",
]

crc32c_sse42_HDRS = [
    "src/crc32c_sse42.h",
]

crc32c_sse42_SRCS = [
    "src/crc32c_sse42.cc",
]

crc32c_HDRS = [
    "src/crc32c_arm64.h",
    "src/crc32c_arm64_check.h",
    "src/crc32c_internal.h",
    "src/crc32c_prefetch.h",
    "src/crc32c_read_le.h",
    "src/crc32c_round_up.h",
    "src/crc32c_sse42.h",
    "src/crc32c_sse42_check.h",
    "include/crc32c/crc32c.h",
]

crc32c_SRCS = [
    "src/crc32c_portable.cc",
    "src/crc32c.cc",
]

config_setting(
    name = "windows",
    constraint_values = [
        "@platforms//os:windows",
        "@platforms//cpu:x86_64",
    ],
    visibility = ["//visibility:public"],
)

config_setting(
    name = "linux_x86_64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
    visibility = ["//visibility:public"],
)

config_setting(
    name = "macos",
    constraint_values = [
        "@platforms//os:macos",
    ],
    visibility = ["//visibility:public"],
)

sse42_copts = select({
    ":windows": ["/arch:AVX"],
    ":linux_x86_64": ["-msse4.2"],
    ":macos": ["-msse4.2"],
    "//conditions:default": [],
})

sse42_enabled = select({
    ":windows": "1",
    ":linux_x86_64": "1",
    ":macos": "1",
    "//conditions:default": "0",
})

load(":configure_template.bzl", "configure_template")

configure_template(
    name = "generate_config",
    output = "crc32c/crc32c_config.h",
    substitutions = {
        "#cmakedefine01": "#define",
        " BYTE_ORDER_BIG_ENDIAN": " BYTE_ORDER_BIG_ENDIAN 0",
        " HAVE_BUILTIN_PREFETCH": " HAVE_BUILTIN_PREFETCH 0",
        " HAVE_MM_PREFETCH": " HAVE_MM_PREFETCH 0",
        " HAVE_SSE42": " HAVE_SSE42 1",
        " HAVE_ARM64_CRC32C": " HAVE_ARM64_CRC32C 0",
        " HAVE_STRONG_GETAUXVAL": " HAVE_STRONG_GETAUXVAL 0",
        " HAVE_WEAK_GETAUXVAL": " HAVE_WEAK_GETAUXVAL 0",
        " CRC32C_TESTS_BUILT_WITH_GLOG": " CRC32C_TESTS_BUILT_WITH_GLOG 0",
    },
    template = "src/crc32c_config.h.in",
)

cc_library(
    name = "crc32c",
    srcs = crc32c_SRCS + crc32c_sse42_SRCS + crc32c_arm64_SRCS,
    hdrs = crc32c_HDRS + ["crc32c/crc32c_config.h"],
    copts = sse42_copts,
    includes = ["include"],
    deps = [],
)

crc32c_test_sources = [
    "src/crc32c_arm64_unittest.cc",
    "src/crc32c_extend_unittests.h",
    "src/crc32c_portable_unittest.cc",
    "src/crc32c_prefetch_unittest.cc",
    "src/crc32c_read_le_unittest.cc",
    "src/crc32c_round_up_unittest.cc",
    "src/crc32c_sse42_unittest.cc",
    "src/crc32c_unittest.cc",
    "src/crc32c_test_main.cc",
]

cc_test(
    name = "crc32c_test",
    srcs = crc32c_test_sources,
    deps = [
        ":crc32c",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

exports_files([
    "LICENSE",
])

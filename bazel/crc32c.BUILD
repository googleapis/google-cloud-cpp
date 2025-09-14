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

load("@bazel_skylib//lib:selects.bzl", "selects")
load(":configure_template.bzl", "configure_template")

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

# select() cannot be use recursively, need to introduce temporary conjunctions.
selects.config_setting_group(
    name = "linux_sse42",
    match_all = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
)

selects.config_setting_group(
    name = "macos_sse42",
    match_all = [
        "@platforms//os:macos",
        "@platforms//cpu:x86_64",
    ],
)

selects.config_setting_group(
    name = "windows_sse42",
    match_all = [
        "@platforms//os:windows",
        "@platforms//cpu:x86_64",
    ],
)

crc32c_copts = select({
    ":linux_sse42": ["-msse4.2"],
    ":macos_sse42": ["-msse4.2"],
    ":windows_sse42": ["/arch:AVX"],
    # No special flags are needed for ARM64+CRC32C.
    "//conditions:default": [],
})

configure_template(
    name = "generate_config",
    output = "crc32c/crc32c_config.h",
    substitutions = {
        "#cmakedefine01": "#define",
        # x86_64 is always little endian. ARM64 is bi-endian. Fortunately, macOS
        # and Windows only use little endian. We assume the same is true for
        # Linux.
        #    https://devblogs.microsoft.com/oldnewthing/20220726-00
        #    https://developer.apple.com/documentation/apple-silicon/porting-your-macos-apps-to-apple-silicon
        " BYTE_ORDER_BIG_ENDIAN": " BYTE_ORDER_BIG_ENDIAN 0",
        " HAVE_BUILTIN_PREFETCH": " HAVE_BUILTIN_PREFETCH 0",
        " HAVE_MM_PREFETCH": " HAVE_MM_PREFETCH 0",
        " HAVE_STRONG_GETAUXVAL": " HAVE_STRONG_GETAUXVAL 0",
        " HAVE_WEAK_GETAUXVAL": " HAVE_WEAK_GETAUXVAL 0",
        " CRC32C_TESTS_BUILT_WITH_GLOG": " CRC32C_TESTS_BUILT_WITH_GLOG 0",
    } | select({
        # We assume all x86_64 CPUs support SSE4.2. This seems reasonably safe
        # in 2023.
        "@platforms//cpu:x86_64": {
            " HAVE_SSE42": " HAVE_SSE42 1",
            " HAVE_ARM64_CRC32C": " HAVE_ARM64_CRC32C 0",
        },
        # We assume all ARM64 CPUs support CRC32C extensions. This seems
        # reasonably safe for workstations and servers in 2023, and we do not
        # target mobile platforms at the moment.
        "@platforms//cpu:arm64": {
            " HAVE_SSE42": " HAVE_SSE42 0",
            " HAVE_ARM64_CRC32C": " HAVE_ARM64_CRC32C 1",
        },
        "//conditions:default": {},
    }),
    template = "src/crc32c_config.h.in",
)

cc_library(
    name = "crc32c",
    srcs = crc32c_SRCS + crc32c_sse42_SRCS + crc32c_arm64_SRCS,
    hdrs = crc32c_HDRS + ["crc32c/crc32c_config.h"],
    copts = crc32c_copts + select({
        "@platforms//os:macos": [
            "-msse4.2",
            "-mcrc32",
        ],
        "//conditions:default": [],
    }),
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

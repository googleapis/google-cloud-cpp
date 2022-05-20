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

"""Load dependencies needed to compile and test the google-cloud-cpp library."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")

def google_cloud_cpp_deps():
    """Loads dependencies need to compile the google-cloud-cpp library.

    Application developers can call this function from their WORKSPACE file
    to obtain all the necessary dependencies for google-cloud-cpp, including
    gRPC and its dependencies. This function only loads dependencies that
    have not been previously loaded, allowing application developers to
    override the version of the dependencies they want to use.
    """

    # Load platforms, we use it directly
    if "platforms" not in native.existing_rules():
        http_archive(
            name = "platforms",
            urls = [
                "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.5/platforms-0.0.5.tar.gz",
                "https://github.com/bazelbuild/platforms/releases/download/0.0.5/platforms-0.0.5.tar.gz",
            ],
            sha256 = "379113459b0feaf6bfbb584a91874c065078aa673222846ac765f86661c27407",
        )

    # Load rules_cc, used by googletest
    if "rules_cc" not in native.existing_rules():
        http_archive(
            name = "rules_cc",
            urls = [
                "https://github.com/bazelbuild/rules_cc/releases/download/0.0.1/rules_cc-0.0.1.tar.gz",
            ],
            sha256 = "4dccbfd22c0def164c8f47458bd50e0c7148f3d92002cdb459c2a96a68498241",
        )

    # Load Abseil
    if "com_google_absl" not in native.existing_rules():
        http_archive(
            name = "com_google_absl",
            strip_prefix = "abseil-cpp-20211102.0",
            urls = [
                "https://github.com/abseil/abseil-cpp/archive/20211102.0.tar.gz",
            ],
            sha256 = "dcf71b9cba8dc0ca9940c4b316a0c796be8fab42b070bb6b7cab62b48f0e66c4",
        )

    # Load a version of googletest that we know works.
    if "com_google_googletest" not in native.existing_rules():
        http_archive(
            name = "com_google_googletest",
            strip_prefix = "googletest-release-1.11.0",
            urls = [
                "https://github.com/google/googletest/archive/release-1.11.0.tar.gz",
            ],
            sha256 = "b4870bf121ff7795ba20d20bcdd8627b8e088f2d1dab299a031c1034eddc93d5",
        )

    # Load a version of benchmark that we know works.
    if "com_google_benchmark" not in native.existing_rules():
        http_archive(
            name = "com_google_benchmark",
            strip_prefix = "benchmark-1.6.1",
            urls = [
                "https://github.com/google/benchmark/archive/v1.6.1.tar.gz",
            ],
            sha256 = "6132883bc8c9b0df5375b16ab520fac1a85dc9e4cf5be59480448ece74b278d4",
        )

    # Load the googleapis dependency.
    if "com_google_googleapis" not in native.existing_rules():
        http_archive(
            name = "com_google_googleapis",
            urls = [
                "https://github.com/googleapis/googleapis/archive/cfe613f3dd717b137f2384b2a1a685c2f48f1d05.tar.gz",
            ],
            strip_prefix = "googleapis-cfe613f3dd717b137f2384b2a1a685c2f48f1d05",
            sha256 = "176247ca11c90f41be1267dffe90a5e2a5d8f232de2c56bbc24373c2d52a79a2",
            build_file = "@com_github_googleapis_google_cloud_cpp//bazel:googleapis.BUILD",
            # Scaffolding for patching googleapis after download. For example:
            #   patches = ["googleapis.patch"]
            # NOTE: This should only be used while developing with a new
            # protobuf message. No changes to `patches` should ever be
            # committed to the main branch.
            patch_tool = "patch",
            patch_args = ["-p1"],
            patches = [],
        )

    # Load protobuf.
    if "com_google_protobuf" not in native.existing_rules():
        http_archive(
            name = "com_google_protobuf",
            strip_prefix = "protobuf-3.20.1",
            urls = [
                "https://github.com/protocolbuffers/protobuf/archive/v3.20.1.tar.gz",
            ],
            sha256 = "8b28fdd45bab62d15db232ec404248901842e5340299a57765e48abe8a80d930",
        )

    # Load BoringSSL, this is used by gRPC, but as I write this (2021-06-03, circa gRPC-1.37.1), the version used by
    # gRPC does not compile with GCC-11
    if "boringssl" not in native.existing_rules():
        http_archive(
            name = "boringssl",
            # Use github mirror instead of https://boringssl.googlesource.com/boringssl
            # to obtain a boringssl archive with consistent sha256
            sha256 = "f0f433106f98f6f50ed6bbc2169f7c42dd73d13d0f09d431082519b5649903a6",
            strip_prefix = "boringssl-56eb68f64215738552be452e311da12047934ab4",
            urls = [
                "https://github.com/google/boringssl/archive/56eb68f64215738552be452e311da12047934ab4.tar.gz",
            ],
        )

    # Load gRPC and its dependencies, using a similar pattern to this function.
    if "com_github_grpc_grpc" not in native.existing_rules():
        http_archive(
            name = "com_github_grpc_grpc",
            strip_prefix = "grpc-1.46.3",
            urls = [
                "https://github.com/grpc/grpc/archive/v1.46.3.tar.gz",
            ],
            sha256 = "d6cbf22cb5007af71b61c6be316a79397469c58c82a942552a62e708bce60964",
        )

    # We use the cc_proto_library() rule from @com_google_protobuf, which
    # assumes that grpc_cpp_plugin and grpc_lib are in the //external: module
    native.bind(
        name = "grpc_cpp_plugin",
        actual = "@com_github_grpc_grpc//src/compiler:grpc_cpp_plugin",
    )

    native.bind(
        name = "grpc_lib",
        actual = "@com_github_grpc_grpc//:grpc++",
    )

    # We need libcurl for the Google Cloud Storage client.
    if "com_github_curl_curl" not in native.existing_rules():
        http_archive(
            name = "com_github_curl_curl",
            urls = [
                "https://curl.haxx.se/download/curl-7.69.1.tar.gz",
            ],
            strip_prefix = "curl-7.69.1",
            sha256 = "01ae0c123dee45b01bbaef94c0bc00ed2aec89cb2ee0fd598e0d302a6b5e0a98",
            build_file = "@com_github_googleapis_google_cloud_cpp//bazel:curl.BUILD",
        )

    # We need the nlohmann_json library
    if "com_github_nlohmann_json" not in native.existing_rules():
        http_archive(
            name = "com_github_nlohmann_json",
            urls = [
                "https://github.com/nlohmann/json/releases/download/v3.10.5/include.zip",
            ],
            sha256 = "b94997df68856753b72f0d7a3703b7d484d4745c567f3584ef97c96c25a5798e",
            build_file = "@com_github_googleapis_google_cloud_cpp//bazel:nlohmann_json.BUILD",
        )

    # Load google/crc32c, a library to efficiently compute CRC32C checksums.
    if "com_github_google_crc32c" not in native.existing_rules():
        http_archive(
            name = "com_github_google_crc32c",
            strip_prefix = "crc32c-1.1.2",
            urls = [
                "https://github.com/google/crc32c/archive/1.1.2.tar.gz",
            ],
            sha256 = "ac07840513072b7fcebda6e821068aa04889018f24e10e46181068fb214d7e56",
            build_file = "@com_github_googleapis_google_cloud_cpp//bazel:crc32c.BUILD",
        )

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
                "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.6/platforms-0.0.6.tar.gz",
                "https://github.com/bazelbuild/platforms/releases/download/0.0.6/platforms-0.0.6.tar.gz",
            ],
            sha256 = "5308fc1d8865406a49427ba24a9ab53087f17f5266a7aabbfc28823f3916e1ca",
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
            strip_prefix = "abseil-cpp-20230125.0",
            urls = [
                "https://github.com/abseil/abseil-cpp/archive/20230125.0.tar.gz",
            ],
            sha256 = "3ea49a7d97421b88a8c48a0de16c16048e17725c7ec0f1d3ea2683a2a75adc21",
        )

    # Load a version of googletest that we know works.
    if "com_google_googletest" not in native.existing_rules():
        http_archive(
            name = "com_google_googletest",
            strip_prefix = "googletest-release-1.12.1",
            urls = [
                "https://github.com/google/googletest/archive/release-1.12.1.tar.gz",
            ],
            sha256 = "81964fe578e9bd7c94dfdb09c8e4d6e6759e19967e397dbea48d1c10e45d0df2",
        )

    # Load a version of benchmark that we know works.
    if "com_google_benchmark" not in native.existing_rules():
        http_archive(
            name = "com_google_benchmark",
            strip_prefix = "benchmark-1.7.0",
            urls = [
                "https://github.com/google/benchmark/archive/v1.7.0.tar.gz",
            ],
            sha256 = "3aff99169fa8bdee356eaa1f691e835a6e57b1efeadb8a0f9f228531158246ac",
        )

    # Load the googleapis dependency.
    if "com_google_googleapis" not in native.existing_rules():
        http_archive(
            name = "com_google_googleapis",
            urls = [
                "https://github.com/googleapis/googleapis/archive/d6e9adbf6646748cb41ca6aa61a78a70a1d83ff5.tar.gz",
                "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_googleapis/d6e9adbf6646748cb41ca6aa61a78a70a1d83ff5.tar.gz",
            ],
            strip_prefix = "googleapis-d6e9adbf6646748cb41ca6aa61a78a70a1d83ff5",
            sha256 = "df6715a8f21465a797cceb4722aeee1d711a7fc06fa68330e1382f06baa1e93c",
            build_file = Label("//bazel:googleapis.BUILD"),
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
            strip_prefix = "protobuf-21.12",
            urls = [
                "https://github.com/protocolbuffers/protobuf/archive/v21.12.tar.gz",
            ],
            sha256 = "22fdaf641b31655d4b2297f9981fa5203b2866f8332d3c6333f6b0107bb320de",
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
            strip_prefix = "grpc-1.51.1",
            urls = [
                "https://github.com/grpc/grpc/archive/v1.51.1.tar.gz",
            ],
            sha256 = "b55696fb249669744de3e71acc54a9382bea0dce7cd5ba379b356b12b82d4229",
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
            build_file = Label("//bazel:curl.BUILD"),
        )

    # We need the nlohmann_json library
    if "com_github_nlohmann_json" not in native.existing_rules():
        http_archive(
            name = "com_github_nlohmann_json",
            urls = [
                "https://github.com/nlohmann/json/releases/download/v3.11.2/include.zip",
            ],
            sha256 = "e5c7a9f49a16814be27e4ed0ee900ecd0092bfb7dbfca65b5a421b774dccaaed",
            build_file = Label("//bazel:nlohmann_json.BUILD"),
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
            build_file = Label("//bazel:crc32c.BUILD"),
            patch_tool = "patch",
            patch_args = ["-p1"],
            patches = [Label("//bazel:configure_template.bzl.patch")],
        )

    # Open Telemetry
    if "io_opentelemetry_cpp" not in native.existing_rules():
        http_archive(
            name = "io_opentelemetry_cpp",
            strip_prefix = "opentelemetry-cpp-d5571916e18abbd994373cdb633be0815deda319",
            urls = [
                "https://github.com/open-telemetry/opentelemetry-cpp/archive/d5571916e18abbd994373cdb633be0815deda319.tar.gz",
            ],
            sha256 = "051cbbc3b76e0459371cdcf4044a1e5182a686a443d03f9dbcec6bb7d4e2aa46",
        )

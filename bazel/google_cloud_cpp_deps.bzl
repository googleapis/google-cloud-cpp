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

    # Load rules_cc, used by googletest
    if "rules_cc" not in native.existing_rules():
        http_archive(
            name = "rules_cc",
            strip_prefix = "rules_cc-a508235df92e71d537fcbae0c7c952ea6957a912",
            urls = [
                "https://github.com/bazelbuild/rules_cc/archive/a508235df92e71d537fcbae0c7c952ea6957a912.tar.gz",
            ],
            sha256 = "d21d38c4b8e81eed8fa95ede48dd69aba01a3b938be6ac03d2b9dc61886a7183",
        )

    # Load a version of googletest that we know works.
    if "com_google_googletest" not in native.existing_rules():
        http_archive(
            name = "com_google_googletest",
            strip_prefix = "googletest-release-1.10.0",
            urls = [
                "https://github.com/google/googletest/archive/release-1.10.0.tar.gz",
            ],
            sha256 = "9dc9157a9a1551ec7a7e43daea9a694a0bb5fb8bec81235d8a1e6ef64c716dcb",
        )

    # Load the googleapis dependency.
    if "com_google_googleapis" not in native.existing_rules():
        http_archive(
            name = "com_google_googleapis",
            urls = [
                "https://github.com/googleapis/googleapis/archive/fea22b1d9f27f86ef355c1d0dba00e0791a08a19.tar.gz",
            ],
            strip_prefix = "googleapis-fea22b1d9f27f86ef355c1d0dba00e0791a08a19",
            sha256 = "957ef432cdedbace1621bb023e6d8637ecbaa78856b3fc6e299f9b277ae990ff",
            build_file = "@com_github_googleapis_google_cloud_cpp//bazel:googleapis.BUILD",
        )

    # Load protobuf.
    if "com_google_protobuf" not in native.existing_rules():
        http_archive(
            name = "com_google_protobuf",
            strip_prefix = "protobuf-3.11.3",
            urls = [
                "https://github.com/google/protobuf/archive/v3.11.3.tar.gz",
            ],
            sha256 = "cf754718b0aa945b00550ed7962ddc167167bd922b842199eeb6505e6f344852",
        )

    # Load opencensus.
    if "io_opencensus_cpp" not in native.existing_rules():
        http_archive(
            name = "io_opencensus_cpp",
            urls = [
                "https://github.com/census-instrumentation/opencensus-cpp/archive/c900c4d723ef596f357b0e695e68e2fa725eec90.tar.gz",
            ],
            strip_prefix = "opencensus-cpp-c900c4d723ef596f357b0e695e68e2fa725eec90",
            sha256 = "040753b92d0ea57e2e9ffe7fc51f4c954cfb352204dc967c04ae6100842d6f45",
        )

    # Load gRPC and its dependencies, using a similar pattern to this function.
    if "com_github_grpc_grpc" not in native.existing_rules():
        http_archive(
            name = "com_github_grpc_grpc",
            strip_prefix = "grpc-78ace4cd5dfcc1f2eced44d22d752f103f377e7b",
            urls = [
                "https://github.com/grpc/grpc/archive/78ace4cd5dfcc1f2eced44d22d752f103f377e7b.tar.gz",
            ],
            sha256 = "a2034a1c8127e35c0cc7b86c1b5ad6d8e79a62c5e133c379b8b22a78ba370015",
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
    if "com_github_nlohmann_json_single_header" not in native.existing_rules():
        http_file(
            name = "com_github_nlohmann_json_single_header",
            urls = [
                "https://github.com/nlohmann/json/releases/download/v3.4.0/json.hpp",
            ],
            sha256 = "63da6d1f22b2a7bb9e4ff7d6b255cf691a161ff49532dcc45d398a53e295835f",
        )

    # Load google/crc32c, a library to efficiently compute CRC32C checksums.
    if "com_github_google_crc32c" not in native.existing_rules():
        http_archive(
            name = "com_github_google_crc32c",
            strip_prefix = "crc32c-1.0.6",
            urls = [
                "https://github.com/google/crc32c/archive/1.0.6.tar.gz",
            ],
            sha256 = "6b3b1d861bb8307658b2407bc7a4c59e566855ef5368a60b35c893551e4788e9",
            build_file = "@com_github_googleapis_google_cloud_cpp//bazel:crc32c.BUILD",
        )

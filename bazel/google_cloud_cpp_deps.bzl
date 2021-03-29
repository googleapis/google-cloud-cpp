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

    # Load Abseil
    if "com_google_absl" not in native.existing_rules():
        http_archive(
            name = "com_google_absl",
            strip_prefix = "abseil-cpp-20200923.3",
            urls = [
                "https://github.com/abseil/abseil-cpp/archive/20200923.3.tar.gz",
            ],
            sha256 = "ebe2ad1480d27383e4bf4211e2ca2ef312d5e6a09eba869fd2e8a5c5d553ded2",
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

    # Load a version of benchmark that we know works.
    if "com_google_benchmark" not in native.existing_rules():
        http_archive(
            name = "com_google_benchmark",
            strip_prefix = "benchmark-1.5.0",
            urls = [
                "https://github.com/google/benchmark/archive/v1.5.0.tar.gz",
            ],
            sha256 = "3c6a165b6ecc948967a1ead710d4a181d7b0fbcaa183ef7ea84604994966221a",
        )

    # Load the googleapis dependency.
    if "com_google_googleapis" not in native.existing_rules():
        http_archive(
            name = "com_google_googleapis",
            urls = [
                "https://github.com/googleapis/googleapis/archive/6ce40ff8faf68226782f507ca6b2d497a77044de.tar.gz",
            ],
            strip_prefix = "googleapis-6ce40ff8faf68226782f507ca6b2d497a77044de",
            sha256 = "6762083f829f998c3971efa2ba858c21d4ac4ba77feb9650bad7d358e3add2a5",
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
            strip_prefix = "protobuf-3.14.0",
            urls = [
                "https://github.com/google/protobuf/archive/v3.14.0.tar.gz",
            ],
            sha256 = "d0f5f605d0d656007ce6c8b5a82df3037e1d8fe8b121ed42e536f569dec16113",
        )

    # Load gRPC and its dependencies, using a similar pattern to this function.
    if "com_github_grpc_grpc" not in native.existing_rules():
        http_archive(
            name = "com_github_grpc_grpc",
            strip_prefix = "grpc-1.35.0",
            urls = [
                "https://github.com/grpc/grpc/archive/v1.35.0.tar.gz",
            ],
            sha256 = "27dd2fc5c9809ddcde8eb6fa1fa278a3486566dfc28335fca13eb8df8bd3b958",
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
                "https://github.com/nlohmann/json/releases/download/v3.9.0/include.zip",
            ],
            sha256 = "5b9b819aed31626aefe2eace23498cafafc1691890556cd36d2a8002f6905009",
            build_file = "@com_github_googleapis_google_cloud_cpp//bazel:nlohmann_json.BUILD",
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

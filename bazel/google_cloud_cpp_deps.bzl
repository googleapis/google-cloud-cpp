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

    # Load a newer version of google test than what gRPC does.
    if "com_google_googletest" not in native.existing_rules():
        http_archive(
            name = "com_google_googletest",
            strip_prefix = "googletest-release-1.8.1",
            urls = [
                "https://github.com/google/googletest/archive/release-1.8.1.tar.gz",
            ],
            sha256 = "9bf1fe5182a604b4135edc1a425ae356c9ad15e9b23f9f12a02e80184c3a249c",
        )

    # Load the googleapis dependency.
    if "com_github_googleapis_googleapis" not in native.existing_rules():
        http_archive(
            name = "com_github_googleapis_googleapis",
            urls = [
                "https://github.com/google/googleapis/archive/6a3277c0656219174ff7c345f31fb20a90b30b97.zip",
            ],
            strip_prefix = "googleapis-6a3277c0656219174ff7c345f31fb20a90b30b97",
            sha256 = "82ba91a41fb01305de4e8805c0a9270ed2035007161aa5a4ec60f887a499f5e9",
            build_file = "@com_github_googlecloudplatform_google_cloud_cpp//bazel:googleapis.BUILD",
        )

    # Load gRPC and its dependencies, using a similar pattern to this function.
    # This implictly loads "com_google_protobuf", which we use.
    if "com_github_grpc_grpc" not in native.existing_rules():
        http_archive(
            name = "com_github_grpc_grpc",
            strip_prefix = "grpc-1.17.2",
            urls = [
                "https://github.com/grpc/grpc/archive/v1.17.2.tar.gz",
                "https://mirror.bazel.build/github.com/grpc/grpc/archive/v1.17.2.tar.gz",
            ],
            sha256 = "34ed95b727e7c6fcbf85e5eb422e962788e21707b712fdb4caf931553c2c6dbc",
        )

    # Load OpenCensus.
    if "io_opencensus_cpp" not in native.existing_rules():
        http_archive(
            name = "io_opencensus_cpp",
            strip_prefix = "opencensus-cpp-893e0835a45d749221f049d0d167e157b67b6d9c",
            urls = [
                "https://github.com/census-instrumentation/opencensus-cpp/archive" +
                "/893e0835a45d749221f049d0d167e157b67b6d9c.tar.gz",
            ],
            sha256 = "8e2bddd3ea6d747a8c4255c73dcea1b9fcdf1560f3bb9ff96bcb20d4d207235e",
        )

    # We need libcurl for the Google Cloud Storage client.
    if "com_github_curl_curl" not in native.existing_rules():
        http_archive(
            name = "com_github_curl_curl",
            urls = [
                "https://mirror.bazel.build/curl.haxx.se/download/curl-7.60.0.tar.gz",
                "https://curl.haxx.se/download/curl-7.60.0.tar.gz",
            ],
            strip_prefix = "curl-7.60.0",
            sha256 = "e9c37986337743f37fd14fe8737f246e97aec94b39d1b71e8a5973f72a9fc4f5",
            build_file = "@com_github_googlecloudplatform_google_cloud_cpp//bazel:curl.BUILD",
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
            build_file = "@com_github_googlecloudplatform_google_cloud_cpp//bazel:crc32c.BUILD",
        )

    # We use the cc_proto_library() rule from @com_google_protobuf, which
    # assumes that grpc_cpp_plugin and grpc_lib are in the //external: module
    native.bind(
        name = "grpc_cpp_plugin",
        actual = "@com_github_grpc_grpc//:grpc_cpp_plugin",
    )

    native.bind(
        name = "grpc_lib",
        actual = "@com_github_grpc_grpc//:grpc++",
    )

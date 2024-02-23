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

"""Load dependencies needed to use the google-cloud-cpp libraries."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def gl_cpp_workspace0(name = None):
    """Loads dependencies need to compile the google-cloud-cpp libraries.

    Application developers can call this function from their WORKSPACE file
    to obtain all the necessary dependencies for google-cloud-cpp, including
    gRPC and its dependencies. This function only loads dependencies that
    have not been previously loaded, allowing application developers to
    override the version of the dependencies they want to use.

    Note that some dependencies have their own `*_deps()` function. These
    functions can only be called after load()-ing the right .bzl file. Bazel
    does not permit interleaving load() and function calls in the same function
    or even the same `.bzl` file. Only the project's `WORKSPACE` file can
    interleave these calls.

    To workaround this limitation in Bazel we offer a series of helpers that can
    be loaded and called in sequence:

    ```
    http_archive(name = "google_cloud_cpp", ...)
    load("@google_cloud_cpp//bazel:workspace0.bzl", "gl_cpp_workspace0")
    gl_cpp_workspace0()
    load("@google_cloud_cpp//bazel:workspace1.bzl", "gl_cpp_workspace1")
    gl_cpp_workspace1()
    load("@google_cloud_cpp//bazel:workspace2.bzl", "gl_cpp_workspace2")
    gl_cpp_workspace2()
    load("@google_cloud_cpp//bazel:workspace3.bzl", "gl_cpp_workspace3")
    gl_cpp_workspace3()
    load("@google_cloud_cpp//bazel:workspace4.bzl", "gl_cpp_workspace4")
    gl_cpp_workspace4()
    load("@google_cloud_cpp//bazel:workspace5.bzl", "gl_cpp_workspace5")
    gl_cpp_workspace5()
    ```

    Args:
        name: Unused. It is conventional to provide a `name` argument to all
            workspace functions.
    """

    # Load platforms, we use it directly
    maybe(
        http_archive,
        name = "platforms",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/platforms/platforms-0.0.8.tar.gz",
            "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.8/platforms-0.0.8.tar.gz",
            "https://github.com/bazelbuild/platforms/releases/download/0.0.8/platforms-0.0.8.tar.gz",
        ],
        sha256 = "8150406605389ececb6da07cbcb509d5637a3ab9a24bc69b1101531367d89d74",
    )

    # Load rules_cc, used by googletest
    maybe(
        http_archive,
        name = "rules_cc",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/rules_cc/rules_cc-0.0.9.tar.gz",
            "https://github.com/bazelbuild/rules_cc/releases/download/0.0.9/rules_cc-0.0.9.tar.gz",
        ],
        sha256 = "2037875b9a4456dce4a79d112a8ae885bbc4aad968e6587dca6e64f3a0900cdf",
        strip_prefix = "rules_cc-0.0.9",
    )

    # The version of `rules_apple` loaded by gRPC is too old for Bazel 7.
    maybe(
        http_archive,
        name = "build_bazel_rules_apple",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/build_bazel_rules_apple/rules_apple.3.3.0.tar.gz",
            "https://github.com/bazelbuild/rules_apple/releases/download/3.3.0/rules_apple.3.3.0.tar.gz",
        ],
        sha256 = "65eafafe94b8573e74160b7f587d091a0fa34d69e6d2c41c4afb1eef140383ec",
    )

    # Load Abseil
    maybe(
        http_archive,
        name = "com_google_absl",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_absl/20240116.1.tar.gz",
            "https://github.com/abseil/abseil-cpp/archive/20240116.1.tar.gz",
        ],
        sha256 = "3c743204df78366ad2eaf236d6631d83f6bc928d1705dd0000b872e53b73dc6a",
        strip_prefix = "abseil-cpp-20240116.1",
    )

    # Load a version of googletest that we know works. This is needed to create
    # //:.*mocks targets, which are public.
    maybe(
        http_archive,
        name = "com_google_googletest",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_googletest/v1.14.0.tar.gz",
            "https://github.com/google/googletest/archive/v1.14.0.tar.gz",
        ],
        sha256 = "8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7",
        strip_prefix = "googletest-1.14.0",
    )

    # Load the googleapis dependency.
    maybe(
        http_archive,
        name = "com_google_googleapis",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_googleapis/87a71a910ce60009cc578eb183062fb7d62e664f.tar.gz",
            "https://github.com/googleapis/googleapis/archive/87a71a910ce60009cc578eb183062fb7d62e664f.tar.gz",
        ],
        sha256 = "a74af4c942881ac10b3815ccd102d29e030a68a8e2dc73d7d2c7333d7b26a4c0",
        strip_prefix = "googleapis-87a71a910ce60009cc578eb183062fb7d62e664f",
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
    maybe(
        http_archive,
        name = "com_google_protobuf",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_protobuf/v25.3.tar.gz",
            "https://github.com/protocolbuffers/protobuf/archive/v25.3.tar.gz",
        ],
        sha256 = "d19643d265b978383352b3143f04c0641eea75a75235c111cc01a1350173180e",
        strip_prefix = "protobuf-25.3",
    )

    # Load BoringSSL. This could be automatically loaded by gRPC. But as of
    # 2023-02-01 the version loaded by gRPC-1.51 does not compile with Clang-15.
    maybe(
        http_archive,
        name = "boringssl",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/boringssl/82a53d8c902f940eb1310f76a0b96c40c67f632f.tar.gz",
            # Use https://github.com/google/boringssl instead of
            # https://boringssl.googlesource.com/boringssl as the
            # former has a (more) consistent sha256.
            "https://github.com/google/boringssl/archive/82a53d8c902f940eb1310f76a0b96c40c67f632f.tar.gz",
        ],
        sha256 = "c25e5c1ac36fa6709b2fd9095584228d48e9f82bcf97d8cd868bcbe796f90ba5",
        strip_prefix = "boringssl-82a53d8c902f940eb1310f76a0b96c40c67f632f",
    )

    # Load gRPC and its dependencies, using a similar pattern to this function.
    maybe(
        http_archive,
        name = "com_github_grpc_grpc",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_github_grpc_grpc/v1.62.0.tar.gz",
            "https://github.com/grpc/grpc/archive/v1.62.0.tar.gz",
        ],
        sha256 = "f40bde4ce2f31760f65dc49a2f50876f59077026494e67dccf23992548b1b04f",
        strip_prefix = "grpc-1.62.0",
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
    maybe(
        http_archive,
        name = "com_github_curl_curl",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_github_curl_curl/curl-7.69.1.tar.gz",
            "https://curl.haxx.se/download/curl-7.69.1.tar.gz",
        ],
        sha256 = "01ae0c123dee45b01bbaef94c0bc00ed2aec89cb2ee0fd598e0d302a6b5e0a98",
        strip_prefix = "curl-7.69.1",
        build_file = Label("//bazel:curl.BUILD"),
    )

    # We need the nlohmann_json library
    maybe(
        http_archive,
        name = "com_github_nlohmann_json",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_github_nlohmann_json/v3.11.3.tar.gz",
            "https://github.com/nlohmann/json/archive/v3.11.3.tar.gz",
        ],
        sha256 = "0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406",
        strip_prefix = "json-3.11.3",
        build_file = Label("//bazel:nlohmann_json.BUILD"),
    )

    # Load google/crc32c, a library to efficiently compute CRC32C checksums.
    maybe(
        http_archive,
        name = "com_github_google_crc32c",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_github_google_crc32c/1.1.2.tar.gz",
            "https://github.com/google/crc32c/archive/1.1.2.tar.gz",
        ],
        sha256 = "ac07840513072b7fcebda6e821068aa04889018f24e10e46181068fb214d7e56",
        strip_prefix = "crc32c-1.1.2",
        build_file = Label("//bazel:crc32c.BUILD"),
        patch_tool = "patch",
        patch_args = ["-p1"],
        patches = [Label("//bazel:configure_template.bzl.patch")],
    )

    # Open Telemetry
    maybe(
        http_archive,
        name = "io_opentelemetry_cpp",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/io_opentelemetry_cpp/v1.14.0.tar.gz",
            "https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.14.0.tar.gz",
        ],
        sha256 = "9a67561b8f4dba8cf8bb323b404069cbcc2f59974e1330b1ba192c5c8d68c28e",
        strip_prefix = "opentelemetry-cpp-1.14.0",
        repo_mapping = {
            "@curl": "@com_github_curl_curl",
            "@com_github_google_benchmark": "@com_github_benchmark",
            "@github_nlohmann_json": "@com_github_nlohmann_json",
        },
    )

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
            "https://github.com/bazelbuild/platforms/releases/download/0.0.11/platforms-0.0.11.tar.gz",
        ],
        sha256 = "29742e87275809b5e598dc2f04d86960cc7a55b3067d97221c9abbc9926bff0f",
    )

    # Load rules_cc, used by googletest
    maybe(
        http_archive,
        name = "rules_cc",
        urls = [
            "https://github.com/bazelbuild/rules_cc/releases/download/0.0.17/rules_cc-0.0.17.tar.gz",
        ],
        sha256 = "abc605dd850f813bb37004b77db20106a19311a96b2da1c92b789da529d28fe1",
        strip_prefix = "rules_cc-0.0.17",
    )

    # protobuf requires this
    maybe(
        http_archive,
        name = "bazel_skylib",
        sha256 = "bc283cdfcd526a52c3201279cda4bc298652efa898b10b4db0837dc51652756f",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.7.1/bazel-skylib-1.7.1.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.7.1/bazel-skylib-1.7.1.tar.gz",
        ],
    )

    # The version of `rules_apple` loaded by gRPC is too old for Bazel 7.
    maybe(
        http_archive,
        name = "build_bazel_rules_apple",
        urls = [
            "https://github.com/bazelbuild/rules_apple/releases/download/3.22.0/rules_apple.3.22.0.tar.gz",
        ],
        sha256 = "a78f26c22ac8d6e3f3fcaad50eace4d9c767688bd7254b75bdf4a6735b299f6a",
    )

    # Load Abseil
    maybe(
        http_archive,
        name = "com_google_absl",
        urls = [
            "https://github.com/abseil/abseil-cpp/archive/20250127.1.tar.gz",
        ],
        sha256 = "b396401fd29e2e679cace77867481d388c807671dc2acc602a0259eeb79b7811",
        strip_prefix = "abseil-cpp-20250127.1",
    )

    # Load a version of googletest that we know works. This is needed to create
    # //:.*mocks targets, which are public.
    maybe(
        http_archive,
        name = "com_google_googletest",
        urls = [
            "https://github.com/google/googletest/archive/v1.16.0.tar.gz",
        ],
        sha256 = "78c676fc63881529bf97bf9d45948d905a66833fbfa5318ea2cd7478cb98f399",
        strip_prefix = "googletest-1.16.0",
    )

    # Load the googleapis dependency.
    maybe(
        http_archive,
        name = "com_google_googleapis",
        urls = [
            "https://github.com/googleapis/googleapis/archive/f01a17a560b4fbc888fd552c978f4e1f8614100b.tar.gz",
        ],
        sha256 = "be41dc99017f2fc7cccf115081947e354d33a37943ede9f5dd705b668bce7f29",
        strip_prefix = "googleapis-f01a17a560b4fbc888fd552c978f4e1f8614100b",
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
            "https://github.com/protocolbuffers/protobuf/archive/v29.4.tar.gz",
        ],
        sha256 = "6bd9dcc91b17ef25c26adf86db71c67ec02431dc92e9589eaf82e22889230496",
        strip_prefix = "protobuf-29.4",
    )

    # Load BoringSSL. This could be automatically loaded by gRPC. But as of
    # 2023-02-01 the version loaded by gRPC-1.51 does not compile with Clang-15.
    maybe(
        http_archive,
        name = "boringssl",
        urls = [
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
            "https://github.com/grpc/grpc/archive/v1.69.0.tar.gz",
        ],
        sha256 = "cd256d91781911d46a57506978b3979bfee45d5086a1b6668a3ae19c5e77f8dc",
        strip_prefix = "grpc-1.69.0",
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
            "https://github.com/nlohmann/json/archive/v3.11.3.tar.gz",
        ],
        sha256 = "0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406",
        strip_prefix = "json-3.11.3",
    )

    # Load google/crc32c, a library to efficiently compute CRC32C checksums.
    maybe(
        http_archive,
        name = "com_github_google_crc32c",
        urls = [
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
            "https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.20.0.tar.gz",
        ],
        sha256 = "4b6eeb852f075133c21b95948017f13a3e21740e55b921d27e42970a47314297",
        strip_prefix = "opentelemetry-cpp-1.20.0",
        repo_mapping = {
            "@curl": "@com_github_curl_curl",
            "@com_github_google_benchmark": "@com_github_benchmark",
            "@github_nlohmann_json": "@com_github_nlohmann_json",
        },
    )

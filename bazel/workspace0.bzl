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
            "https://github.com/bazelbuild/platforms/releases/download/1.0.0/platforms-1.0.0.tar.gz",
        ],
        sha256 = "3384eb1c30762704fbe38e440204e114154086c8fc8a8c2e3e28441028c019a8",
    )

    # Load rules_cc, used by googletest
    maybe(
        http_archive,
        name = "rules_cc",
        urls = [
            "https://github.com/bazelbuild/rules_cc/releases/download/0.1.4/rules_cc-0.1.4.tar.gz",
        ],
        sha256 = "0d3b4f984c4c2e1acfd1378e0148d35caf2ef1d9eb95b688f8e19ce0c41bdf5b",
        strip_prefix = "rules_cc-0.1.4",
    )

    maybe(
        http_archive,
        name = "com_envoyproxy_protoc_gen_validate",
        urls = [
            "https://github.com/bufbuild/protoc-gen-validate/archive/v1.2.1.tar.gz",
        ],
        strip_prefix = "protoc-gen-validate-1.2.1",
        integrity = "sha256-5HGDUnVN8Tk7h5K2MTOKqFYvOQ6BYHg+NlRUvBHZYyg=",
    )

    # protobuf requires this
    maybe(
        http_archive,
        name = "bazel_skylib",
        sha256 = "6e78f0e57de26801f6f564fa7c4a48dc8b36873e416257a92bbb0937eeac8446",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.8.2/bazel-skylib-1.8.2.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.8.2/bazel-skylib-1.8.2.tar.gz",
        ],
    )

    # The version of `rules_apple` loaded by gRPC is too old for Bazel 7.
    maybe(
        http_archive,
        name = "build_bazel_rules_apple",
        urls = [
            "https://github.com/bazelbuild/rules_apple/releases/download/4.2.0/rules_apple.4.2.0.tar.gz",
        ],
        sha256 = "ef8a5744b2ffff49f47647226f69f0f06522ca2e8a6fa1aaf5d65d5314813c34",
    )

    # Load Abseil
    maybe(
        http_archive,
        name = "abseil-cpp",
        urls = [
            "https://github.com/abseil/abseil-cpp/archive/20250512.1.tar.gz",
        ],
        sha256 = "9b7a064305e9fd94d124ffa6cc358592eb42b5da588fb4e07d09254aa40086db",
        strip_prefix = "abseil-cpp-20250512.1",
    )

    # Load a version of googletest that we know works. This is needed to create
    # //:.*mocks targets, which are public.
    maybe(
        http_archive,
        name = "googletest",
        urls = [
            "https://github.com/google/googletest/archive/v1.16.0.tar.gz",
        ],
        sha256 = "78c676fc63881529bf97bf9d45948d905a66833fbfa5318ea2cd7478cb98f399",
        strip_prefix = "googletest-1.16.0",
    )

    # Load the googleapis dependency.
    maybe(
        http_archive,
        name = "googleapis",
        urls = [
            "https://github.com/googleapis/googleapis/archive/05f65958eb7f2a8bc59db87ad40487f0fb093097.tar.gz",
        ],
        sha256 = "a7c3d7cdbd54e43ddc7c44f3571a71e004cba519c205da2e8b937480d608ae86",
        strip_prefix = "googleapis-05f65958eb7f2a8bc59db87ad40487f0fb093097",
        build_file = Label("//bazel:googleapis.BUILD"),
        # Scaffolding for patching googleapis after download. For example:
        patches = [

            # NOTE: This should only be used while developing with a new
            # protobuf message. No changes to `patches` should ever be
            # committed to the main branch.
            #"googleapis.patch",

            # Mirrors the patch from the current bazel module
            "//bazel:remove_upb_c_rules.patch",
        ],
        patch_tool = "patch",

        # Use the following args when developing with a new proto message
        # patch_args = ["-p1", "-l", "-n"],
        repo_mapping = {
            "@com_github_grpc_grpc": "@grpc",
        },
    )

    # Load protobuf.
    # The name "com_google_protobuf" is internally used by @bazel_tools,
    # a native repository we cannot override.
    # We will revert this to @protobuf once @bazel_tools is deprecated
    # and libraries have strayed away from it.
    # See https://github.com/googleapis/google-cloud-cpp/issues/15393
    maybe(
        http_archive,
        name = "com_google_protobuf",
        urls = [
            "https://github.com/protocolbuffers/protobuf/archive/v31.1.tar.gz",
        ],
        sha256 = "c3a0a9ece8932e31c3b736e2db18b1c42e7070cd9b881388b26d01aa71e24ca2",
        strip_prefix = "protobuf-31.1",
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

    # This is a transitive dependency of grpc
    maybe(
        http_archive,
        name = "io_bazel_rules_go",
        sha256 = "d93ef02f1e72c82d8bb3d5169519b36167b33cf68c252525e3b9d3d5dd143de7",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.49.0/rules_go-v0.49.0.zip",
            "https://github.com/bazelbuild/rules_go/releases/download/v0.49.0/rules_go-v0.49.0.zip",
        ],
        patch_args = ["-p1"],
    )

    # Load gRPC and its dependencies, using a similar pattern to this function.
    maybe(
        http_archive,
        name = "grpc",
        urls = [
            "https://github.com/grpc/grpc/archive/v1.74.1.tar.gz",
        ],
        repo_mapping = {
            "@com_google_absl": "@abseil-cpp",
            "@com_github_grpc_grpc": "@grpc",
        },
        sha256 = "7bf97c11cf3808d650a3a025bbf9c5f922c844a590826285067765dfd055d228",
        strip_prefix = "grpc-1.74.1",
    )

    native.bind(
        name = "protocol_compiler",
        actual = "@com_google_protobuf//:protoc",
    )

    # We use the cc_proto_library() rule from @com_google_protobuf, which
    # assumes that grpc_cpp_plugin and grpc_lib are in the //external: module
    native.bind(
        name = "grpc_cpp_plugin",
        actual = "@grpc//src/compiler:grpc_cpp_plugin",
    )

    native.bind(
        name = "grpc_lib",
        actual = "@grpc//:grpc++",
    )

    # We need libcurl for the Google Cloud Storage client.
    http_archive(
        name = "com_github_curl_curl",
        urls = [
            "https://curl.haxx.se/download/curl-7.74.0.tar.gz",
        ],
        sha256 = "e56b3921eeb7a2951959c02db0912b5fcd5fdba5aca071da819e1accf338bbd7",
        strip_prefix = "curl-7.74.0",
        build_file = Label("//bazel:curl.BUILD"),
    )

    # We need the nlohmann_json library
    maybe(
        http_archive,
        name = "nlohmann_json",
        urls = [
            "https://github.com/nlohmann/json/archive/v3.11.3.tar.gz",
        ],
        sha256 = "0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406",
        strip_prefix = "json-3.11.3",
    )

    # Open Telemetry
    http_archive(
        name = "opentelemetry-cpp",
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

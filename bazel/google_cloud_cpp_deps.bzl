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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def google_cloud_cpp_development_deps(name = None):
    """Loads dependencies needed to develop the google-cloud-cpp libraries.

    google-cloud-cpp developers call this function from the top-level WORKSPACE
    file to obtain all the necessary *development* dependencies for
    google-cloud-cpp. This includes testing dependencies and dependencies used
    by development tools.

    It is a bug if the targets used for google-cloud-cpp can be used outside
    the package. All such targets should have their visibility restricted, or
    are deprecated. If you still need to use such targets, this function may
    be useful in your own WORKSPACE file.

    This function only loads dependencies that have not been previously loaded,
    allowing developers to override the version of the dependencies they want to
    use.

    Args:
        name: Unused. It is conventional to provide a `name` argument to all
            workspace functions.
    """

    # This is only needed to run the microbenchmarks.
    maybe(
        http_archive,
        name = "com_google_benchmark",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_benchmark/v1.8.2.tar.gz",
            "https://github.com/google/benchmark/archive/v1.8.2.tar.gz",
        ],
        sha256 = "2aab2980d0376137f969d92848fbb68216abb07633034534fc8c65cc4e7a0e93",
        strip_prefix = "benchmark-1.8.2",
    )

    # PugiXML, this is only used in the docfx internal tool.
    maybe(
        http_archive,
        name = "com_github_zeux_pugixml",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_github_zeux_pugixml/v1.13.tar.gz",
            "https://github.com/zeux/pugixml/archive/v1.13.tar.gz",
        ],
        sha256 = "5c5ad5d7caeb791420408042a7d88c2c6180781bf218feca259fd9d840a888e1",
        strip_prefix = "pugixml-1.13",
        build_file = Label("//bazel:pugixml.BUILD"),
    )

    maybe(
        http_archive,
        name = "com_github_jbeder_yaml_cpp",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_github_jbeder_yaml_cpp/0.8.0.tar.gz",
            "https://github.com/jbeder/yaml-cpp/archive/0.8.0.tar.gz",
        ],
        sha256 = "fbe74bbdcee21d656715688706da3c8becfd946d92cd44705cc6098bb23b3a16",
        strip_prefix = "yaml-cpp-0.8.0",
    )

def google_cloud_cpp_deps(name = None):
    """Loads dependencies need to compile the google-cloud-cpp libraries.

    Application developers can call this function from their WORKSPACE file
    to obtain all the necessary dependencies for google-cloud-cpp, including
    gRPC and its dependencies. This function only loads dependencies that
    have not been previously loaded, allowing application developers to
    override the version of the dependencies they want to use.

    Args:
        name: Unused. It is conventional to provide a `name` argument to all
            workspace functions.
    """

    # Load platforms, we use it directly
    maybe(
        http_archive,
        name = "platforms",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/platforms/platforms-0.0.7.tar.gz",
            "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.7/platforms-0.0.7.tar.gz",
            "https://github.com/bazelbuild/platforms/releases/download/0.0.7/platforms-0.0.7.tar.gz",
        ],
        sha256 = "3a561c99e7bdbe9173aa653fd579fe849f1d8d67395780ab4770b1f381431d51",
    )

    # Load rules_cc, used by googletest
    maybe(
        http_archive,
        name = "rules_cc",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/rules_cc/rules_cc-0.0.1.tar.gz",
            "https://github.com/bazelbuild/rules_cc/releases/download/0.0.1/rules_cc-0.0.1.tar.gz",
        ],
        sha256 = "4dccbfd22c0def164c8f47458bd50e0c7148f3d92002cdb459c2a96a68498241",
    )

    # Load Abseil
    maybe(
        http_archive,
        name = "com_google_absl",
        urls = [
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_absl/20230802.0.tar.gz",
            "https://github.com/abseil/abseil-cpp/archive/20230802.0.tar.gz",
        ],
        sha256 = "59d2976af9d6ecf001a81a35749a6e551a335b949d34918cfade07737b9d93c5",
        strip_prefix = "abseil-cpp-20230802.0",
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
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_googleapis/0b58a18a17a5091682c5943bbf8fd46775002227.tar.gz",
            "https://github.com/googleapis/googleapis/archive/0b58a18a17a5091682c5943bbf8fd46775002227.tar.gz",
        ],
        sha256 = "cdbc8dcc84578420d5526482841676de609e6c773c24089c66dc4a4287b0874c",
        strip_prefix = "googleapis-0b58a18a17a5091682c5943bbf8fd46775002227",
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
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_google_protobuf/v24.0.tar.gz",
            "https://github.com/protocolbuffers/protobuf/archive/v24.0.tar.gz",
        ],
        sha256 = "850357336189c470e429e9bdffca92229d8cd5b7f84aa2f3b4c5fdb80ce8351b",
        strip_prefix = "protobuf-24.0",
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
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_github_grpc_grpc/v1.57.0.tar.gz",
            "https://github.com/grpc/grpc/archive/v1.57.0.tar.gz",
        ],
        sha256 = "8393767af531b2d0549a4c26cf8ba1f665b16c16fb6c9238a7755e45444881dd",
        strip_prefix = "grpc-1.57.0",
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
            "https://storage.googleapis.com/cloud-cpp-community-archive/com_github_nlohmann_json/v3.11.2.tar.gz",
            "https://github.com/nlohmann/json/archive/v3.11.2.tar.gz",
        ],
        sha256 = "d69f9deb6a75e2580465c6c4c5111b89c4dc2fa94e3a85fcd2ffcd9a143d9273",
        strip_prefix = "json-3.11.2",
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
            "https://storage.googleapis.com/cloud-cpp-community-archive/io_opentelemetry_cpp/v1.9.1.tar.gz",
            "https://github.com/open-telemetry/opentelemetry-cpp/archive/v1.9.1.tar.gz",
        ],
        sha256 = "668de24f81c8d36d75092ad9dcb02a97cd41473adbe72485ece05e336db48249",
        strip_prefix = "opentelemetry-cpp-1.9.1",
    )

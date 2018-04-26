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
        native.new_http_archive(
            name = "com_google_googletest",
            build_file = "@com_github_googlecloudplatform_google_cloud_cpp//bazel:googletest.BUILD",
            strip_prefix = "googletest-4bd8c4638ada823a8da2569735cc0a9402fb8052",
            url = "https://github.com/google/googletest/archive/4bd8c4638ada823a8da2569735cc0a9402fb8052.tar.gz",
            sha256 = "9f842f79d92dfa694d9811918efb36a0bcd82d7b32f37a8f852dddde264c0f55",
        )

    # Load the googleapis dependency.
    if "com_github_googleapis_googleapis" not in native.existing_rules():
        native.new_http_archive(
            name = "com_github_googleapis_googleapis",
            url = "https://github.com/google/googleapis/archive/f81082ea1e2f85c43649bee26e0d9871d4b41cdb.zip",
            strip_prefix="googleapis-f81082ea1e2f85c43649bee26e0d9871d4b41cdb",
            sha256 = "824870d87a176f26bcef663e92051f532fac756d1a06b404055dc078425f4378",
            build_file = "@com_github_googlecloudplatform_google_cloud_cpp//bazel:googleapis.BUILD",
            workspace_file = "@com_github_googlecloudplatform_google_cloud_cpp//bazel:googleapis.WORKSPACE",
        )

    # Load gRPC and its dependencies, using a similar pattern to this function.
    # This implictly loads "com_google_protobuf", which we use.
    if "com_github_grpc_grpc" not in native.existing_rules():
        native.http_archive(
            name = "com_github_grpc_grpc",
            strip_prefix = "grpc-1.10.0",
                    urls = [
                        "https://mirror.bazel.build/github.com/grpc/grpc/archive/v1.10.0.tar.gz",
                        "https://github.com/grpc/grpc/archive/v1.10.0.tar.gz",
                    ],
                    sha256 = "39a73de6fa2a03bdb9c43c89a4283e09880833b3c1976ef3ce3edf45c8cacf72"
        )

    # We use the cc_proto_library() rule from @com_google_protobuf, which
    # assumes that grpc_cpp_plugin and grpc_lib are in the //external: module
    native.bind(
        name = "grpc_cpp_plugin",
        actual = "@com_github_grpc_grpc//:grpc_cpp_plugin"
    )

    native.bind(
        name = "grpc_lib",
        actual = "@com_github_grpc_grpc//:grpc++"
    )

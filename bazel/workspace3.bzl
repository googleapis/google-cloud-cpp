# Copyright 2023 Google LLC
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

"""Load dependencies needed for google-cloud-cpp development / Phase 3."""

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")
load("@com_google_googletest//:googletest_deps.bzl", "googletest_deps")
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
load("@io_opentelemetry_cpp//bazel:repository.bzl", "opentelemetry_cpp_deps")

def gl_cpp_workspace3(name = None):
    """Loads dependencies needed to use the google-cloud-cpp libraries.

    Call this function after `//bazel/workspace2:workspace2()`.

    Args:
        name: Unused. It is conventional to provide a `name` argument to all
            workspace functions.
    """

    # The gRPC extra dependencies need grpc_deps() to have been called first.
    grpc_extra_deps()

    # Protobuf dependencies must be loaded after the gRPC dependencies.
    protobuf_deps()
    opentelemetry_cpp_deps()

    # Googletest dependencies
    googletest_deps()

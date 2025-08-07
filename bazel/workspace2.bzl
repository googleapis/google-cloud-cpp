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

"""Load dependencies needed for google-cloud-cpp development / Phase 2."""

load("@build_bazel_apple_support//lib:repositories.bzl", "apple_support_dependencies")
load("@grpc//bazel:grpc_deps.bzl", "grpc_deps")
load(
    "@googleapis//:repository_rules.bzl",
    "switched_rules_by_language",
)
load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies")

def gl_cpp_workspace2(name = None):
    """Loads dependencies needed to use the google-cloud-cpp libraries.

    Call this function after `//bazel/workspace1:workspace1()`.

    Args:
        name: Unused. It is conventional to provide a `name` argument to all
            workspace functions.
    """

    # `google-cloud-cpp` does not use these, but we need to override the
    # rules_apple initialization in gRPC.
    apple_support_dependencies()

    rules_cc_dependencies()

    # Configure @googleapis to only compile C++ and gRPC libraries.
    switched_rules_by_language(
        name = "googleapis_imports",
        cc = True,
        grpc = True,
    )

    grpc_deps()

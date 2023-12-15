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

"""Load dependencies needed for google-cloud-cpp development / Phase 1."""

load(
    "@rules_cc//cc:repositories.bzl",
    "rules_cc_dependencies",
)
load(
    "@com_google_googleapis//:repository_rules.bzl",
    "switched_rules_by_language",
)
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

def gl_cpp_workspace1(name = None):
    """Loads dependencies needed to use the google-cloud-cpp libraries.

    Call this function after `//bazel/workspace0:workspace0()`.

    Args:
        name: Unused. It is conventional to provide a `name` argument to all
            workspace functions.
    """

    rules_cc_dependencies()

    # Configure @com_google_googleapis to only compile C++ and gRPC libraries.
    switched_rules_by_language(
        name = "com_google_googleapis_imports",
        cc = True,
        grpc = True,
    )

    grpc_deps()

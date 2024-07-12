# Copyright 2024 Google LLC
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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def gl_cpp_development2(name = None):
    """Loads dependencies needed to develop the google-cloud-cpp libraries.

    This function assumes the repositories needed by our dependencies are loaded


    Args:
        name: Unused. It is conventional to provide a `name` argument to all
            workspace functions.
    """

    # An XML parser and generator, this is only used in //docfx.
    # This is an internal tool used to generate the reference documentation.
    maybe(
        http_archive,
        name = "com_github_zeux_pugixml",
        urls = [
            "https://github.com/zeux/pugixml/archive/v1.14.tar.gz",
        ],
        sha256 = "610f98375424b5614754a6f34a491adbddaaec074e9044577d965160ec103d2e",
        strip_prefix = "pugixml-1.14",
        build_file = Label("//bazel:pugixml.BUILD"),
    )

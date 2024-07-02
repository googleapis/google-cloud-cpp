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

"""Load dependencies needed to use the google-cloud-cpp libraries."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def gl_cpp_bzlmod0(name = None):
    """Loads dependencies need to compile the google-cloud-cpp libraries.

    This is a workaround until all dependencies of `google-cloud-cpp` can
    be managed via bzlmod.

    Args:
        name: Unused. It is conventional to provide a `name` argument to all
            workspace functions.
    """

    # TODO(#11485) - use some bazel_dep() from BCR.
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

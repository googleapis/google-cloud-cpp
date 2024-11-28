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

"""Load dependencies needed for google-cloud-cpp development / Phase 0."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def gl_cpp_development0(name = None):
    """Loads dependencies needed to develop the google-cloud-cpp libraries.

    google-cloud-cpp developers call this function from the top-level WORKSPACE
    file to obtain all the necessary *development* dependencies for
    google-cloud-cpp. This includes testing dependencies and dependencies used
    by development tools.

    It is a bug if the targets used for google-cloud-cpp development can be used
    outside the package. All such targets should have their visibility
    restricted, or are deprecated. If you still need to use such targets, this
    function may be useful in your own WORKSPACE file.

    This function only loads dependencies that have not been previously loaded,
    allowing developers to override the version of the dependencies they want to
    use.

    Args:
        name: Unused. It is conventional to provide a `name` argument to all
            workspace functions.
    """

    # This is needed by com_google_benchmark. We cache it because
    # sourceforge.net can have outages and that breaks the build.
    maybe(
        http_archive,
        name = "libpfm",
        urls = [
            "https://sourceforge.net/projects/perfmon2/files/libpfm4/libpfm-4.11.0.tar.gz",
        ],
        sha256 = "5da5f8872bde14b3634c9688d980f68bda28b510268723cc12973eedbab9fecc",
        strip_prefix = "libpfm-4.11.0",
        build_file = Label("//bazel:libpfm.BUILD"),
    )

    # This is only needed to run the microbenchmarks.
    maybe(
        http_archive,
        name = "com_google_benchmark",
        urls = [
            "https://github.com/google/benchmark/archive/v1.9.1.tar.gz",
        ],
        sha256 = "32131c08ee31eeff2c8968d7e874f3cb648034377dfc32a4c377fa8796d84981",
        strip_prefix = "benchmark-1.9.1",
    )

    # A YAML parser and generator, this is only used in //docfx and //generator.
    # Both are internal tools used for development only.
    maybe(
        http_archive,
        name = "com_github_jbeder_yaml_cpp",
        urls = [
            "https://github.com/jbeder/yaml-cpp/archive/0.8.0.tar.gz",
        ],
        sha256 = "fbe74bbdcee21d656715688706da3c8becfd946d92cd44705cc6098bb23b3a16",
        strip_prefix = "yaml-cpp-0.8.0",
    )

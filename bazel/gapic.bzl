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

"""A definition for the typical C++ GAPIC library."""

def cc_gapic_library(name, service_dirs = [], googleapis_deps = []):
    """Defines targets for the typical fully generated GAPIC library

    Args:
        name: The name of the library, e.g. `accessapproval`

        service_dirs: The list of service directories. e.g. `["v1/"]`

        googlapis_deps: The googleapis-defined cc_grpc_library definitions that
            this library depends on.
    """

    code_glob = [d + i + f for d in service_dirs for i in [
        "",
        "internal/",
    ] for f in [
        "*.h",
        "*.cc",
    ]]

    sources_glob = [d + "internal/*_sources.cc" for d in service_dirs]

    native.filegroup(
        name = "srcs",
        srcs = native.glob(sources_glob),
    )

    native.filegroup(
        name = "hdrs",
        srcs = native.glob(include = code_glob, exclude = sources_glob),
    )

    native.filegroup(
        name = "public_hdrs",
        srcs = native.glob([d + "*.h" for d in service_dirs]),
        visibility = ["//:__pkg__"],
    )

    native.filegroup(
        name = "mocks",
        srcs = native.glob([d + "mocks/*.h" for d in service_dirs]),
        visibility = ["//:__pkg__"],
    )

    native.cc_library(
        name = "google_cloud_cpp_" + name,
        srcs = [":srcs"],
        hdrs = [":hdrs"],
        visibility = ["//:__pkg__"],
        deps = ["//:common", "//:grpc_utils"] + googleapis_deps,
    )

    native.cc_library(
        name = "google_cloud_cpp_" + name + "_mocks",
        hdrs = [":mocks"],
        visibility = ["//:__pkg__"],
        deps = [
            ":google_cloud_cpp_" + name,
            "@com_google_googletest//:gtest",
        ],
    )

    [native.cc_test(
        name = sample.replace("/", "_").replace(".cc", ""),
        srcs = [sample],
        tags = ["integration-test"],
        deps = [
            "//:" + name,
            "//google/cloud/testing_util:google_cloud_cpp_testing_private",
        ],
    ) for sample in native.glob([d + "samples/*_samples.cc" for d in service_dirs])]

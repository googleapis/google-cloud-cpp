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

def compute_library(service_dir, inner_deps = []):
    """Defines targets for a compute resource library

    Args:
        service_dir: The service directory. e.g. "disks/v1/"

        inner_deps: Other libraries that this library depends on. In practice,
            these are the compute_*_operations libraries.
    """

    d = service_dir
    service = d.replace("/v1/", "")

    native.filegroup(
        name = service + "_srcs",
        srcs = native.glob([d + "*.cc", d + "internal/*.cc"]),
    )

    native.filegroup(
        name = service + "_hdrs",
        srcs = native.glob([d + "*.h", d + "internal/*.h"]),
    )

    native.filegroup(
        name = service + "_public_hdrs",
        srcs = native.glob([d + "*.h"]),
        visibility = ["//:__pkg__"],
    )

    native.filegroup(
        name = service + "_mock_hdrs",
        srcs = native.glob([d + "mocks/*.h"]),
        visibility = ["//:__pkg__"],
    )

    native.cc_library(
        name = "google_cloud_cpp_compute_" + service + "_mocks",
        hdrs = [":" + service + "_mock_hdrs"],
        visibility = ["//:__pkg__"],
        deps = [
            ":google_cloud_cpp_compute_" + service,
            "@com_google_googletest//:gtest",
        ],
    )

    native.cc_library(
        name = "google_cloud_cpp_compute_" + service,
        srcs = [":" + service + "_srcs"],
        hdrs = [":" + service + "_hdrs"],
        visibility = ["//:__pkg__"],
        deps = [
            "//:common",
            "//:grpc_utils",
            "//google/cloud:google_cloud_cpp_rest_internal",
            "//google/cloud:google_cloud_cpp_rest_protobuf_internal",
            "//protos:system_includes",
            "//protos/google/cloud/compute:cc_proto",
        ] + inner_deps,
    )

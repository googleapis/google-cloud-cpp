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

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

load("@com_google_protobuf//:protobuf.bzl", "cc_proto_library")

cc_proto_library(
    name = "taq_protos",
    srcs = [
        "taq.proto",
    ],
    include = ".",
    default_runtime = "@com_google_protobuf//:protobuf",
    protoc = "@com_google_protobuf//:protoc",
    use_grpc_plugin = False,
    deps = ["@com_google_protobuf//:cc_wkt_protos"],
)

cc_library(
    name = "circular_buffer",
    hdrs = ["circular_buffer.h"],
)

timeseries_demos = [
    "compute_spread",
    "compute_vwap",
    "estimate_split_points",
    "upload_taq_nbbo",
    "upload_taq_trades",
    "shuffle_taq_nbbo",
    "shuffle_taq_trades",
]

[cc_binary(
    name = demo,
    srcs = [demo + ".cc"],
    deps = [
        ":circular_buffer",
        ":taq_protos",
        "//google/cloud/bigtable:bigtable_client",
    ],
) for demo in timeseries_demos]

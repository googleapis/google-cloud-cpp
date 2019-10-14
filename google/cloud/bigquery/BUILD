# Copyright 2019 Google LLC
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

cc_library(
    name = "bigquery_client",
    hdrs = [
      "client.h",
      "connection.h",
      "connection_options.h",
      "version.h",
      "version_info.h",
    ],
    srcs = [
      "client.cc",
      "connection_options.cc",
      "internal/stream_reader.h",
      "internal/bigquerystorage_stub.cc",
      "internal/bigquerystorage_stub.h",
      "internal/connection_impl.cc",
      "internal/connection_impl.h",
      "version.cc",
    ],
    deps = [
        "@com_github_googleapis_google_cloud_cpp//google/cloud:google_cloud_cpp_common",
        "@com_github_googleapis_google_cloud_cpp//google/cloud/grpc_utils:google_cloud_cpp_grpc_utils",
	"@com_google_googleapis//:bigquery_protos",
    ],

)

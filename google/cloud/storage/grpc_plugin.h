// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_GRPC_PLUGIN_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_GRPC_PLUGIN_H

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"

namespace google {
namespace cloud {
/**
 * Contains experimental features for the GCS C++ Client Library.
 *
 * The types, functions, aliases, and objects in this namespace are subject to
 * change without notice, including removal.
 */
namespace storage_experimental {
inline namespace STORAGE_CLIENT_NS {

/**
 * Low-level experimental settings for the GCS+gRPC plugin.
 *
 * Possible values for the string include:
 *
 * - "default" or "none": do not use any special settings with gRPC.
 * - "dp": enable Google Direct Access (formerly 'Direct Path') equivalent to
 *   setting both "pick-first-lb" and "enable-dns-srv-queries".
 * - "alts": same settings as "dp", but use the experimental ALTS credentials.
 * - "enable-dns-srv-queries": set the `grpc.dns_enable_srv_queries` channel
 *   argument to `1`, see [dns-query-arg].
 * - "disable-dns-srv-queries": set the `grpc.dns_enable_srv_queries` channel
 *   argument to `0`, see [dns-query-arg].
 * - "pick-first-lb": configure the gRPC load balancer to use the "pick_first"
 *   policy.
 * - "exclusive": use an exclusive channel for each stub.
 *
 * Unknown values are ignored.
 *
 * [dns-query-arg]:
 * https://grpc.github.io/grpc/core/group__grpc__arg__keys.html#ga247ed6771077938be12ab24790a95732
 */
struct GrpcPluginOption {
  using Type = std::string;
};

/**
 * Create a `google::cloud::storage::Client` object configured to use gRPC.
 *
 * @note the Credentials parameter in the configuration is ignored. The gRPC
 *     client only supports Google Default Credentials.
 *
 * @param opts the configuration parameters for the Client.
 *
 * @warning this is an experimental feature, and subject to change without
 *     notice.
 */
google::cloud::storage::Client DefaultGrpcClient(Options opts = {});

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_GRPC_PLUGIN_H

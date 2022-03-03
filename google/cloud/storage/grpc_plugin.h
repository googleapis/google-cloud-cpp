// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Configure the GCS+gRPC plugin.
 *
 * - "none": use REST, disables gRPC.
 * - "media": use gRPC for media (aka data, aka I/O) operations, and REST for
 *   all other requests. In other words, only `ReadObject()`, `WriteObject()`,
 *   and `InsertObject()` use gRPC.
 * - "metadata": use gRPC for all operations.
 *
 * @warning GCS+gRPC is an experimental feature of the C++ client library, and
 *   subject to change without notice.  The service itself is not generally
 *   available, does not have an SLA and requires projects to be allow-listed.
 */
struct GrpcPluginOption {
  using Type = std::string;
};

/**
 * Create a `google::cloud::storage::Client` object configured to use gRPC.
 *
 * @note The Credentials parameter in the configuration is ignored. The gRPC
 *     client only supports Google Default Credentials.
 *
 * @param opts the configuration parameters for the Client.
 *
 * @warning This is an experimental feature, and subject to change without
 *     notice.
 *
 * @par Example storage_grpc_samples.cc grpc-read-write
 */
google::cloud::storage::Client DefaultGrpcClient(Options opts = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_GRPC_PLUGIN_H

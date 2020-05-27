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
 * Create a `google::cloud::storage::Client` object configured to use gRPC.
 *
 * @warning this is an experimental feature, and subject to change without
 *     notice.
 *
 * @par Example
 * @snippet storage_grpc_samples.cc grpc-default-client
 */
StatusOr<google::cloud::storage::Client> DefaultGrpcClient();

/**
 * Create a `google::cloud::storage::Client` object configured to use gRPC.
 *
 * @note the Credentials parameter in the configuration is ignored. The gRPC
 *     client only supports Google Default Credentials.
 *
 * @param options the configuration parameters for the Client.
 *
 * @warning this is an experimental feature, and subject to change without
 *     notice.
 */
google::cloud::storage::Client DefaultGrpcClient(
    google::cloud::storage::ClientOptions options);

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_GRPC_PLUGIN_H

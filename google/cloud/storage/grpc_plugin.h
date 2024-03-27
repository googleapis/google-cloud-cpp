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
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Create a `google::cloud::storage::Client` object configured to use gRPC.
 *
 * @param opts the configuration parameters for the Client.
 *
 * @warning At present, GCS gRPC is GA with Allowlist. To access this API,
 *   kindly contact the Google Cloud Storage gRPC team at
 *   gcs-grpc-contact@google.com with a list of GCS buckets you would like to
 *   Allowlist. Please note that while the **service** is GA (with Allowlist),
 *   the client library features remain experimental and subject to change
 *   without notice.
 *
 * @par Example
 * @snippet storage_grpc_samples.cc grpc-read-write
 */
google::cloud::storage::Client MakeGrpcClient(Options opts = {});

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage

// TODO(#13857) - remove the backwards compatibility shims.
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * Configure the GCS+gRPC plugin.
 *
 * @deprecated use `google::cloud::storage::Client()` to create JSON-based
 *     clients and `google::cloud::storage::MakeGrpcClient()` to create
 *     gRPC-based clients. If you need to pick one dynamically a simple
 *     `if()` statement or ternary expression can do the job.
 */
struct [[deprecated(
    "use storage::Client() or storage::MakeGrpcClient()")]] GrpcPluginOption {
  using Type = std::string;
};

/**
 * Create a `google::cloud::storage::Client` object configured to use gRPC.
 *
 * @deprecated Please use `google::cloud::storage::MakeGrpcClient`.
 */
[[deprecated(
    "use ::google::cloud::storage::MakeGrpcClient() instead")]] inline google::
    cloud::storage::Client
    DefaultGrpcClient(Options opts = {}) {
  return ::google::cloud::storage::MakeGrpcClient(std::move(opts));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_GRPC_PLUGIN_H

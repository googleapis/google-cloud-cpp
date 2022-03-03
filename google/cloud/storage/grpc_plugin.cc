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

#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/internal/hybrid_client.h"

namespace google {
namespace cloud {
namespace storage_experimental {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

google::cloud::storage::Client DefaultGrpcClient(Options opts) {
  opts = google::cloud::storage::internal::DefaultOptionsGrpc(std::move(opts));
  auto config = opts.get<GrpcPluginOption>();
  if (config == "none" || config.empty()) {
    return google::cloud::storage::Client(std::move(opts));
  }
  if (config == "metadata") {
    return storage::internal::ClientImplDetails::CreateClient(
        storage::internal::GrpcClient::Create(opts));
  }
  return storage::internal::ClientImplDetails::CreateClient(
      storage::internal::HybridClient::Create(opts));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/internal/hybrid_client.h"
#include "google/cloud/internal/getenv.h"

namespace google {
namespace cloud {
namespace storage_experimental {
inline namespace STORAGE_CLIENT_NS {

namespace {
bool UseGrpcForMetadata() {
  auto v =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG")
          .value_or("");
  return v.find("metadata") != std::string::npos;
}
}  // namespace

StatusOr<google::cloud::storage::Client> DefaultGrpcClient() {
  auto options = storage::ClientOptions::CreateDefaultClientOptions();
  if (!options) return std::move(options).status();

  return DefaultGrpcClient(*std::move(options));
}

google::cloud::storage::Client DefaultGrpcClient(
    google::cloud::storage::ClientOptions options) {
  if (UseGrpcForMetadata()) {
    return storage::Client(
        std::make_shared<storage::internal::GrpcClient>(std::move(options)));
  }
  return storage::Client(
      std::make_shared<storage::internal::HybridClient>(std::move(options)));
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

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

StatusOr<google::cloud::storage::Client> DefaultGrpcClient(Options opts) {
  return DefaultGrpcClient(0, std::move(opts));
}

StatusOr<google::cloud::storage::Client> DefaultGrpcClient(int channel_id,
                                                           Options opts) {
  opts = google::cloud::storage::internal::DefaultOptionsGrpc(std::move(opts));
  if (UseGrpcForMetadata()) {
    return storage::internal::ClientImplDetails::CreateClient(
        storage::internal::GrpcClient::Create(opts, channel_id));
  }
  // The hybrid client might need the OAuth2 credentials
  if (!opts.has<storage::internal::Oauth2CredentialsOption>()) {
    storage::ChannelOptions channel_options;
    channel_options.set_ssl_root_path(
        opts.get<storage::internal::SslRootPathOption>());
    auto credentials =
        storage::oauth2::GoogleDefaultCredentials(channel_options);
    if (!credentials) return std::move(credentials).status();
    opts.set<storage::internal::Oauth2CredentialsOption>(
        *std::move(credentials));
  }
  return storage::internal::ClientImplDetails::CreateClient(
      storage::internal::HybridClient::Create(opts, channel_id));
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage_experimental
}  // namespace cloud
}  // namespace google

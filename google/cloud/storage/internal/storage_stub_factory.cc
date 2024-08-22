// Copyright 2022 Google LLC
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

#include "google/cloud/storage/internal/storage_stub_factory.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/internal/storage_auth_decorator.h"
#include "google/cloud/storage/internal/storage_logging_decorator.h"
#include "google/cloud/storage/internal/storage_metadata_decorator.h"
#include "google/cloud/storage/internal/storage_round_robin_decorator.h"
#include "google/cloud/storage/internal/storage_tracing_stub.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/log.h"
#include <grpcpp/grpcpp.h>
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::shared_ptr<grpc::Channel> CreateGrpcChannel(
    google::cloud::internal::GrpcAuthenticationStrategy& auth,
    Options const& options, int channel_id) {
  auto args = internal::MakeChannelArguments(options);
  // Use a local subchannel pool to avoid contention in gRPC.
  args.SetInt(GRPC_ARG_USE_LOCAL_SUBCHANNEL_POOL, 1);
  // Use separate sockets for each channel. This is redundant since we also set
  // `GRPC_ARG_USE_LOCAL_SUBCHANNEL_POOL`, but it is harmless.
  args.SetInt(GRPC_ARG_CHANNEL_ID, channel_id);

  // Disable queries. GCS+gRPC does not use a load-balancer (such as `grpclb`)
  // that requires server queries. Disabling the server queries is, therefore,
  // harmless. Furthermore, it avoids triggering any latent bugs in the code
  // to send and/or receive those queries.
  args.SetInt(GRPC_ARG_DNS_ENABLE_SRV_QUERIES, 0);

  // This is needed to filter GCS+gRPC metrics based on the authority field.
  if (options.has<AuthorityOption>()) {
    args.SetString(GRPC_ARG_DEFAULT_AUTHORITY, options.get<AuthorityOption>());
  }

  return auth.CreateChannel(options.get<EndpointOption>(), std::move(args));
}

}  // namespace

std::pair<std::vector<std::shared_ptr<grpc::Channel>>,
          std::shared_ptr<StorageStub>>
CreateDecoratedStubs(google::cloud::CompletionQueue cq, Options const& options,
                     BaseStorageStubFactory const& base_factory) {
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      std::move(cq), options);

  std::vector<std::shared_ptr<grpc::Channel>> channels(
      (std::max)(1, options.get<GrpcNumChannelsOption>()));
  int id = 0;
  std::generate(channels.begin(), channels.end(),
                [&] { return CreateGrpcChannel(*auth, options, id++); });
  std::vector<std::shared_ptr<StorageStub>> children(channels.size());
  std::transform(channels.begin(), channels.end(), children.begin(),
                 base_factory);

  std::shared_ptr<StorageStub> stub =
      std::make_shared<StorageRoundRobin>(std::move(children));
  if (auth->RequiresConfigureContext()) {
    stub = std::make_shared<StorageAuth>(std::move(auth), std::move(stub));
  }
  stub = std::make_shared<StorageMetadata>(
      std::move(stub), std::multimap<std::string, std::string>{},
      internal::HandCraftedLibClientHeader());
  if (google::cloud::internal::Contains(options.get<LoggingComponentsOption>(),
                                        "rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<StorageLogging>(
        std::move(stub), options.get<GrpcTracingOptionsOption>(),
        options.get<LoggingComponentsOption>());
  }
  if (internal::TracingEnabled(options)) {
    stub = MakeStorageTracingStub(std::move(stub));
  }
  return std::make_pair(std::move(channels), std::move(stub));
}

std::pair<std::shared_ptr<GrpcChannelRefresh>, std::shared_ptr<StorageStub>>
CreateStorageStub(google::cloud::CompletionQueue cq, Options const& options) {
  auto p =
      CreateDecoratedStubs(cq, options, [](std::shared_ptr<grpc::Channel> c) {
        return std::make_shared<DefaultStorageStub>(
            google::storage::v2::Storage::NewStub(std::move(c)));
      });
  auto refresh = std::make_shared<GrpcChannelRefresh>(std::move(p.first));
  refresh->StartRefreshLoop(std::move(cq));
  return std::make_pair(std::move(refresh), std::move(p.second));
}

std::shared_ptr<google::cloud::internal::MinimalIamCredentialsStub>
CreateStorageIamStub(google::cloud::CompletionQueue cq,
                     Options const& options) {
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      std::move(cq), options);
  return google::cloud::internal::MakeMinimalIamCredentialsStub(
      std::move(auth),
      google::cloud::internal::MakeMinimalIamCredentialsOptions(options));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

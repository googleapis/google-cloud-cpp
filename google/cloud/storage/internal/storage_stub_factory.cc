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
#include "google/cloud/storage/internal/storage_round_robin.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/unified_grpc_credentials.h"
#include "google/cloud/log.h"
#include <grpcpp/grpcpp.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::shared_ptr<grpc::Channel> CreateGrpcChannel(
    google::cloud::internal::GrpcAuthenticationStrategy& auth,
    Options const& options, int channel_id) {
  auto args = internal::MakeChannelArguments(options);
  // Use a local subchannel pool to avoid contention in gRPC
  args.SetInt(GRPC_ARG_USE_LOCAL_SUBCHANNEL_POOL, 1);
  // Use separate sockets for each channel, this is redundant since we also set
  // `GRPC_ARG_USE_LOCAL_SUBCHANNEL_POOL`.
  args.SetInt(GRPC_ARG_CHANNEL_ID, channel_id);
  // Disable queries see b/243676671, this is harmless as we do not use any
  // load balancer that requires server queries.
  args.SetInt(GRPC_ARG_DNS_ENABLE_SRV_QUERIES, 0);

  // Effectively disable keepalive messages.
  auto constexpr kDisableKeepaliveTime =
      std::chrono::milliseconds(std::chrono::hours(24));
  args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS,
              static_cast<int>(kDisableKeepaliveTime.count()));
  // Make gRPC set the TCP_USER_TIMEOUT socket option to a value that detects
  // broken servers more quickly.
  auto constexpr kKeepaliveTimeout =
      std::chrono::milliseconds(std::chrono::seconds(60));
  args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS,
              static_cast<int>(kKeepaliveTimeout.count()));
  return auth.CreateChannel(options.get<EndpointOption>(), std::move(args));
}

std::shared_ptr<StorageStub> CreateStorageStubRoundRobin(
    std::vector<std::shared_ptr<StorageStub>> children, CompletionQueue cq,
    std::vector<std::shared_ptr<grpc::Channel>> channels) {
  auto stub = std::make_shared<StorageRoundRobin>(std::move(children));
  stub->StartRefreshLoop(std::move(cq), std::move(channels));
  return stub;
}

}  // namespace

std::shared_ptr<StorageStub> CreateDecoratedStubs(
    google::cloud::CompletionQueue cq, Options const& options,
    BaseStorageStubFactory const& base_factory) {
  auto auth =
      google::cloud::internal::CreateAuthenticationStrategy(cq, options);

  std::vector<std::shared_ptr<grpc::Channel>> channels(
      (std::max)(1, options.get<GrpcNumChannelsOption>()));
  int id = 0;
  std::generate(channels.begin(), channels.end(),
                [&] { return CreateGrpcChannel(*auth, options, id++); });
  std::vector<std::shared_ptr<StorageStub>> children(channels.size());
  std::transform(channels.begin(), channels.end(), children.begin(),
                 base_factory);

  auto stub = CreateStorageStubRoundRobin(std::move(children), std::move(cq),
                                          std::move(channels));
  if (auth->RequiresConfigureContext()) {
    stub = std::make_shared<StorageAuth>(std::move(auth), std::move(stub));
  }
  stub = std::make_shared<StorageMetadata>(std::move(stub));
  if (google::cloud::internal::Contains(options.get<TracingComponentsOption>(),
                                        "rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<StorageLogging>(
        std::move(stub), options.get<GrpcTracingOptionsOption>(),
        options.get<TracingComponentsOption>());
  }
  return stub;
}

std::shared_ptr<StorageStub> CreateStorageStub(
    google::cloud::CompletionQueue cq, Options const& options) {
  return CreateDecoratedStubs(
      std::move(cq), options, [](std::shared_ptr<grpc::Channel> c) {
        return std::make_shared<DefaultStorageStub>(
            google::storage::v2::Storage::NewStub(std::move(c)));
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

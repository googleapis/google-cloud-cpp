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
  args.SetInt("grpc.channel_id", channel_id);
  return auth.CreateChannel(options.get<EndpointOption>(), std::move(args));
}

}  // namespace

std::shared_ptr<StorageStub> CreateStorageStubRoundRobin(
    Options const& options,
    std::function<std::shared_ptr<StorageStub>(int)> child_factory) {
  std::vector<std::shared_ptr<StorageStub>> children(
      (std::max)(1, options.get<GrpcNumChannelsOption>()));
  int id = 0;
  std::generate(children.begin(), children.end(),
                [&id, &child_factory] { return child_factory(id++); });
  return std::make_shared<StorageRoundRobin>(std::move(children));
}

std::shared_ptr<StorageStub> CreateDecoratedStubs(
    google::cloud::CompletionQueue cq, Options const& options,
    BaseStorageStubFactory const& base_factory) {
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      std::move(cq), options);
  auto child_factory = [base_factory, &auth, options](int id) {
    auto channel = CreateGrpcChannel(*auth, options, id);
    return base_factory(std::move(channel));
  };
  auto stub = CreateStorageStubRoundRobin(options, std::move(child_factory));
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

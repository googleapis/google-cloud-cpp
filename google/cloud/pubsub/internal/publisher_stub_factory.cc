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

#include "google/cloud/pubsub/internal/publisher_stub_factory.h"
#include "google/cloud/pubsub/internal/create_channel.h"
#include "google/cloud/pubsub/internal/publisher_auth_decorator.h"
#include "google/cloud/pubsub/internal/publisher_logging_decorator.h"
#include "google/cloud/pubsub/internal/publisher_metadata_decorator.h"
#include "google/cloud/pubsub/internal/publisher_round_robin_decorator.h"
#include "google/cloud/pubsub/internal/publisher_tracing_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::shared_ptr<PublisherStub> CreateRoundRobinPublisherStub(
    Options const& options,
    std::function<std::shared_ptr<PublisherStub>(int)> child_factory) {
  std::vector<std::shared_ptr<PublisherStub>> children(
      (std::max)(1, options.get<GrpcNumChannelsOption>()));
  int id = 0;
  std::generate(children.begin(), children.end(),
                [&id, &child_factory] { return child_factory(id++); });
  return std::make_shared<PublisherRoundRobin>(std::move(children));
}

std::shared_ptr<grpc::Channel> CreateGrpcChannel(
    google::cloud::internal::GrpcAuthenticationStrategy& auth,
    Options const& options, int channel_id) {
  return auth.CreateChannel(
      options.get<EndpointOption>(),
      pubsub_internal::MakeChannelArguments(options, channel_id));
}

}  // namespace

std::shared_ptr<PublisherStub> MakeRoundRobinPublisherStub(
    google::cloud::CompletionQueue cq, Options const& options) {
  return CreateDecoratedStubs(
      std::move(cq), options, [](std::shared_ptr<grpc::Channel> c) {
        return std::make_shared<DefaultPublisherStub>(
            google::pubsub::v1::Publisher::NewStub(std::move(c)));
      });
}

std::shared_ptr<PublisherStub> CreateDecoratedStubs(
    google::cloud::CompletionQueue cq, Options const& options,
    BasePublisherStubFactory const& base_factory) {
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      std::move(cq), options);
  auto child_factory = [base_factory, &auth, options](int id) {
    auto channel = CreateGrpcChannel(*auth, options, id);
    return base_factory(std::move(channel));
  };

  auto stub = CreateRoundRobinPublisherStub(options, std::move(child_factory));
  if (auth->RequiresConfigureContext()) {
    stub = std::make_shared<pubsub_internal::PublisherAuth>(std::move(auth),
                                                            std::move(stub));
  }
  stub = std::make_shared<pubsub_internal::PublisherMetadata>(
      std::move(stub), std::multimap<std::string, std::string>{},
      internal::HandCraftedLibClientHeader());
  if (internal::Contains(options.get<LoggingComponentsOption>(), "rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<pubsub_internal::PublisherLogging>(
        std::move(stub), options.get<GrpcTracingOptionsOption>(),
        options.get<LoggingComponentsOption>());
  }

  if (internal::TracingEnabled(options)) {
    stub = MakePublisherTracingStub(std::move(stub));
  }
  return stub;
}

std::shared_ptr<PublisherStub> MakeTestPublisherStub(
    google::cloud::CompletionQueue cq, Options const& options,
    std::vector<std::shared_ptr<PublisherStub>> mocks) {
  return CreateDecoratedStubs(
      std::move(cq), options,
      [mocks = std::move(mocks)](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<pubsub_internal::PublisherRoundRobin>(
            std::move(mocks));
      });
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

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
#include "google/cloud/pubsub/internal/publisher_round_robin.h"
#include "google/cloud/log.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::shared_ptr<pubsub_internal::PublisherStub> DecoratePublisherStub(
    Options const& opts,
    std::shared_ptr<internal::GrpcAuthenticationStrategy> auth,
    std::shared_ptr<PublisherStub> stub) {
  if (auth->RequiresConfigureContext()) {
    stub = std::make_shared<pubsub_internal::PublisherAuth>(std::move(auth),
                                                            std::move(stub));
  }
  stub = std::make_shared<pubsub_internal::PublisherMetadata>(std::move(stub));
  if (internal::Contains(opts.get<TracingComponentsOption>(), "rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<pubsub_internal::PublisherLogging>(
        std::move(stub), opts.get<GrpcTracingOptionsOption>());
  }
  return stub;
}

}  // namespace

std::shared_ptr<PublisherStub> MakeDefaultPublisherStub(
    google::cloud::CompletionQueue cq, Options const& options) {
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      std::move(cq), options);
  auto stub = pubsub_internal::CreateDefaultPublisherStub(
      auth->CreateChannel(options.get<EndpointOption>(),
                          pubsub_internal::MakeChannelArguments(options, 0)));
  return DecoratePublisherStub(options, std::move(auth), std::move(stub));
}

std::shared_ptr<PublisherStub> MakeRoundRobinPublisherStub(
    google::cloud::CompletionQueue cq, Options const& options) {
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      std::move(cq), options);
  std::vector<std::shared_ptr<pubsub_internal::PublisherStub>> children(
      options.get<GrpcNumChannelsOption>());
  int id = 0;
  std::generate(children.begin(), children.end(), [&id, &options, &auth] {
    return pubsub_internal::CreateDefaultPublisherStub(auth->CreateChannel(
        options.get<EndpointOption>(),
        pubsub_internal::MakeChannelArguments(options, id++)));
  });

  auto stub = std::make_shared<pubsub_internal::PublisherRoundRobin>(
      std::move(children));
  return DecoratePublisherStub(options, std::move(auth), std::move(stub));
}

std::shared_ptr<PublisherStub> MakeTestPublisherStub(
    google::cloud::CompletionQueue cq, Options const& options,
    std::vector<std::shared_ptr<PublisherStub>> mocks) {
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      std::move(cq), options);
  auto stub =
      std::make_shared<pubsub_internal::PublisherRoundRobin>(std::move(mocks));
  return DecoratePublisherStub(options, std::move(auth), std::move(stub));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

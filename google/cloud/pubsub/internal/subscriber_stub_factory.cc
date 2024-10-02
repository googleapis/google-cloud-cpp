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

#include "google/cloud/pubsub/internal/subscriber_stub_factory.h"
#include "google/cloud/pubsub/internal/create_channel.h"
#include "google/cloud/pubsub/internal/subscriber_auth_decorator.h"
#include "google/cloud/pubsub/internal/subscriber_logging_decorator.h"
#include "google/cloud/pubsub/internal/subscriber_metadata_decorator.h"
#include "google/cloud/pubsub/internal/subscriber_round_robin_decorator.h"
#include "google/cloud/pubsub/internal/subscriber_tracing_stub.h"
#include "google/cloud/common_options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/algorithm.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/log.h"
#include <google/iam/v1/iam_policy.grpc.pb.h>
#include <google/pubsub/v1/pubsub.grpc.pb.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

namespace {

std::shared_ptr<SubscriberStub> CreateRoundRobinSubscriberStub(
    Options const& options,
    std::function<std::shared_ptr<SubscriberStub>(int)> child_factory) {
  std::vector<std::shared_ptr<SubscriberStub>> children(
      (std::max)(1, options.get<GrpcNumChannelsOption>()));
  int id = 0;
  std::generate(children.begin(), children.end(),
                [&id, &child_factory] { return child_factory(id++); });
  return std::make_shared<SubscriberRoundRobin>(std::move(children));
}

std::shared_ptr<grpc::Channel> CreateGrpcChannel(
    google::cloud::internal::GrpcAuthenticationStrategy& auth,
    Options const& options, int channel_id) {
  return auth.CreateChannel(
      options.get<EndpointOption>(),
      pubsub_internal::MakeChannelArguments(options, channel_id));
}

}  // namespace

std::shared_ptr<SubscriberStub> CreateDefaultSubscriberStub(
    std::shared_ptr<grpc::Channel> channel) {
  return std::make_shared<DefaultSubscriberStub>(
      google::pubsub::v1::Subscriber::NewStub(channel),
      google::iam::v1::IAMPolicy::NewStub(channel));
}

std::shared_ptr<SubscriberStub> MakeRoundRobinSubscriberStub(
    google::cloud::CompletionQueue cq, Options const& options) {
  return CreateDecoratedStubs(std::move(cq), options,
                              [](std::shared_ptr<grpc::Channel> c) {
                                return std::make_shared<DefaultSubscriberStub>(
                                    google::pubsub::v1::Subscriber::NewStub(c),
                                    google::iam::v1::IAMPolicy::NewStub(c));
                              });
}

std::shared_ptr<SubscriberStub> MakeTestSubscriberStub(
    google::cloud::CompletionQueue cq, Options const& options,
    std::vector<std::shared_ptr<SubscriberStub>> mocks) {
  return CreateDecoratedStubs(
      std::move(cq), options,
      [mocks = std::move(mocks)](std::shared_ptr<grpc::Channel> const&) {
        return std::make_shared<pubsub_internal::SubscriberRoundRobin>(
            std::move(mocks));
      });
}

std::shared_ptr<SubscriberStub> CreateDecoratedStubs(
    google::cloud::CompletionQueue cq, Options const& options,
    BaseSubscriberStubFactory const& base_factory) {
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      std::move(cq), options);
  auto child_factory = [base_factory, &auth, options](int id) {
    auto channel = CreateGrpcChannel(*auth, options, id);
    return base_factory(std::move(channel));
  };

  auto stub = CreateRoundRobinSubscriberStub(options, std::move(child_factory));
  if (auth->RequiresConfigureContext()) {
    stub = std::make_shared<SubscriberAuth>(std::move(auth), std::move(stub));
  }
  stub = std::make_shared<SubscriberMetadata>(
      std::move(stub), std::multimap<std::string, std::string>{},
      internal::HandCraftedLibClientHeader());
  if (internal::Contains(options.get<LoggingComponentsOption>(), "rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<SubscriberLogging>(
        std::move(stub), options.get<GrpcTracingOptionsOption>(),
        options.get<LoggingComponentsOption>());
  }

  if (internal::TracingEnabled(options)) {
    stub = MakeSubscriberTracingStub(std::move(stub));
  }
  return stub;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/internal/batching_publisher_connection.h"
#include "google/cloud/pubsub/internal/create_channel.h"
#include "google/cloud/pubsub/internal/default_batch_sink.h"
#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/internal/flow_controlled_publisher_connection.h"
#include "google/cloud/pubsub/internal/ordering_key_publisher_connection.h"
#include "google/cloud/pubsub/internal/publisher_auth.h"
#include "google/cloud/pubsub/internal/publisher_logging.h"
#include "google/cloud/pubsub/internal/publisher_metadata.h"
#include "google/cloud/pubsub/internal/publisher_round_robin.h"
#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/pubsub/internal/rejects_with_ordering_key.h"
#include "google/cloud/pubsub/internal/sequential_batch_sink.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/future_void.h"
#include "google/cloud/internal/non_constructible.h"
#include "google/cloud/log.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {
class ContainingPublisherConnection : public PublisherConnection {
 public:
  ContainingPublisherConnection(std::shared_ptr<BackgroundThreads> background,
                                std::shared_ptr<PublisherConnection> child)
      : background_(std::move(background)), child_(std::move(child)) {}

  ~ContainingPublisherConnection() override = default;

  future<StatusOr<std::string>> Publish(PublishParams p) override {
    return child_->Publish(std::move(p));
  }
  void Flush(FlushParams p) override { child_->Flush(std::move(p)); }
  void ResumePublish(ResumePublishParams p) override {
    child_->ResumePublish(std::move(p));
  }

 private:
  std::shared_ptr<BackgroundThreads> background_;
  std::shared_ptr<PublisherConnection> child_;
};

std::shared_ptr<pubsub_internal::PublisherStub> DecoratePublisherStub(
    Options const& opts,
    std::shared_ptr<internal::GrpcAuthenticationStrategy> auth,
    std::vector<std::shared_ptr<pubsub_internal::PublisherStub>> children) {
  std::shared_ptr<pubsub_internal::PublisherStub> stub =
      std::make_shared<pubsub_internal::PublisherRoundRobin>(
          std::move(children));
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

std::shared_ptr<pubsub::PublisherConnection> ConnectionFromDecoratedStub(
    pubsub::Topic topic, Options opts,
    std::shared_ptr<BackgroundThreads> background,
    std::shared_ptr<pubsub_internal::PublisherStub> stub) {
  auto make_connection = [&]() -> std::shared_ptr<pubsub::PublisherConnection> {
    auto cq = background->cq();
    std::shared_ptr<pubsub_internal::BatchSink> sink =
        pubsub_internal::DefaultBatchSink::Create(stub, cq, opts);
    if (opts.get<pubsub::MessageOrderingOption>()) {
      auto factory = [topic, opts, sink, cq](std::string const& key) {
        auto used_sink = sink;
        if (!key.empty()) {
          used_sink = pubsub_internal::SequentialBatchSink::Create(sink);
        }
        return pubsub_internal::BatchingPublisherConnection::Create(
            topic, opts, key, std::move(used_sink), cq);
      };
      return pubsub_internal::OrderingKeyPublisherConnection::Create(
          std::move(factory));
    }
    return pubsub_internal::RejectsWithOrderingKey::Create(
        pubsub_internal::BatchingPublisherConnection::Create(
            std::move(topic), opts, {}, std::move(sink), std::move(cq)));
  };
  auto connection = make_connection();
  if (opts.get<pubsub::FullPublisherActionOption>() !=
      pubsub::FullPublisherAction::kIgnored) {
    connection = pubsub_internal::FlowControlledPublisherConnection::Create(
        std::move(opts), std::move(connection));
  }
  return std::make_shared<pubsub::ContainingPublisherConnection>(
      std::move(background), std::move(connection));
}
}  // namespace

PublisherConnection::~PublisherConnection() = default;

// NOLINTNEXTLINE(performance-unnecessary-value-param)
future<StatusOr<std::string>> PublisherConnection::Publish(PublishParams) {
  return make_ready_future(StatusOr<std::string>(
      Status{StatusCode::kUnimplemented, "needs-override"}));
}

void PublisherConnection::Flush(FlushParams) {}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
void PublisherConnection::ResumePublish(ResumePublishParams) {}

std::shared_ptr<PublisherConnection> MakePublisherConnection(
    Topic topic, std::initializer_list<internal::NonConstructible>) {
  return MakePublisherConnection(std::move(topic));
}

std::shared_ptr<PublisherConnection> MakePublisherConnection(Topic topic,
                                                             Options opts) {
  internal::CheckExpectedOptions<CommonOptionList, GrpcOptionList,
                                 PolicyOptionList, PublisherOptionList>(
      opts, __func__);
  opts = pubsub_internal::DefaultPublisherOptions(std::move(opts));

  auto background = internal::MakeBackgroundThreadsFactory(opts)();
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      background->cq(), opts);
  std::vector<std::shared_ptr<pubsub_internal::PublisherStub>> children(
      opts.get<GrpcNumChannelsOption>());
  int id = 0;
  std::generate(children.begin(), children.end(), [&id, &opts, &auth] {
    return pubsub_internal::CreateDefaultPublisherStub(
        auth->CreateChannel(opts.get<EndpointOption>(),
                            pubsub_internal::MakeChannelArguments(opts, id++)));
  });
  auto stub = DecoratePublisherStub(opts, std::move(auth), std::move(children));
  return ConnectionFromDecoratedStub(std::move(topic), std::move(opts),
                                     std::move(background), std::move(stub));
}

std::shared_ptr<PublisherConnection> MakePublisherConnection(
    Topic topic, PublisherOptions options, ConnectionOptions connection_options,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  auto opts = internal::MergeOptions(
      pubsub_internal::MakeOptions(std::move(options)),
      internal::MakeOptions(std::move(connection_options)));
  if (retry_policy) opts.set<RetryPolicyOption>(retry_policy->clone());
  if (backoff_policy) opts.set<BackoffPolicyOption>(backoff_policy->clone());
  return MakePublisherConnection(std::move(topic), std::move(opts));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub

namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

std::shared_ptr<pubsub::PublisherConnection> MakeTestPublisherConnection(
    pubsub::Topic topic, Options opts,
    std::vector<std::shared_ptr<PublisherStub>> stubs) {
  auto background = internal::MakeBackgroundThreadsFactory(opts)();
  auto auth = google::cloud::internal::CreateAuthenticationStrategy(
      background->cq(), opts);
  auto stub =
      pubsub::DecoratePublisherStub(opts, std::move(auth), std::move(stubs));
  return pubsub::ConnectionFromDecoratedStub(std::move(topic), std::move(opts),
                                             std::move(background),
                                             std::move(stub));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

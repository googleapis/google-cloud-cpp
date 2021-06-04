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

#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/internal/batching_publisher_connection.h"
#include "google/cloud/pubsub/internal/default_batch_sink.h"
#include "google/cloud/pubsub/internal/default_retry_policies.h"
#include "google/cloud/pubsub/internal/flow_controlled_publisher_connection.h"
#include "google/cloud/pubsub/internal/ordering_key_publisher_connection.h"
#include "google/cloud/pubsub/internal/publisher_logging.h"
#include "google/cloud/pubsub/internal/publisher_metadata.h"
#include "google/cloud/pubsub/internal/publisher_round_robin.h"
#include "google/cloud/pubsub/internal/publisher_stub.h"
#include "google/cloud/pubsub/internal/rejects_with_ordering_key.h"
#include "google/cloud/pubsub/internal/sequential_batch_sink.h"
#include "google/cloud/future_void.h"
#include "google/cloud/log.h"
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
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
    Topic topic, PublisherOptions options, ConnectionOptions connection_options,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  std::vector<std::shared_ptr<pubsub_internal::PublisherStub>> children(
      connection_options.num_channels());
  int id = 0;
  std::generate(children.begin(), children.end(), [&id, &connection_options] {
    return pubsub_internal::CreateDefaultPublisherStub(connection_options,
                                                       id++);
  });
  return pubsub_internal::MakePublisherConnection(
      std::move(topic), std::move(options), std::move(connection_options),
      std::move(children), std::move(retry_policy), std::move(backoff_policy));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::shared_ptr<pubsub::PublisherConnection> MakePublisherConnection(
    pubsub::Topic topic, pubsub::PublisherOptions options,
    pubsub::ConnectionOptions connection_options,
    std::shared_ptr<PublisherStub> stub,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  std::vector<std::shared_ptr<PublisherStub>> children{std::move(stub)};
  return MakePublisherConnection(
      std::move(topic), std::move(options), std::move(connection_options),
      std::move(children), std::move(retry_policy), std::move(backoff_policy));
}

std::shared_ptr<pubsub::PublisherConnection> MakePublisherConnection(
    pubsub::Topic topic, pubsub::PublisherOptions options,
    pubsub::ConnectionOptions connection_options,
    std::vector<std::shared_ptr<PublisherStub>> stubs,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  if (stubs.empty()) return nullptr;
  if (!retry_policy) retry_policy = DefaultRetryPolicy();
  if (!backoff_policy) backoff_policy = DefaultBackoffPolicy();
  std::shared_ptr<PublisherStub> stub =
      std::make_shared<PublisherRoundRobin>(std::move(stubs));
  stub = std::make_shared<PublisherMetadata>(std::move(stub));
  if (connection_options.tracing_enabled("rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<PublisherLogging>(
        std::move(stub), connection_options.tracing_options());
  }

  auto default_thread_pool_size = []() -> std::size_t {
    auto constexpr kDefaultThreadPoolSize = 4;
    auto const n = std::thread::hardware_concurrency();
    return n == 0 ? kDefaultThreadPoolSize : n;
  };
  if (connection_options.background_thread_pool_size() == 0) {
    connection_options.set_background_thread_pool_size(
        default_thread_pool_size());
  }

  auto background = connection_options.background_threads_factory()();
  auto make_connection = [&]() -> std::shared_ptr<pubsub::PublisherConnection> {
    auto cq = background->cq();
    std::shared_ptr<BatchSink> sink = DefaultBatchSink::Create(
        stub, cq, std::move(retry_policy), std::move(backoff_policy));
    if (options.message_ordering()) {
      auto factory = [topic, options, sink, cq](std::string const& key) {
        return BatchingPublisherConnection::Create(
            topic, options, key, SequentialBatchSink::Create(std::move(sink)),
            cq);
      };
      return OrderingKeyPublisherConnection::Create(std::move(factory));
    }
    return RejectsWithOrderingKey::Create(BatchingPublisherConnection::Create(
        std::move(topic), std::move(options), {}, sink, std::move(cq)));
  };
  auto connection = make_connection();
  if (options.full_publisher_rejects() || options.full_publisher_blocks()) {
    connection = FlowControlledPublisherConnection::Create(
        options, std::move(connection));
  }
  return std::make_shared<pubsub::ContainingPublisherConnection>(
      std::move(background), std::move(connection));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

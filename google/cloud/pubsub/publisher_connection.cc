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
#include "google/cloud/pubsub/internal/ordering_key_publisher_connection.h"
#include "google/cloud/pubsub/internal/publisher_logging.h"
#include "google/cloud/pubsub/internal/publisher_metadata.h"
#include "google/cloud/pubsub/internal/publisher_stub.h"
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

 private:
  std::shared_ptr<BackgroundThreads> background_;
  std::shared_ptr<PublisherConnection> child_;
};
}  // namespace

PublisherConnection::~PublisherConnection() = default;

std::shared_ptr<PublisherConnection> MakePublisherConnection(
    Topic topic, PublisherOptions options,
    ConnectionOptions connection_options) {
  auto stub = pubsub_internal::CreateDefaultPublisherStub(connection_options,
                                                          /*channel_id=*/0);
  return pubsub_internal::MakePublisherConnection(
      std::move(topic), std::move(options), std::move(connection_options),
      std::move(stub));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

std::shared_ptr<pubsub::PublisherConnection> MakePublisherConnection(
    pubsub::Topic topic, pubsub::PublisherOptions options,
    pubsub::ConnectionOptions connection_options,
    std::shared_ptr<PublisherStub> stub) {
  stub = std::make_shared<pubsub_internal::PublisherMetadata>(std::move(stub));
  if (connection_options.tracing_enabled("rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<pubsub_internal::PublisherLogging>(
        std::move(stub), connection_options.tracing_options());
  }

  auto default_thread_pool_size = []() -> std::size_t {
    auto constexpr kMinThreadPoolSize = 4;
    auto const n = std::thread::hardware_concurrency();
    return n == 0 ? kMinThreadPoolSize : n;
  };
  if (connection_options.background_thread_pool_size() == 0) {
    connection_options.set_background_thread_pool_size(
        default_thread_pool_size());
  }

  auto background = connection_options.background_threads_factory()();
  auto make_connection = [&]() -> std::shared_ptr<pubsub::PublisherConnection> {
    auto cq = background->cq();
    if (options.message_ordering()) {
      auto factory = [topic, options, stub, cq](std::string const&) {
        return BatchingPublisherConnection::Create(topic, options, stub, cq);
      };
      return OrderingKeyPublisherConnection::Create(std::move(factory));
    }
    return BatchingPublisherConnection::Create(
        std::move(topic), std::move(options), std::move(stub), std::move(cq));
  };
  return std::make_shared<pubsub::ContainingPublisherConnection>(
      std::move(background), make_connection());
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

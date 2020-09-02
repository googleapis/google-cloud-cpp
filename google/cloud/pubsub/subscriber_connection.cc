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

#include "google/cloud/pubsub/subscriber_connection.h"
#include "google/cloud/pubsub/internal/subscriber_logging.h"
#include "google/cloud/pubsub/internal/subscriber_metadata.h"
#include "google/cloud/pubsub/internal/subscription_session.h"
#include "google/cloud/log.h"
#include <algorithm>
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

SubscriberConnection::~SubscriberConnection() = default;

std::shared_ptr<SubscriberConnection> MakeSubscriberConnection(
    ConnectionOptions options) {
  auto stub =
      pubsub_internal::CreateDefaultSubscriberStub(options, /*channel_id=*/0);
  return pubsub_internal::MakeSubscriberConnection(std::move(stub),
                                                   std::move(options));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {
class SubscriberConnectionImpl : public pubsub::SubscriberConnection {
 public:
  explicit SubscriberConnectionImpl(
      std::shared_ptr<pubsub_internal::SubscriberStub> stub,
      pubsub::ConnectionOptions const& options)
      : stub_(std::move(stub)),
        background_(options.background_threads_factory()()) {}

  ~SubscriberConnectionImpl() override = default;

  future<Status> Subscribe(SubscribeParams p) override {
    return CreateSubscriptionSession(stub_, background_->cq(), std::move(p));
  }

 private:
  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
  std::shared_ptr<BackgroundThreads> background_;
};
}  // namespace

std::shared_ptr<pubsub::SubscriberConnection> MakeSubscriberConnection(
    std::shared_ptr<SubscriberStub> stub, pubsub::ConnectionOptions options) {
  stub = std::make_shared<SubscriberMetadata>(std::move(stub));
  if (options.tracing_enabled("rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<pubsub_internal::SubscriberLogging>(
        std::move(stub), options.tracing_options());
  }
  auto default_thread_pool_size = []() -> std::size_t {
    auto constexpr kDefaultThreadPoolSize = 4;
    auto const n = std::thread::hardware_concurrency();
    return n == 0 ? kDefaultThreadPoolSize : n;
  };
  if (options.background_thread_pool_size() == 0) {
    options.set_background_thread_pool_size(default_thread_pool_size());
  }
  return std::make_shared<SubscriberConnectionImpl>(std::move(stub), options);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

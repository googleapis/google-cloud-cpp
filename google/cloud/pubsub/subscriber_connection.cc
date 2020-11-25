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
#include "google/cloud/pubsub/internal/default_retry_policies.h"
#include "google/cloud/pubsub/internal/subscriber_logging.h"
#include "google/cloud/pubsub/internal/subscriber_metadata.h"
#include "google/cloud/pubsub/internal/subscriber_round_robin.h"
#include "google/cloud/pubsub/internal/subscription_session.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include <algorithm>
#include <limits>
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

SubscriberConnection::~SubscriberConnection() = default;

// NOLINTNEXTLINE(performance-unnecessary-value-param)
future<Status> SubscriberConnection::Subscribe(SubscribeParams) {
  return make_ready_future(
      Status{StatusCode::kUnimplemented, "needs-override"});
}

std::shared_ptr<SubscriberConnection> MakeSubscriberConnection(
    Subscription subscription, SubscriberOptions options,
    ConnectionOptions connection_options,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  std::vector<std::shared_ptr<pubsub_internal::SubscriberStub>> children(
      connection_options.num_channels());
  int id = 0;
  std::generate(children.begin(), children.end(), [&id, &connection_options] {
    return pubsub_internal::CreateDefaultSubscriberStub(connection_options,
                                                        id++);
  });
  return pubsub_internal::MakeSubscriberConnection(
      std::move(subscription), std::move(options),
      std::move(connection_options), std::move(children),
      std::move(retry_policy), std::move(backoff_policy));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {
class SubscriberConnectionImpl : public pubsub::SubscriberConnection {
 public:
  explicit SubscriberConnectionImpl(
      pubsub::Subscription subscription, pubsub::SubscriberOptions options,
      pubsub::ConnectionOptions const& connection_options,
      std::shared_ptr<pubsub_internal::SubscriberStub> stub,
      std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
      std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy)
      : subscription_(std::move(subscription)),
        options_(std::move(options)),
        stub_(std::move(stub)),
        background_(connection_options.background_threads_factory()()),
        retry_policy_(std::move(retry_policy)),
        backoff_policy_(std::move(backoff_policy)),
        generator_(google::cloud::internal::MakeDefaultPRNG()) {}

  ~SubscriberConnectionImpl() override = default;

  future<Status> Subscribe(SubscribeParams p) override {
    auto client_id = [this] {
      std::lock_guard<std::mutex> lk(mu_);
      auto constexpr kLength = 32;
      auto constexpr kChars = "abcdefghijklmnopqrstuvwxyz0123456789";
      return google::cloud::internal::Sample(generator_, kLength, kChars);
    }();
    return CreateSubscriptionSession(
        subscription_, options_, stub_, background_->cq(), std::move(client_id),
        std::move(p), retry_policy_->clone(), backoff_policy_->clone());
  }

 private:
  pubsub::Subscription const subscription_;
  pubsub::SubscriberOptions const options_;
  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
  std::shared_ptr<BackgroundThreads> background_;
  std::unique_ptr<pubsub::RetryPolicy const> retry_policy_;
  std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy_;
  std::mutex mu_;
  google::cloud::internal::DefaultPRNG generator_;
};
}  // namespace

std::shared_ptr<pubsub::SubscriberConnection> MakeSubscriberConnection(
    pubsub::Subscription subscription, pubsub::SubscriberOptions options,
    pubsub::ConnectionOptions connection_options,
    std::shared_ptr<SubscriberStub> stub,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  std::vector<std::shared_ptr<SubscriberStub>> children{std::move(stub)};
  return MakeSubscriberConnection(std::move(subscription), std::move(options),
                                  std::move(connection_options),
                                  std::move(children), std::move(retry_policy),
                                  std::move(backoff_policy));
}

std::shared_ptr<pubsub::SubscriberConnection> MakeSubscriberConnection(
    pubsub::Subscription subscription, pubsub::SubscriberOptions options,
    pubsub::ConnectionOptions connection_options,
    std::vector<std::shared_ptr<SubscriberStub>> stubs,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  auto default_retry_policy = [] {
    // Subscribers are special: by default we want to retry essentially forever
    // because (a) the service will disconnect the streaming pull from time to
    // time, but that is not a "failure", (b) applications can change this
    // behavior if they need, and this is easier than some hard-coded
    // "treat these disconnects as non-failures" code.
    return pubsub::LimitedErrorCountRetryPolicy(std::numeric_limits<int>::max())
        .clone();
  };
  if (stubs.empty()) return nullptr;
  if (!retry_policy) retry_policy = default_retry_policy();
  if (!backoff_policy) backoff_policy = DefaultBackoffPolicy();
  std::shared_ptr<SubscriberStub> stub =
      std::make_shared<SubscriberRoundRobin>(std::move(stubs));
  stub = std::make_shared<SubscriberMetadata>(std::move(stub));
  if (connection_options.tracing_enabled("rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<pubsub_internal::SubscriberLogging>(
        std::move(stub), connection_options.tracing_options(),
        connection_options.tracing_enabled("rpc-streams"));
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
  return std::make_shared<SubscriberConnectionImpl>(
      std::move(subscription), std::move(options),
      std::move(connection_options), std::move(stub), std::move(retry_policy),
      std::move(backoff_policy));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

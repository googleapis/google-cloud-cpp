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
#include "google/cloud/pubsub/internal/defaults.h"
#include "google/cloud/pubsub/internal/subscriber_logging.h"
#include "google/cloud/pubsub/internal/subscriber_metadata.h"
#include "google/cloud/pubsub/internal/subscriber_round_robin.h"
#include "google/cloud/pubsub/internal/subscription_session.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/retry_policy.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/log.h"
#include <algorithm>
#include <limits>
#include <memory>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_NS {

SubscriberConnection::~SubscriberConnection() = default;

// NOLINTNEXTLINE(performance-unnecessary-value-param)
future<Status> SubscriberConnection::Subscribe(SubscribeParams) {
  return make_ready_future(
      Status{StatusCode::kUnimplemented, "needs-override"});
}

std::shared_ptr<SubscriberConnection> MakeSubscriberConnection(
    Subscription subscription,
    std::initializer_list<pubsub_internal::NonConstructible>) {
  return MakeSubscriberConnection(std::move(subscription));
}

std::shared_ptr<SubscriberConnection> MakeSubscriberConnection(
    Subscription subscription, Options opts) {
  internal::CheckExpectedOptions<CommonOptionList, GrpcOptionList,
                                 PolicyOptionList, SubscriberOptionList>(
      opts, __func__);
  opts = pubsub_internal::DefaultSubscriberOptions(std::move(opts));
  std::vector<std::shared_ptr<pubsub_internal::SubscriberStub>> children(
      opts.get<GrpcNumChannelsOption>());
  int id = 0;
  std::generate(children.begin(), children.end(), [&id, &opts] {
    return pubsub_internal::CreateDefaultSubscriberStub(opts, id++);
  });
  return pubsub_internal::MakeSubscriberConnection(
      std::move(subscription), std::move(opts), std::move(children));
}

std::shared_ptr<SubscriberConnection> MakeSubscriberConnection(
    Subscription subscription, SubscriberOptions options,
    ConnectionOptions connection_options,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  auto opts = internal::MergeOptions(
      pubsub_internal::MakeOptions(std::move(options)),
      internal::MakeOptions(std::move(connection_options)));
  if (retry_policy) opts.set<RetryPolicyOption>(retry_policy->clone());
  if (backoff_policy) opts.set<BackoffPolicyOption>(backoff_policy->clone());
  return MakeSubscriberConnection(std::move(subscription), std::move(opts));
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {
class SubscriberConnectionImpl : public pubsub::SubscriberConnection {
 public:
  explicit SubscriberConnectionImpl(
      pubsub::Subscription subscription, Options opts,
      std::shared_ptr<pubsub_internal::SubscriberStub> stub)
      : subscription_(std::move(subscription)),
        opts_(std::move(opts)),
        stub_(std::move(stub)),
        background_(internal::MakeBackgroundThreadsFactory(opts_)()),
        generator_(internal::MakeDefaultPRNG()) {}

  ~SubscriberConnectionImpl() override = default;

  future<Status> Subscribe(SubscribeParams p) override {
    auto client_id = [this] {
      std::lock_guard<std::mutex> lk(mu_);
      auto constexpr kLength = 32;
      auto constexpr kChars = "abcdefghijklmnopqrstuvwxyz0123456789";
      return internal::Sample(generator_, kLength, kChars);
    }();
    return CreateSubscriptionSession(subscription_, opts_, stub_,
                                     background_->cq(), std::move(client_id),
                                     std::move(p));
  }

 private:
  pubsub::Subscription const subscription_;
  Options const opts_;
  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
  std::shared_ptr<BackgroundThreads> background_;
  std::mutex mu_;
  internal::DefaultPRNG generator_;
};
}  // namespace

std::shared_ptr<pubsub::SubscriberConnection> MakeSubscriberConnection(
    pubsub::Subscription subscription, Options opts,
    std::vector<std::shared_ptr<SubscriberStub>> stubs) {
  if (stubs.empty()) return nullptr;
  std::shared_ptr<SubscriberStub> stub =
      std::make_shared<SubscriberRoundRobin>(std::move(stubs));
  stub = std::make_shared<SubscriberMetadata>(std::move(stub));
  auto const& tracing = opts.get<TracingComponentsOption>();
  if (internal::Contains(tracing, "rpc")) {
    GCP_LOG(INFO) << "Enabled logging for gRPC calls";
    stub = std::make_shared<SubscriberLogging>(
        std::move(stub), opts.get<GrpcTracingOptionsOption>(),
        internal::Contains(tracing, "rpc-streams"));
  }
  return std::make_shared<SubscriberConnectionImpl>(
      std::move(subscription), std::move(opts), std::move(stub));
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

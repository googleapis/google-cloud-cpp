// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/internal/subscriber_connection_impl.h"
#include "google/cloud/pubsub/internal/ack_handler_wrapper.h"
#include "google/cloud/pubsub/internal/default_pull_ack_handler.h"
#include "google/cloud/pubsub/internal/subscription_session.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/retry_loop.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

SubscriberConnectionImpl::SubscriberConnectionImpl(
    Options opts, std::shared_ptr<pubsub_internal::SubscriberStub> stub)
    : opts_(std::move(opts)),
      stub_(std::move(stub)),
      background_(internal::MakeBackgroundThreadsFactory(opts_)()),
      generator_(internal::MakeDefaultPRNG()) {}

SubscriberConnectionImpl::~SubscriberConnectionImpl() = default;

future<Status> SubscriberConnectionImpl::Subscribe(SubscribeParams p) {
  return CreateSubscriptionSession(google::cloud::internal::CurrentOptions(),
                                   stub_, background_->cq(), MakeClientId(),
                                   std::move(p.callback));
}

future<Status> SubscriberConnectionImpl::ExactlyOnceSubscribe(
    ExactlyOnceSubscribeParams p) {
  return CreateSubscriptionSession(google::cloud::internal::CurrentOptions(),
                                   stub_, background_->cq(), MakeClientId(),
                                   std::move(p.callback));
}

StatusOr<pubsub::PullResponse> SubscriberConnectionImpl::Pull() {
  auto const& current = internal::CurrentOptions();
  auto subscription = current.get<pubsub::SubscriptionOption>();

  google::pubsub::v1::PullRequest request;
  request.set_subscription(subscription.FullName());
  request.set_max_messages(1);

  auto response = google::cloud::internal::RetryLoop(
      current.get<pubsub::RetryPolicyOption>()->clone(),
      current.get<pubsub::BackoffPolicyOption>()->clone(),
      google::cloud::Idempotency::kIdempotent,
      [stub = stub_](auto& context, auto const& request) {
        return stub->Pull(context, request);
      },
      request, __func__);
  if (!response) return std::move(response).status();
  if (response->received_messages_size() != 1) {
    return internal::InternalError("invalid response, mismatched ID count",
                                   GCP_ERROR_INFO());
  }
  auto received_message =
      std::move(response->mutable_received_messages()->at(0));
  auto impl = absl::make_unique<pubsub_internal::DefaultPullAckHandler>(
      background_->cq(), stub_, current, std::move(subscription),
      std::move(*received_message.mutable_ack_id()),
      received_message.delivery_attempt());
  auto message = pubsub_internal::FromProto(
      std::move(*received_message.mutable_message()));
  return pubsub::PullResponse{pubsub::PullAckHandler(std::move(impl)),
                              std::move(message)};
}

Options SubscriberConnectionImpl::options() { return opts_; }

std::string SubscriberConnectionImpl::MakeClientId() {
  std::lock_guard<std::mutex> lk(mu_);
  auto constexpr kLength = 32;
  auto constexpr kChars = "abcdefghijklmnopqrstuvwxyz0123456789";
  return internal::Sample(generator_, kLength, kChars);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

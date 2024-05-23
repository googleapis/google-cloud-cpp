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
#include "google/cloud/pubsub/internal/pull_ack_handler_factory.h"
#include "google/cloud/pubsub/internal/subscription_session.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/retry_loop.h"
#include "google/cloud/internal/retry_loop_helpers.h"

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
  // Ask Pub/Sub to return at most 1 message.
  request.set_max_messages(1);

  auto retry_policy = current.get<pubsub::RetryPolicyOption>()->clone();
  auto backoff_policy = current.get<pubsub::BackoffPolicyOption>()->clone();
  google::cloud::Status last_status;
  while (!retry_policy->IsExhausted()) {
    grpc::ClientContext context;
    google::cloud::internal::ConfigureContext(context, current);
    auto response = stub_->Pull(context, current, request);

    if (response && !response->received_messages().empty()) {
      if (response->received_messages_size() > 1) {
        return internal::InternalError("invalid response, mismatched ID count",
                                       GCP_ERROR_INFO());
      }

      auto received_message =
          std::move(response->mutable_received_messages()->at(0));
      auto ack_handler =
          MakePullAckHandler(background_->cq(), stub_, std::move(subscription),
                             std::move(*received_message.mutable_ack_id()),
                             received_message.delivery_attempt(), current);
      auto message = pubsub_internal::FromProto(
          std::move(*received_message.mutable_message()));
      return pubsub::PullResponse{std::move(ack_handler), std::move(message)};
    }
    last_status = std::move(response).status();
    if (!retry_policy->OnFailure(last_status)) {
      break;
    }
    std::this_thread::sleep_for(backoff_policy->OnCompletion());
  }
  if (last_status.ok()) {
    return google::cloud::internal::UnavailableError(
        "no messages returned",
        GCP_ERROR_INFO().WithMetadata("gl-cpp.error.origin", "client"));
  }
  return google::cloud::internal::RetryLoopError(last_status, __func__,
                                                 retry_policy->IsExhausted());
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

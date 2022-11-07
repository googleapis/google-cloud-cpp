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

#include "google/cloud/pubsub/internal/pull_ack_handler.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/internal/async_retry_loop.h"
#include <google/pubsub/v1/pubsub.pb.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

auto constexpr kMinimalLeaseExtension = std::chrono::seconds(10);

std::chrono::system_clock::time_point DefaultLeaseDeadline(
    std::chrono::system_clock::time_point now, Options const& options) {
  return now + options.get<pubsub::MaxDeadlineTimeOption>();
}

std::chrono::seconds DefaultLeaseExtension(Options const& options) {
  if (options.has<pubsub::MinDeadlineExtensionOption>()) {
    return options.get<pubsub::MinDeadlineExtensionOption>();
  }
  return options.get<pubsub::MaxDeadlineExtensionOption>();
}

}  // namespace

PullAckHandler::PullAckHandler(CompletionQueue cq,
                               std::weak_ptr<SubscriberStub> w, Options options,
                               pubsub::Subscription subscription,
                               std::string ack_id,
                               std::int32_t delivery_attempt, Clock clock)
    : cq_(std::move(cq)),
      stub_(std::move(w)),
      options_(std::move(options)),
      subscription_(std::move(subscription)),
      ack_id_(std::move(ack_id)),
      delivery_attempt_(delivery_attempt),
      clock_(std::move(clock)),
      lease_deadline_(DefaultLeaseDeadline(clock_(), options_)),
      lease_extension_(DefaultLeaseExtension(options_)),
      current_lease_(clock_() + kMinimalLeaseExtension) {}

PullAckHandler::~PullAckHandler() {
  if (!timer_.valid()) return;
  timer_.cancel();
}

future<Status> PullAckHandler::ack() {
  if (auto s = stub_.lock()) {
    google::pubsub::v1::AcknowledgeRequest request;
    request.set_subscription(subscription_.FullName());
    request.add_ack_ids(ack_id_);
    return internal::AsyncRetryLoop(
        options_.get<pubsub::RetryPolicyOption>()->clone(),
        options_.get<pubsub::BackoffPolicyOption>()->clone(),
        google::cloud::Idempotency::kIdempotent, cq_,
        [stub = std::move(s)](auto cq, auto context, auto const& request) {
          return stub->AsyncAcknowledge(cq, std::move(context), request);
        },
        request, __func__);
  }
  return make_ready_future(
      Status(StatusCode::kFailedPrecondition, "session already shutdown"));
}

future<Status> PullAckHandler::nack() {
  if (auto s = stub_.lock()) {
    google::pubsub::v1::ModifyAckDeadlineRequest request;
    request.set_subscription(subscription_.FullName());
    request.set_ack_deadline_seconds(0);
    request.add_ack_ids(ack_id_);
    return internal::AsyncRetryLoop(
        options_.get<pubsub::RetryPolicyOption>()->clone(),
        options_.get<pubsub::BackoffPolicyOption>()->clone(),
        google::cloud::Idempotency::kIdempotent, cq_,
        [stub = std::move(s)](auto cq, auto context, auto const& request) {
          return stub->AsyncModifyAckDeadline(cq, std::move(context), request);
        },
        request, __func__);
  }
  return make_ready_future(
      Status(StatusCode::kFailedPrecondition, "session already shutdown"));
}

std::int32_t PullAckHandler::delivery_attempt() const {
  return delivery_attempt_;
}

void PullAckHandler::StartLeaseLoop() {
  auto s = stub_.lock();
  if (!s) return;
  auto const now = clock_();

  // Check if the lease has expired, or is so close to expiring that we cannot
  // extend it. In either case, simply return and stop the loop.
  std::unique_lock<std::mutex> lk(mu_);
  if (current_lease_ <= now || lease_deadline_ <= now) return;
  lk.unlock();
  auto const remaining =
      std::chrono::duration_cast<std::chrono::seconds>(lease_deadline_ - now);
  auto const es =
      std::chrono::duration_cast<std::chrono::seconds>(lease_extension_);
  auto const extension = (std::min)(remaining, es);
  if (extension == std::chrono::seconds(0)) return;

  google::pubsub::v1::ModifyAckDeadlineRequest request;
  request.set_subscription(subscription_.FullName());
  request.set_ack_deadline_seconds(
      static_cast<std::int32_t>(extension.count()));
  request.add_ack_ids(ack_id_);
  auto self = std::weak_ptr<PullAckHandler>(shared_from_this());
  internal::AsyncRetryLoop(
      options_.get<pubsub::RetryPolicyOption>()->clone(),
      options_.get<pubsub::BackoffPolicyOption>()->clone(),
      google::cloud::Idempotency::kIdempotent, cq_,
      [stub = std::move(s), deadline = now + extension, clock = clock_](
          auto cq, auto context, auto const& request) {
        if (deadline < clock()) {
          return make_ready_future(
              Status(StatusCode::kDeadlineExceeded, "lease already expired"));
        }
        context->set_deadline((std::min)(deadline, context->deadline()));
        return stub->AsyncModifyAckDeadline(cq, std::move(context), request);
      },
      request, __func__)
      .then([w = std::move(self), deadline = now + extension](auto f) {
        if (auto self = w.lock()) self->OnLeaseExtended(deadline, f.get());
      });
}

std::chrono::milliseconds PullAckHandler::LeaseRefreshPeriod() const {
  auto constexpr kLeaseExtensionSlack = std::chrono::seconds(1);
  if (lease_extension_ > 2 * kLeaseExtensionSlack) {
    return lease_extension_ - kLeaseExtensionSlack;
  }
  return std::chrono::milliseconds(500);
}

void PullAckHandler::OnLeaseTimer(Status const& timer_status) {
  if (!timer_status.ok()) return;
  StartLeaseLoop();
}

void PullAckHandler::OnLeaseExtended(
    std::chrono::system_clock::time_point new_deadline,
    google::cloud::Status const& status) {
  if (!status.ok()) return;

  auto self = std::weak_ptr<PullAckHandler>(shared_from_this());
  auto timer = cq_.MakeRelativeTimer(LeaseRefreshPeriod())
                   .then([w = std::move(self)](auto f) {
                     if (auto self = w.lock())
                       self->OnLeaseTimer(f.get().status());
                   });
  // We need to cancel the timer in the destructor.
  std::lock_guard<std::mutex> lk(mu_);
  timer_ = std::move(timer);
  current_lease_ = new_deadline;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

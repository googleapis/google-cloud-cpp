// Copyright 2023 Google LLC
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

#include "google/cloud/pubsub/internal/default_pull_lease_manager.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/internal/async_retry_loop.h"
#include "google/cloud/internal/make_status.h"

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

DefaultPullLeaseManager::DefaultPullLeaseManager(
    CompletionQueue cq, std::weak_ptr<SubscriberStub> w, Options options,
    pubsub::Subscription subscription, std::string ack_id,
    std::shared_ptr<PullLeaseManagerImpl> impl, std::shared_ptr<Clock> clock)
    : cq_(std::move(cq)),
      stub_(std::move(w)),
      options_(
          google::cloud::internal::MakeImmutableOptions(std::move(options))),
      subscription_(std::move(subscription)),
      ack_id_(std::move(ack_id)),
      impl_(std::move(impl)),
      clock_(std::move(clock)),
      lease_deadline_(DefaultLeaseDeadline(clock_->Now(), *options_)),
      lease_extension_(DefaultLeaseExtension(*options_)),
      current_lease_(clock_->Now() + kMinimalLeaseExtension) {}

DefaultPullLeaseManager::~DefaultPullLeaseManager() {
  if (!timer_.valid()) return;
  timer_.cancel();
}

void DefaultPullLeaseManager::StartLeaseLoop() {
  auto s = stub_.lock();
  if (!s) return;
  auto const now = clock_->Now();

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

  auto self = std::weak_ptr<DefaultPullLeaseManager>(shared_from_this());
  ExtendLease(std::move(s), now, extension)
      .then([w = std::move(self), deadline = now + extension](auto f) {
        if (auto self = w.lock()) self->OnLeaseExtended(deadline, f.get());
      });
}

future<Status> DefaultPullLeaseManager::ExtendLease(
    std::shared_ptr<SubscriberStub> stub,
    std::chrono::system_clock::time_point now, std::chrono::seconds extension) {
  google::pubsub::v1::ModifyAckDeadlineRequest request;
  request.set_subscription(subscription_.FullName());
  request.set_ack_deadline_seconds(
      static_cast<std::int32_t>(extension.count()));
  request.add_ack_ids(ack_id_);
  return internal::AsyncRetryLoop(
      options_->get<pubsub::RetryPolicyOption>()->clone(),
      options_->get<pubsub::BackoffPolicyOption>()->clone(),
      google::cloud::Idempotency::kIdempotent, cq_,
      [stub = std::move(stub), deadline = now + extension, clock = clock_,
       impl = impl_](auto cq, auto context, auto options, auto const& request) {
        if (deadline < clock->Now()) {
          return make_ready_future(internal::DeadlineExceededError(
              "lease already expired", GCP_ERROR_INFO()));
        }
        context->set_deadline((std::min)(deadline, context->deadline()));
        return impl->AsyncModifyAckDeadline(stub, cq, std::move(context),
                                            std::move(options), request);
      },
      options_, request, __func__);
}

std::chrono::milliseconds DefaultPullLeaseManager::LeaseRefreshPeriod() const {
  auto constexpr kLeaseExtensionSlack = std::chrono::seconds(1);
  if (lease_extension_ > 2 * kLeaseExtensionSlack) {
    return lease_extension_ - kLeaseExtensionSlack;
  }
  return std::chrono::milliseconds(500);
}

void DefaultPullLeaseManager::OnLeaseTimer(Status const& timer_status) {
  if (!timer_status.ok()) return;
  StartLeaseLoop();
}

void DefaultPullLeaseManager::OnLeaseExtended(
    std::chrono::system_clock::time_point new_deadline,
    google::cloud::Status const& status) {
  if (!status.ok()) return;

  auto self = std::weak_ptr<DefaultPullLeaseManager>(shared_from_this());
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

std::string DefaultPullLeaseManager::ack_id() const { return ack_id_; }

pubsub::Subscription DefaultPullLeaseManager::subscription() const {
  return subscription_;
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

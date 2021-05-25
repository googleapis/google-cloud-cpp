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

#include "google/cloud/pubsub/internal/subscription_session.h"
#include "google/cloud/pubsub/internal/streaming_subscription_batch_source.h"
#include "google/cloud/pubsub/internal/subscription_lease_management.h"
#include "google/cloud/pubsub/internal/subscription_message_queue.h"
#include "google/cloud/log.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

class SubscriptionSessionImpl
    : public std::enable_shared_from_this<SubscriptionSessionImpl> {
 public:
  static future<Status> Create(
      pubsub::SubscriberOptions const& options,
      google::cloud::CompletionQueue executor,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriptionBatchSource> source,
      pubsub::SubscriberConnection::SubscribeParams p) {
    auto queue =
        SubscriptionMessageQueue::Create(shutdown_manager, std::move(source));
    auto concurrency_control = SubscriptionConcurrencyControl::Create(
        executor, shutdown_manager, std::move(queue),
        options.max_concurrency());

    auto self = std::make_shared<SubscriptionSessionImpl>(
        std::move(executor), std::move(shutdown_manager),
        std::move(concurrency_control), options.shutdown_polling_period());

    auto weak = std::weak_ptr<SubscriptionSessionImpl>(self);
    auto result = self->shutdown_manager_->Start(promise<Status>([weak] {
      if (auto self = weak.lock()) self->InitiateApplicationShutdown();
    }));
    // Create a (periodic) timer, this serves two purposes:
    // 1) Effectively the timer owns "self" and the timer is owned by the
    //    CompletionQueue.
    // 2) When the completion queue is shutdown, the timer is canceled and
    //    `self` gets a chance to shutdown the pipeline.
    self->ScheduleTimer();
    self->pipeline_->Start(std::move(p.callback));
    return result.then([weak](future<Status> f) {
      if (auto self = weak.lock()) self->ShutdownCompleted();
      return f.get();
    });
  }

  SubscriptionSessionImpl(
      CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriptionConcurrencyControl> pipeline,
      std::chrono::milliseconds shutdown_polling_period)
      : cq_(std::move(cq)),
        shutdown_manager_(std::move(shutdown_manager)),
        shutdown_polling_period_(shutdown_polling_period),
        pipeline_(std::move(pipeline)) {}

 private:
  enum ShutdownState {
    kNotInShutdown,
    kShutdownByCompletionQueue,
    kShutdownByApplication,
    kShutdownCompleted,
  };

  void MarkAsShutdown(std::unique_lock<std::mutex> lk, ShutdownState reason,
                      char const* where) {
    auto pipeline = pipeline_;
    auto shutdown_manager = shutdown_manager_;
    if (reason == kShutdownByCompletionQueue) pipeline_.reset();
    shutdown_state_ = reason;
    lk.unlock();
    shutdown_manager->MarkAsShutdown(where, {});
    if (pipeline) pipeline->Shutdown();
  }

  void InitiateApplicationShutdown() {
    GCP_LOG(TRACE) << __func__ << "()";
    std::unique_lock<std::mutex> lk(mu_);
    switch (shutdown_state_) {
      case kNotInShutdown:
        MarkAsShutdown(std::move(lk), kShutdownByApplication, __func__);
        break;
      case kShutdownByApplication:
      case kShutdownByCompletionQueue:
      case kShutdownCompleted:
        break;
    }
  }

  void ShutdownCompleted() {
    ShutdownCompleted(std::unique_lock<std::mutex>(mu_));
  }
  void ShutdownCompleted(std::unique_lock<std::mutex>) {
    GCP_LOG(TRACE) << __func__ << "()";
    pipeline_.reset();
    shutdown_state_ = kShutdownCompleted;
    if (timer_.valid()) timer_.cancel();
  }

  void ScheduleTimer() {
    GCP_LOG(TRACE) << __func__ << "()";
    using TimerArg = future<StatusOr<std::chrono::system_clock::time_point>>;

    auto self = shared_from_this();
    auto t = cq_.MakeRelativeTimer(shutdown_polling_period_)
                 .then([self](TimerArg f) { self->OnTimer(!f.get().ok()); });
    std::unique_lock<std::mutex> lk(mu_);
    if (shutdown_state_ == kShutdownByCompletionQueue) return;
    timer_ = std::move(t);
  }

  void OnTimer(bool cancelled) {
    GCP_LOG(TRACE) << __func__ << "(" << cancelled << ")";
    if (!cancelled) {
      ScheduleTimer();
      return;
    }
    std::unique_lock<std::mutex> lk(mu_);
    switch (shutdown_state_) {
      case kNotInShutdown:
        MarkAsShutdown(std::move(lk), kShutdownByCompletionQueue, __func__);
        break;
      case kShutdownByApplication:
        ShutdownCompleted(std::move(lk));
        break;
      case kShutdownByCompletionQueue:
      case kShutdownCompleted:
        break;
    }
  }

  CompletionQueue cq_;
  std::shared_ptr<SessionShutdownManager> const shutdown_manager_;
  std::chrono::milliseconds const shutdown_polling_period_;

  std::mutex mu_;
  std::shared_ptr<SubscriptionConcurrencyControl> pipeline_;
  ShutdownState shutdown_state_ = kNotInShutdown;
  future<void> timer_;
};
}  // namespace

future<Status> CreateSubscriptionSession(
    pubsub::Subscription const& subscription,
    pubsub::SubscriberOptions const& options,
    std::shared_ptr<pubsub_internal::SubscriberStub> const& stub,
    google::cloud::CompletionQueue const& executor, std::string client_id,
    pubsub::SubscriberConnection::SubscribeParams p,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  auto shutdown_manager = std::make_shared<SessionShutdownManager>();
  auto batch = std::make_shared<StreamingSubscriptionBatchSource>(
      executor, shutdown_manager, stub, subscription.FullName(),
      std::move(client_id), options, std::move(retry_policy),
      std::move(backoff_policy));
  auto lease_management = SubscriptionLeaseManagement::Create(
      executor, shutdown_manager, std::move(batch), options.max_deadline_time(),
      options.max_deadline_extension());

  return SubscriptionSessionImpl::Create(
      options, std::move(executor), std::move(shutdown_manager),
      std::move(lease_management), std::move(p));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

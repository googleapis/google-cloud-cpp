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
#include "google/cloud/pubsub/internal/default_subscription_batch_source.h"
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
      google::cloud::CompletionQueue executor,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriptionBatchSource> source,
      pubsub::SubscriberConnection::SubscribeParams p) {
    auto flow_control = SubscriptionFlowControl::Create(
        executor, shutdown_manager, std::move(source),
        p.options.message_count_lwm(), p.options.message_count_hwm(),
        p.options.message_size_lwm(), p.options.message_size_hwm());
    auto concurrency_control = SubscriptionConcurrencyControl::Create(
        executor, shutdown_manager, std::move(flow_control),
        p.options.concurrency_lwm(), p.options.concurrency_hwm());

    auto self = std::make_shared<SubscriptionSessionImpl>(
        std::move(executor), std::move(shutdown_manager),
        std::move(concurrency_control), p.options.shutdown_polling_period());

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
  void InitiateApplicationShutdown() {
    GCP_LOG(DEBUG) << __func__ << "()";
    std::unique_lock<std::mutex> lk(mu_);
    std::shared_ptr<SubscriptionConcurrencyControl> p = pipeline_;
    switch (shutdown_state_) {
      case kNotInShutdown:
        shutdown_state_ = kShutdownByApplication;
        lk.unlock();
        shutdown_manager_->MarkAsShutdown(__func__, {});
        if (p) p->Shutdown();
        break;
      case kShutdownByApplication:
      case kShutdownByCompletionQueue:
      case kShutdownCompleted:
        break;
    }
  }

  void ShutdownCompleted() {
    GCP_LOG(DEBUG) << __func__ << "()";
    std::lock_guard<std::mutex> lk(mu_);
    pipeline_.reset();
    shutdown_state_ = kShutdownCompleted;
    if (timer_.valid()) timer_.cancel();
  }

  void ScheduleTimer() {
    GCP_LOG(DEBUG) << __func__ << "()";
    using TimerArg = future<StatusOr<std::chrono::system_clock::time_point>>;

    std::unique_lock<std::mutex> lk(mu_);
    auto self = shared_from_this();
    lk.unlock();
    auto t = cq_.MakeRelativeTimer(shutdown_polling_period_)
                 .then([self](TimerArg f) { self->OnTimer(!f.get().ok()); });
    lk.lock();
    if (shutdown_state_ == kShutdownByCompletionQueue) return;
    timer_ = std::move(t);
  }

  void OnTimer(bool cancelled) {
    GCP_LOG(DEBUG) << __func__ << "(" << cancelled << ")";
    if (!cancelled) {
      ScheduleTimer();
      return;
    }
    std::unique_lock<std::mutex> lk(mu_);
    std::shared_ptr<SubscriptionConcurrencyControl> p;
    switch (shutdown_state_) {
      case kNotInShutdown:
        shutdown_state_ = kShutdownByCompletionQueue;
        p.swap(pipeline_);
        shutdown_manager_->MarkAsShutdown(__func__, {});
        lk.unlock();
        p->Shutdown();
        break;
      case kShutdownByApplication:
        shutdown_state_ = kShutdownCompleted;
        p.swap(pipeline_);
        lk.unlock();
        break;
      case kShutdownByCompletionQueue:
      case kShutdownCompleted:
        break;
    }
  }

  enum ShutdownState {
    kNotInShutdown,
    kShutdownByCompletionQueue,
    kShutdownByApplication,
    kShutdownCompleted,
  };

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
    std::shared_ptr<pubsub_internal::SubscriberStub> const& stub,
    google::cloud::CompletionQueue const& executor,
    pubsub::SubscriberConnection::SubscribeParams p,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  auto shutdown_manager = std::make_shared<SessionShutdownManager>();
  auto batch = std::make_shared<DefaultSubscriptionBatchSource>(
      executor, stub, std::move(p.full_subscription_name),
      std::move(retry_policy), std::move(backoff_policy));
  auto lease_management = SubscriptionLeaseManagement::Create(
      executor, shutdown_manager, std::move(batch),
      p.options.max_deadline_time());

  return SubscriptionSessionImpl::Create(
      std::move(executor), std::move(shutdown_manager),
      std::move(lease_management), std::move(p));
}

future<Status> CreateTestingSubscriptionSession(
    std::shared_ptr<pubsub_internal::SubscriberStub> const& stub,
    google::cloud::CompletionQueue const& executor,
    pubsub::SubscriberConnection::SubscribeParams p,
    std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
    std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
  if (!retry_policy) {
    retry_policy = pubsub::LimitedErrorCountRetryPolicy(3).clone();
  }
  if (!backoff_policy) {
    using us = std::chrono::microseconds;
    backoff_policy =
        pubsub::ExponentialBackoffPolicy(
            /*initial_delay=*/us(10), /*maximum_delay=*/us(20), /*scaling=*/2.0)
            .clone();
  }
  auto shutdown_manager = std::make_shared<SessionShutdownManager>();
  auto batch = std::make_shared<DefaultSubscriptionBatchSource>(
      executor, stub, std::move(p.full_subscription_name),
      std::move(retry_policy), std::move(backoff_policy));

  auto cq = executor;  // need a copy to make it mutable
  auto timer = [cq](std::chrono::system_clock::time_point) mutable {
    return cq.MakeRelativeTimer(std::chrono::milliseconds(50))
        .then([](future<StatusOr<std::chrono::system_clock::time_point>> f) {
          return f.get().status();
        });
  };
  auto lease_management = SubscriptionLeaseManagement::CreateForTesting(
      executor, shutdown_manager, timer, std::move(batch),
      p.options.max_deadline_time());

  return SubscriptionSessionImpl::Create(
      std::move(executor), std::move(shutdown_manager),
      std::move(lease_management), std::move(p));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

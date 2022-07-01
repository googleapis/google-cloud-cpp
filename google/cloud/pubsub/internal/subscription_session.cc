// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/internal/subscription_session.h"
#include "google/cloud/pubsub/ack_handler.h"
#include "google/cloud/pubsub/internal/exactly_once_ack_handler.h"
#include "google/cloud/pubsub/internal/streaming_subscription_batch_source.h"
#include "google/cloud/pubsub/internal/subscription_lease_management.h"
#include "google/cloud/pubsub/internal/subscription_message_queue.h"
#include "google/cloud/log.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class AckHandlerWrapper : public pubsub::AckHandler::Impl {
 public:
  explicit AckHandlerWrapper(
      std::unique_ptr<pubsub_internal::ExactlyOnceAckHandler::Impl> impl,
      std::string message_id)
      : impl_(std::move(impl)), message_id_(std::move(message_id)) {}
  ~AckHandlerWrapper() override = default;

  void ack() override {
    (void)impl_->ack().then([id = std::move(message_id_)](auto f) {
      auto status = f.get();
      GCP_LOG(WARNING) << "error while trying to ack(), status=" << status
                       << ", message_id=" << id;
    });
  }
  void nack() override {
    (void)impl_->nack().then([id = std::move(message_id_)](auto f) {
      auto status = f.get();
      GCP_LOG(WARNING) << "error while trying to nack(), status=" << status
                       << ", message_id=" << id;
    });
  }
  std::int32_t delivery_attempt() const override {
    return impl_->delivery_attempt();
  }

 private:
  std::unique_ptr<pubsub_internal::ExactlyOnceAckHandler::Impl> impl_;
  std::string message_id_;
};

class SubscriptionSessionImpl
    : public std::enable_shared_from_this<SubscriptionSessionImpl> {
 public:
  static future<Status> Create(
      Options const& opts, CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriptionBatchSource> source, Callback callback) {
    auto queue =
        SubscriptionMessageQueue::Create(shutdown_manager, std::move(source));
    auto concurrency_control = SubscriptionConcurrencyControl::Create(
        cq, shutdown_manager, std::move(queue),
        opts.get<pubsub::MaxConcurrencyOption>());

    auto self = std::make_shared<SubscriptionSessionImpl>(
        std::move(cq), std::move(shutdown_manager),
        std::move(concurrency_control),
        opts.get<pubsub::ShutdownPollingPeriodOption>());

    auto weak = std::weak_ptr<SubscriptionSessionImpl>(self);
    auto result = self->shutdown_manager_->Start(promise<Status>([weak] {
      if (auto self = weak.lock()) self->InitiateApplicationShutdown();
    }));
    // Create a (periodic) timer, this serves two purposes:
    // 1) Effectively the timer owns "self" and the timer is owned by the
    //    CompletionQueue.
    // 2) When the completion queue is shutdown, the timer is canceled and
    //    `self` gets a chance to shut down the pipeline.
    self->ScheduleTimer();
    self->pipeline_->Start(std::move(callback));
    return result.then([weak](future<Status> f) {
      if (auto self = weak.lock()) self->ShutdownCompleted();
      return f.get();
    });
  }

  static future<Status> Create(
      Options const& opts, CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriptionBatchSource> source,
      pubsub::ApplicationCallback application_callback) {
    return Create(
        opts, std::move(cq), std::move(shutdown_manager), std::move(source),
        [cb = std::move(application_callback)](
            pubsub::Message m, std::unique_ptr<ExactlyOnceAckHandler::Impl> h) {
          auto wrapper = absl::make_unique<AckHandlerWrapper>(std::move(h),
                                                              m.message_id());
          cb(std::move(m), pubsub::AckHandler(std::move(wrapper)));
        });
  }

  static future<Status> Create(
      Options const& opts, CompletionQueue cq,
      std::shared_ptr<SessionShutdownManager> shutdown_manager,
      std::shared_ptr<SubscriptionBatchSource> source,
      pubsub_internal::ExactlyOnceApplicationCallback application_callback) {
    return Create(
        opts, std::move(cq), std::move(shutdown_manager), std::move(source),
        [cb = std::move(application_callback)](
            pubsub::Message m, std::unique_ptr<ExactlyOnceAckHandler::Impl> h) {
          cb(std::move(m),
             pubsub_internal::ExactlyOnceAckHandler(std::move(h)));
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
    pubsub::Subscription const& subscription, Options const& opts,
    std::shared_ptr<SubscriberStub> const& stub, CompletionQueue const& cq,
    std::string client_id, pubsub::ApplicationCallback application_callback) {
  auto shutdown_manager = std::make_shared<SessionShutdownManager>();
  auto batch = std::make_shared<StreamingSubscriptionBatchSource>(
      cq, shutdown_manager, stub, subscription.FullName(), std::move(client_id),
      opts);
  auto lease_management = SubscriptionLeaseManagement::Create(
      cq, shutdown_manager, std::move(batch),
      opts.get<pubsub::MaxDeadlineTimeOption>(),
      opts.get<pubsub::MaxDeadlineExtensionOption>());

  return SubscriptionSessionImpl::Create(opts, cq, std::move(shutdown_manager),
                                         std::move(lease_management),
                                         std::move(application_callback));
}

future<Status> CreateSubscriptionSession(
    pubsub::Subscription const& subscription, Options const& opts,
    std::shared_ptr<SubscriberStub> const& stub, CompletionQueue const& cq,
    std::string client_id,
    pubsub_internal::ExactlyOnceApplicationCallback application_callback) {
  auto shutdown_manager = std::make_shared<SessionShutdownManager>();
  auto batch = std::make_shared<StreamingSubscriptionBatchSource>(
      cq, shutdown_manager, stub, subscription.FullName(), std::move(client_id),
      opts);
  auto lease_management = SubscriptionLeaseManagement::Create(
      cq, shutdown_manager, std::move(batch),
      opts.get<pubsub::MaxDeadlineTimeOption>(),
      opts.get<pubsub::MaxDeadlineExtensionOption>());

  return SubscriptionSessionImpl::Create(opts, cq, std::move(shutdown_manager),
                                         std::move(lease_management),
                                         std::move(application_callback));
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

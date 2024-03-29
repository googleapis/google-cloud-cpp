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

#include "google/cloud/pubsub/internal/subscription_concurrency_control.h"
#include "google/cloud/pubsub/exactly_once_ack_handler.h"
#include "google/cloud/pubsub/internal/default_batch_callback.h"
#include "google/cloud/pubsub/internal/message_callback_wrapper.h"
#include "google/cloud/pubsub/internal/tracing_batch_callback.h"
#include "google/cloud/pubsub/options.h"
#include "google/cloud/pubsub/subscription.h"
#include "google/cloud/log.h"
#include "google/cloud/opentelemetry_options.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

class AckHandlerImpl : public pubsub::ExactlyOnceAckHandler::Impl {
 public:
  explicit AckHandlerImpl(std::weak_ptr<SubscriptionConcurrencyControl> w,
                          std::string ack_id, pubsub::Subscription subscription,
                          std::int32_t delivery_attempt)
      : source_(std::move(w)),
        ack_id_(std::move(ack_id)),
        subscription_(std::move(subscription)),
        delivery_attempt_(delivery_attempt) {}
  ~AckHandlerImpl() override = default;

  future<Status> ack() override {
    if (auto s = source_.lock()) return s->AckMessage(ack_id_);
    return make_ready_future(
        Status(StatusCode::kFailedPrecondition, "session already shutdown"));
  }
  future<Status> nack() override {
    if (auto s = source_.lock()) return s->NackMessage(ack_id_);
    return make_ready_future(
        Status(StatusCode::kFailedPrecondition, "session already shutdown"));
  }
  std::int32_t delivery_attempt() const override { return delivery_attempt_; }
  std::string ack_id() override { return ack_id_; }
  pubsub::Subscription subscription() const override { return subscription_; }

 private:
  std::weak_ptr<SubscriptionConcurrencyControl> source_;
  std::string ack_id_;
  pubsub::Subscription subscription_;
  std::int32_t delivery_attempt_;
};

}  // namespace

void SubscriptionConcurrencyControl::Start(
    std::shared_ptr<MessageCallback> cb) {
  std::unique_lock<std::mutex> lk(mu_);
  if (callback_) return;

  auto message_callback = std::make_shared<MessageCallbackWrapper>(
      std::move(cb), [w = WeakFromThis()](MessageCallback::ReceivedMessage r) {
        if (auto self = w.lock()) self->OnMessage(std::move(r.message));
      });

  callback_ = std::make_shared<DefaultBatchCallback>(
      [](BatchCallback::StreamingPullResponse const&) {},
      std::move(message_callback));
  auto const& current = internal::CurrentOptions();
  if (current.get<OpenTelemetryTracingOption>()) {
    callback_ = MakeTracingBatchCallback(
        std::move(callback_), current.get<pubsub::SubscriptionOption>());
  }

  source_->Start(callback_);
  if (total_messages() >= max_concurrency_) return;
  auto const read_count = max_concurrency_ - total_messages();
  messages_requested_ = read_count;
  lk.unlock();
  source_->Read(read_count);
}

void SubscriptionConcurrencyControl::Shutdown() {
  shutdown_manager_->MarkAsShutdown(__func__, {});
  source_->Shutdown();
}

future<Status> SubscriptionConcurrencyControl::AckMessage(
    std::string const& ack_id) {
  auto r = source_->AckMessage(ack_id);
  MessageHandled();
  return r;
}

future<Status> SubscriptionConcurrencyControl::NackMessage(
    std::string const& ack_id) {
  auto r = source_->NackMessage(ack_id);
  MessageHandled();
  return r;
}

void SubscriptionConcurrencyControl::MessageHandled() {
  if (shutdown_manager_->FinishedOperation("handler")) return;
  std::unique_lock<std::mutex> lk(mu_);
  --message_count_;
  if (total_messages() < max_concurrency_) {
    auto const read_count = max_concurrency_ - total_messages();
    messages_requested_ += read_count;
    lk.unlock();
    source_->Read(read_count);
  }
}

void SubscriptionConcurrencyControl::OnMessage(
    google::pubsub::v1::ReceivedMessage m) {
  callback_->StartConcurrencyControl(m.ack_id());
  std::unique_lock<std::mutex> lk(mu_);
  if (messages_requested_ > 0) --messages_requested_;
  ++message_count_;
  lk.unlock();

  std::weak_ptr<SubscriptionConcurrencyControl> w = shared_from_this();
  shutdown_manager_->StartAsyncOperation(
      __func__, "callback", cq_, [m = std::move(m), w = std::move(w)] {
        if (auto s = w.lock()) s->OnMessageAsync(std::move(m), std::move(w));
      });
}

void SubscriptionConcurrencyControl::OnMessageAsync(
    google::pubsub::v1::ReceivedMessage m,
    std::weak_ptr<SubscriptionConcurrencyControl> w) {
  shutdown_manager_->StartOperation(__func__, "handler", [&] {
    callback_->EndConcurrencyControl(m.ack_id());
    auto h = std::make_unique<AckHandlerImpl>(
        std::move(w), std::move(*m.mutable_ack_id()), subscription_,
        m.delivery_attempt());
    callback_->user_callback(MessageCallback::MessageAndHandler{
        FromProto(std::move(*m.mutable_message())), std::move(h)});
  });
  shutdown_manager_->FinishedOperation("callback");
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/pubsub/internal/subscription_concurrency_control.h"
#include "google/cloud/pubsub/ack_handler.h"
#include "google/cloud/log.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {
class AckHandlerImpl : public pubsub::AckHandler::Impl {
 public:
  explicit AckHandlerImpl(
      std::shared_ptr<SubscriptionConcurrencyControl> const& source,
      std::string ack_id, std::int32_t delivery_attempt,
      std::size_t message_size)
      : source_(source),
        ack_id_(std::move(ack_id)),
        delivery_attempt_(delivery_attempt),
        message_size_(message_size) {}
  ~AckHandlerImpl() override = default;

  void ack() override {
    if (auto s = source_.lock()) s->AckMessage(ack_id_, message_size_);
  }
  void nack() override {
    if (auto s = source_.lock()) s->NackMessage(ack_id_, message_size_);
  }
  std::string ack_id() const override { return ack_id_; }
  std::int32_t delivery_attempt() const override { return delivery_attempt_; }

 private:
  std::weak_ptr<SubscriptionConcurrencyControl> source_;
  std::string ack_id_;
  std::int32_t delivery_attempt_;
  std::size_t message_size_;
};

}  // namespace

void SubscriptionConcurrencyControl::Start(pubsub::ApplicationCallback cb) {
  std::unique_lock<std::mutex> lk(mu_);
  if (callback_) return;
  callback_ = std::move(cb);
  std::weak_ptr<SubscriptionConcurrencyControl> weak = shared_from_this();
  source_->Start([weak](google::pubsub::v1::ReceivedMessage r) {
    if (auto self = weak.lock()) self->OnMessage(std::move(r));
  });
  if (total_messages() >= message_count_hwm_) return;
  auto const read_count = message_count_hwm_ - total_messages();
  messages_requested_ = read_count;
  if (total_messages() >= message_count_hwm_) overflow_ = true;
  lk.unlock();
  source_->Read(read_count);
}

void SubscriptionConcurrencyControl::Shutdown() {
  shutdown_manager_->MarkAsShutdown(__func__, {});
  source_->Shutdown();
}

void SubscriptionConcurrencyControl::AckMessage(std::string const& ack_id,
                                                std::size_t size) {
  source_->AckMessage(ack_id, size);
  MessageHandled();
}

void SubscriptionConcurrencyControl::NackMessage(std::string const& ack_id,
                                                 std::size_t size) {
  source_->NackMessage(ack_id, size);
  MessageHandled();
}

void SubscriptionConcurrencyControl::MessageHandled() {
  if (shutdown_manager_->FinishedOperation("handler")) return;
  std::unique_lock<std::mutex> lk(mu_);
  --message_count_;
  if (total_messages() <= message_count_lwm_) {
    auto const read_count = message_count_hwm_ - total_messages();
    messages_requested_ += read_count;
    lk.unlock();
    source_->Read(read_count);
  }
}

void SubscriptionConcurrencyControl::OnMessage(
    google::pubsub::v1::ReceivedMessage m) {
  std::unique_lock<std::mutex> lk(mu_);
  if (messages_requested_ > 0) --messages_requested_;
  ++message_count_;
  if (total_messages() >= message_count_hwm_) overflow_ = true;
  lk.unlock();

  struct MoveCapture {
    std::shared_ptr<SessionShutdownManager> shutdown_manager;
    pubsub::ApplicationCallback callback;
    pubsub::Message m;
    std::unique_ptr<AckHandlerImpl> h;
    void operator()() {
      shutdown_manager->StartOperation("OnMessage/callback", "handler", [&] {
        callback(std::move(m), pubsub::AckHandler(std::move(h)));
      });
      shutdown_manager->FinishedOperation("callback");
    }
  };
  auto handler = absl::make_unique<AckHandlerImpl>(
      shared_from_this(), std::move(*m.mutable_ack_id()), m.delivery_attempt(),
      MessageProtoSize(m.message()));
  auto message = FromProto(std::move(*m.mutable_message()));
  shutdown_manager_->StartAsyncOperation(
      __func__, "callback", cq_,
      MoveCapture{shutdown_manager_, callback_, std::move(message),
                  std::move(handler)});
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

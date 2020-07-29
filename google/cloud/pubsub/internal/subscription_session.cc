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
#include "google/cloud/pubsub/internal/default_ack_handler_impl.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class NotifyWhenMessageHandled : public pubsub::AckHandler::Impl {
 public:
  explicit NotifyWhenMessageHandled(
      std::weak_ptr<SubscriptionSession> session,
      std::unique_ptr<pubsub::AckHandler::Impl> child, std::size_t message_size)
      : session_(std::move(session)),
        child_(std::move(child)),
        message_size_(message_size) {}
  ~NotifyWhenMessageHandled() override = default;

  void ack() override {
    child_->ack();
    NotifySession();
  }
  void nack() override {
    child_->nack();
    NotifySession();
  }
  std::string ack_id() const override { return child_->ack_id(); }

 private:
  void NotifySession() {
    auto session = session_.lock();
    if (session) session->MessageHandled(message_size_);
  }

  std::weak_ptr<SubscriptionSession> session_;
  std::unique_ptr<pubsub::AckHandler::Impl> child_;
  std::size_t message_size_;
};

future<Status> SubscriptionSession::Start() {
  auto self = shared_from_this();
  std::weak_ptr<SubscriptionSession> w(self);
  result_ = promise<Status>{[w] {
    auto self = w.lock();
    if (!self) return;
    self->Cancel();
  }};
  executor_.RunAsync([self] { self->PullOne(); });
  return result_.get_future();
}

void SubscriptionSession::MessageHandled(std::size_t) {
  auto self = shared_from_this();
  // After the callback re-schedule ourselves.
  // TODO(#4652) - support parallel scheduling of callbacks
  executor_.RunAsync([self] { self->HandleQueue(); });
}

void SubscriptionSession::Cancel() {
  std::lock_guard<std::mutex> lk(mu_);
  shutdown_ = true;
}

void SubscriptionSession::PullOne() {
  std::unique_lock<std::mutex> lk(mu_);
  if (!messages_.empty()) {
    HandleQueue(std::move(lk));
    return;
  }

  if (shutdown_) {
    result_.set_value(Status{});
    return;
  }

  auto self = shared_from_this();
  lk.unlock();

  auto context = absl::make_unique<grpc::ClientContext>();
  google::pubsub::v1::PullRequest request;
  request.set_subscription(params_.full_subscription_name);
  request.set_max_messages(1);
  stub_->AsyncPull(executor_, std::move(context), request)
      .then([self](future<StatusOr<google::pubsub::v1::PullResponse>> f) {
        self->OnPull(f.get());
      });
}

void SubscriptionSession::OnPull(StatusOr<google::pubsub::v1::PullResponse> r) {
  if (!r) {
    result_.set_value(std::move(r).status());
    return;
  }

  std::unique_lock<std::mutex> lk(mu_);
  std::move(r->mutable_received_messages()->begin(),
            r->mutable_received_messages()->end(),
            std::back_inserter(messages_));
  lk.unlock();

  auto self = shared_from_this();
  executor_.RunAsync([self] { self->HandleQueue(); });
}

void SubscriptionSession::HandleQueue() {
  HandleQueue(std::unique_lock<std::mutex>(mu_));
}

void SubscriptionSession::HandleQueue(std::unique_lock<std::mutex> lk) {
  auto self = shared_from_this();
  if (messages_.empty()) {
    executor_.RunAsync([self] { self->PullOne(); });
    return;
  }
  auto m = std::move(messages_.front());
  messages_.pop_front();
  lk.unlock();

  std::unique_ptr<pubsub::AckHandler::Impl> handler =
      absl::make_unique<DefaultAckHandlerImpl>(executor_, stub_,
                                               params_.full_subscription_name,
                                               std::move(*m.mutable_ack_id()));
  // TODO(#...) - use a better estimation for the message size.
  auto const message_size = m.message().data().size();
  handler = absl::make_unique<NotifyWhenMessageHandled>(
      self, std::move(handler), message_size);
  params_.callback(FromProto(std::move(*m.mutable_message())),
                   pubsub::AckHandler(std::move(handler)));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

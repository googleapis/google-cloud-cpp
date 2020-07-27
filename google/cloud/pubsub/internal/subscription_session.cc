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
    auto id = ack_id();
    child_->ack();
    NotifySession(id);
  }
  void nack() override {
    auto id = ack_id();
    child_->nack();
    NotifySession(id);
  }
  std::string ack_id() const override { return child_->ack_id(); }

 private:
  void NotifySession(std::string const& id) {
    auto session = session_.lock();
    if (session) session->MessageHandled(id, message_size_);
  }

  std::weak_ptr<SubscriptionSession> session_;
  std::unique_ptr<pubsub::AckHandler::Impl> child_;
  std::size_t message_size_;
};

int constexpr SubscriptionSession::kMinimumAckDeadlineSeconds;
int constexpr SubscriptionSession::kMaximumAckDeadlineSeconds;
int constexpr SubscriptionSession::kAckDeadlineSlackSeconds;

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

void SubscriptionSession::MessageHandled(std::string const& ack_id,
                                         std::size_t) {
  std::unique_lock<std::mutex> lk(mu_);
  ack_deadlines_.erase(ack_id);
  auto self = shared_from_this();
  // After the callback re-schedule ourselves.
  // TODO(#4652) - support parallel scheduling of callbacks
  executor_.RunAsync([self] { self->HandleQueue(); });
}

void SubscriptionSession::Cancel() {
  std::lock_guard<std::mutex> lk(mu_);
  shutdown_ = true;
  if (refresh_timer_.valid()) refresh_timer_.cancel();
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
    // TODO(#4699) - retry on transient failures
    result_.set_value(std::move(r).status());
    return;
  }

  std::unique_lock<std::mutex> lk(mu_);
  auto const now = std::chrono::system_clock::now();
  auto const estimated_server_deadline = now + std::chrono::seconds(10);
  auto const handling_deadline = now + params_.options.max_deadline_time();
  for (auto const& rm : r->received_messages()) {
    ack_deadlines_.emplace(
        rm.ack_id(), AckStatus{estimated_server_deadline, handling_deadline});
  }
  std::move(r->mutable_received_messages()->begin(),
            r->mutable_received_messages()->end(),
            std::back_inserter(messages_));
  if (refresh_timer_.valid()) {
    refresh_timer_.cancel();
    refresh_timer_ = {};
  }
  auto self = shared_from_this();
  executor_.RunAsync([self] { self->HandleQueue(); });

  StartRefreshAckDeadlinesTimer(
      std::move(lk), std::chrono::system_clock::now() +
                         std::chrono::seconds(kMinimumAckDeadlineSeconds));
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
  // TODO(#4645) - use a better estimation for the message size.
  auto const message_size = m.message().data().size();
  handler = absl::make_unique<NotifyWhenMessageHandled>(
      self, std::move(handler), message_size);
  params_.callback(FromProto(std::move(*m.mutable_message())),
                   pubsub::AckHandler(std::move(handler)));
}

void SubscriptionSession::RefreshAckDeadlines(std::unique_lock<std::mutex> lk) {
  if (ack_deadlines_.empty() || refreshing_deadlines_) return;
  google::pubsub::v1::ModifyAckDeadlineRequest request;
  auto const now = std::chrono::system_clock::now();
  request.set_subscription(params_.full_subscription_name);
  using seconds = std::chrono::seconds;
  auto extension = seconds(kMaximumAckDeadlineSeconds);
  for (auto const& kv : ack_deadlines_) {
    // This message cannot be extended any further, and we do not want to send
    // an extension of 0 seconds because that is a nack.
    if (kv.second.handling_deadline < now + seconds(1)) continue;
    auto const message_extension =
        std::chrono::duration_cast<seconds>(kv.second.handling_deadline - now);
    extension = (std::min)(extension, message_extension);
    request.add_ack_ids(kv.first);
  }
  auto const new_deadline = now + extension;
  if (request.ack_ids().empty()) {
    StartRefreshAckDeadlinesTimer(std::move(lk), new_deadline);
    return;
  }
  request.set_ack_deadline_seconds(
      static_cast<std::int32_t>(extension.count()));
  refreshing_deadlines_ = true;
  auto context = absl::make_unique<grpc::ClientContext>();
  auto weak = std::weak_ptr<SubscriptionSession>(shared_from_this());
  lk.unlock();
  stub_->AsyncModifyAckDeadline(executor_, std::move(context), request)
      .then([weak, request, new_deadline](future<Status>) {
        auto self = weak.lock();
        if (!self) return;
        self->OnRefreshAckDeadlines(request, new_deadline);
      });
}

void SubscriptionSession::OnRefreshAckDeadlines(
    google::pubsub::v1::ModifyAckDeadlineRequest const& request,
    std::chrono::system_clock::time_point new_server_deadline) {
  std::unique_lock<std::mutex> lk(mu_);
  for (auto const& ack : request.ack_ids()) {
    auto i = ack_deadlines_.find(ack);
    if (i == ack_deadlines_.end()) continue;
    i->second.estimated_server_deadline = new_server_deadline;
  }
  StartRefreshAckDeadlinesTimer(std::move(lk), new_server_deadline);
}

void SubscriptionSession::StartRefreshAckDeadlinesTimer(
    std::unique_lock<std::mutex>,
    std::chrono::system_clock::time_point new_server_deadline) {
  if (shutdown_) return;
  auto weak = std::weak_ptr<SubscriptionSession>(shared_from_this());
  auto deadline =
      new_server_deadline - std::chrono::seconds(kAckDeadlineSlackSeconds);
  if (test_refresh_period_.count() != 0) {
    deadline = std::chrono::system_clock::now() + test_refresh_period_;
  }
  refresh_timer_ = executor_.MakeDeadlineTimer(deadline).then(
      [weak](future<StatusOr<std::chrono::system_clock::time_point>> f) {
        auto self = weak.lock();
        if (!self || !f.get()) return;
        self->OnRefreshAckDeadlinesTimer();
      });
}

void SubscriptionSession::OnRefreshAckDeadlinesTimer() {
  std::unique_lock<std::mutex> lk(mu_);
  refreshing_deadlines_ = false;
  refresh_timer_ = {};
  RefreshAckDeadlines(std::move(lk));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

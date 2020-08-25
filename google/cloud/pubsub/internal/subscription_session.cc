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
#include "google/cloud/log.h"
#include "absl/memory/memory.h"

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class NotifyWhenMessageHandled : public pubsub::AckHandler::Impl {
 public:
  explicit NotifyWhenMessageHandled(
      std::shared_ptr<SubscriptionSession> session,
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
  std::int32_t delivery_attempt() const override {
    return child_->delivery_attempt();
  }

 private:
  void NotifySession(std::string const& id) {
    session_->MessageHandled(id, message_size_);
    session_.reset();
  }

  std::shared_ptr<SubscriptionSession> session_;
  std::unique_ptr<pubsub::AckHandler::Impl> child_;
  std::size_t message_size_;
  std::string id_;
};

int constexpr SubscriptionSession::kMinimumAckDeadlineSeconds;
int constexpr SubscriptionSession::kMaximumAckDeadlineSeconds;
int constexpr SubscriptionSession::kAckDeadlineSlackSeconds;

future<Status> SubscriptionSession::Start() {
  // Transfer the result from shutdown_manager_'s future to a new future that
  // has a proper cancellation callback.
  class MoveCapture {
   public:
    explicit MoveCapture(std::weak_ptr<SubscriptionSession> const& w)
        : cancel_([w] {
            auto self = w.lock();
            if (!self) return;
            self->Cancel();
          }) {}

    void operator()(future<Status> f) { cancel_.set_value(f.get()); }
    future<Status> get_future() { return cancel_.get_future(); }

   private:
    promise<Status> cancel_;
  };

  auto self = shared_from_this();
  MoveCapture chain(self);
  auto f = chain.get_future();
  shutdown_manager_.Start().then(std::move(chain));
  shutdown_manager_.StartAsyncOperation(__func__, "PullOne", executor_,
                                        [self] { self->PullOne(); });
  return f;
}

void SubscriptionSession::MessageHandled(std::string const& ack_id,
                                         std::size_t) {
  if (shutdown_manager_.FinishedOperation("callback")) return;
  std::unique_lock<std::mutex> lk(mu_);
  leases_.erase(ack_id);
  if (!callback_flow_control_.Release()) return;
  auto self = shared_from_this();
  // After the callback re-schedule ourselves.
  shutdown_manager_.StartAsyncOperation(__func__, "HandleQueue", executor_,
                                        [self] { self->HandleQueue(); });
}

void SubscriptionSession::Cancel() {
  // Stop any more work from being created as soon as possible.
  shutdown_manager_.MarkAsShutdown(__func__, {});
  std::unique_lock<std::mutex> lk(mu_);
  // Cancel any existing timers.
  if (refresh_timer_.valid()) refresh_timer_.cancel();
  // Schedule a nack for each pending message, using the existing executor.
  NackAll(lk, messages_.begin(), messages_.end());
}

void SubscriptionSession::PullOne() {
  std::unique_lock<std::mutex> lk(mu_);
  auto self = shared_from_this();
  if (shutdown_manager_.FinishedOperation("PullOne")) return;
  if (!messages_.empty()) {
    shutdown_manager_.StartAsyncOperation(__func__, "HandleQueue", executor_,
                                          [self] { self->HandleQueue(); });
    return;
  }

  lk.unlock();

  auto context = absl::make_unique<grpc::ClientContext>();
  google::pubsub::v1::PullRequest request;
  request.set_subscription(params_.full_subscription_name);
  request.set_max_messages(1);
  shutdown_manager_.StartOperation(__func__, "OnPull", [&] {
    stub_->AsyncPull(executor_, std::move(context), request)
        .then([self](future<StatusOr<google::pubsub::v1::PullResponse>> f) {
          self->OnPull(f.get());
        });
  });
}

void SubscriptionSession::OnPull(StatusOr<google::pubsub::v1::PullResponse> r) {
  if (!r) {
    shutdown_manager_.MarkAsShutdown(__func__, std::move(r).status());
    return;
  }
  std::unique_lock<std::mutex> lk(mu_);
  if (shutdown_manager_.FinishedOperation("OnPull")) {
    NackAll(lk, r->received_messages().begin(), r->received_messages().end());
    return;
  }

  auto const now = std::chrono::system_clock::now();
  auto const estimated_server_deadline = now + std::chrono::seconds(10);
  auto const handling_deadline = now + params_.options.max_deadline_time();
  for (auto const& rm : r->received_messages()) {
    leases_.emplace(rm.ack_id(),
                    LeaseStatus{estimated_server_deadline, handling_deadline});
  }
  std::move(r->mutable_received_messages()->begin(),
            r->mutable_received_messages()->end(),
            std::back_inserter(messages_));
  auto self = shared_from_this();
  shutdown_manager_.StartAsyncOperation(__func__, "HandleQueue", executor_,
                                        [self] { self->HandleQueue(); });

  // Setup a timer to refresh the message leases. We do not want to immediately
  // refresh them because there is a good chance they will be handled before
  // the minimum lease time, and it seems wasteful to refresh the lease just to
  // quickly turnaround and ack or nack the message.
  StartRefreshTimer(std::move(lk),
                    std::chrono::system_clock::now() +
                        std::chrono::seconds(kMinimumAckDeadlineSeconds));
}

void SubscriptionSession::HandleQueue(std::unique_lock<std::mutex> lk) {
  auto self = shared_from_this();
  if (shutdown_manager_.FinishedOperation("HandleQueue")) {
    // We do not wait for this to complete before completing the session
    // shutdown, NackAll() does not need to callback or otherwise inform the
    // session of any state changes.
    NackAll(lk, messages_.begin(), messages_.end());
    return;
  }
  if (messages_.empty()) {
    shutdown_manager_.StartAsyncOperation(__func__, "PullOne", executor_,
                                          [self] { self->PullOne(); });
    return;
  }

  while (!messages_.empty() && callback_flow_control_.MaybeAdmit()) {
    auto m = std::move(messages_.front());
    messages_.pop_front();

    struct MoveCapture {
      std::function<void(pubsub::Message, pubsub::AckHandler)> callback;
      pubsub::Message m;
      std::unique_ptr<pubsub::AckHandler::Impl> h;
      void operator()() {
        GCP_LOG(DEBUG) << "calling callback(" << h->ack_id() << ")";
        callback(std::move(m), pubsub::AckHandler(std::move(h)));
      }
    };
    auto message = FromProto(std::move(*m.mutable_message()));
    auto const message_size = MessageSize(message);
    std::unique_ptr<pubsub::AckHandler::Impl> handler =
        absl::make_unique<DefaultAckHandlerImpl>(
            executor_, stub_, params_.full_subscription_name,
            std::move(*m.mutable_ack_id()), m.delivery_attempt());
    handler = absl::make_unique<NotifyWhenMessageHandled>(
        self, std::move(handler), message_size);

    shutdown_manager_.StartAsyncOperation(
        __func__, "callback", executor_,
        MoveCapture{params_.callback, std::move(message), std::move(handler)});
  }
}

void SubscriptionSession::RefreshMessageLeases(
    std::unique_lock<std::mutex> lk) {
  if (leases_.empty() || refreshing_leases_) return;

  google::pubsub::v1::ModifyAckDeadlineRequest request;
  auto const now = std::chrono::system_clock::now();
  request.set_subscription(params_.full_subscription_name);
  using seconds = std::chrono::seconds;
  auto extension = seconds(kMaximumAckDeadlineSeconds);
  for (auto const& kv : leases_) {
    // This message lease cannot be extended any further, and we do not want to
    // send an extension of 0 seconds because that is a nack.
    if (kv.second.handling_deadline < now + seconds(1)) continue;
    auto const message_extension =
        std::chrono::duration_cast<seconds>(kv.second.handling_deadline - now);
    extension = (std::min)(extension, message_extension);
    request.add_ack_ids(kv.first);
  }
  auto const new_deadline = now + extension;
  if (request.ack_ids().empty()) {
    StartRefreshTimer(std::move(lk), new_deadline);
    return;
  }
  request.set_ack_deadline_seconds(
      static_cast<std::int32_t>(extension.count()));
  refreshing_leases_ = true;
  auto context = absl::make_unique<grpc::ClientContext>();
  auto weak = std::weak_ptr<SubscriptionSession>(shared_from_this());
  lk.unlock();

  shutdown_manager_.StartOperation(__func__, "OnRefreshMessageLeases", [&] {
    stub_->AsyncModifyAckDeadline(executor_, std::move(context), request)
        .then([weak, request, new_deadline](future<Status>) {
          auto self = weak.lock();
          if (!self) return;
          self->OnRefreshMessageLeases(request, new_deadline);
        });
  });
}

void SubscriptionSession::OnRefreshMessageLeases(
    google::pubsub::v1::ModifyAckDeadlineRequest const& request,
    std::chrono::system_clock::time_point new_server_deadline) {
  if (shutdown_manager_.FinishedOperation(__func__)) return;
  std::unique_lock<std::mutex> lk(mu_);
  refreshing_leases_ = false;
  for (auto const& ack : request.ack_ids()) {
    auto i = leases_.find(ack);
    if (i == leases_.end()) continue;
    i->second.estimated_server_deadline = new_server_deadline;
  }
  StartRefreshTimer(std::move(lk), new_server_deadline);
}

void SubscriptionSession::StartRefreshTimer(
    std::unique_lock<std::mutex>,
    std::chrono::system_clock::time_point new_server_deadline) {
  auto weak = std::weak_ptr<SubscriptionSession>(shared_from_this());
  auto deadline =
      new_server_deadline - std::chrono::seconds(kAckDeadlineSlackSeconds);
  if (test_refresh_period_.count() != 0) {
    deadline = std::chrono::system_clock::now() + test_refresh_period_;
  }

  shutdown_manager_.StartOperation(__func__, "OnRefreshTimer", [&] {
    if (refresh_timer_.valid()) refresh_timer_.cancel();
    refresh_timer_ = executor_.MakeDeadlineTimer(deadline).then(
        [weak](future<StatusOr<std::chrono::system_clock::time_point>> f) {
          auto self = weak.lock();
          if (!self) return;
          self->OnRefreshTimer(!f.get().ok());
        });
  });
}

void SubscriptionSession::OnRefreshTimer(bool cancelled) {
  if (shutdown_manager_.FinishedOperation(__func__)) return;
  if (cancelled) return;
  RefreshMessageLeases(std::unique_lock<std::mutex>(mu_));
}

void SubscriptionSession::NackAll(
    std::unique_lock<std::mutex> const&,
    google::pubsub::v1::ModifyAckDeadlineRequest request) {
  if (request.ack_ids().empty()) return;
  request.set_ack_deadline_seconds(0);
  request.set_subscription(params_.full_subscription_name);

  // This is a fire-and-forget asynchronous operation. If it fails we will
  // simply let the normal timeouts expire any messages.
  (void)stub_->AsyncModifyAckDeadline(
      executor_, absl::make_unique<grpc::ClientContext>(), request);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

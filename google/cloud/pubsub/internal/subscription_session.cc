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
};

int constexpr SubscriptionSession::kMinimumAckDeadlineSeconds;
int constexpr SubscriptionSession::kMaximumAckDeadlineSeconds;
int constexpr SubscriptionSession::kAckDeadlineSlackSeconds;

SessionShutdownManager::~SessionShutdownManager() {
  if (signaled_) return;
  signaled_ = true;
  done_.set_value(std::move(result_));
}

void SessionShutdownManager::LogStart(const char* caller, const char* name) {
  GCP_LOG(DEBUG) << "operation <" << name << "> starting from " << caller
                 << ", shutdown=" << shutdown_ << ", signaled=" << signaled_
                 << ", outstanding_operations=" << outstanding_operations_
                 << ", result=" << result_;
}

bool SessionShutdownManager::FinishedOperation(char const* name) {
  std::unique_lock<std::mutex> lk(mu_);
  GCP_LOG(DEBUG) << "operation <" << name << "> finished"
                 << ", shutdown=" << shutdown_ << ", signaled=" << signaled_
                 << ", outstanding_operations=" << outstanding_operations_
                 << ", result=" << result_;
  bool r = shutdown_;
  --outstanding_operations_;
  SignalOnShutdown(std::move(lk));
  return r;
}

void SessionShutdownManager::MarkAsShutdown(char const* caller, Status status) {
  std::unique_lock<std::mutex> lk(mu_);
  GCP_LOG(DEBUG) << caller << "() - shutting down"
                 << ", shutdown=" << shutdown_ << ", signaled=" << signaled_
                 << ", outstanding_operations=" << outstanding_operations_
                 << ", result=" << result_;
  shutdown_ = true;
  result_ = std::move(status);
  SignalOnShutdown(std::move(lk));
}

void SessionShutdownManager::SignalOnShutdown(std::unique_lock<std::mutex> lk) {
  GCP_LOG(DEBUG) << __func__ << "() - maybe signal"
                 << ", shutdown=" << shutdown_ << ", signaled=" << signaled_
                 << ", outstanding_operations=" << outstanding_operations_
                 << ", result=" << result_;
  if (!shutdown_ || signaled_) return;
  signaled_ = true;
  lk.unlock();
  done_.set_value(std::move(result_));
}

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
  ack_deadlines_.erase(ack_id);
  auto self = shared_from_this();
  // After the callback re-schedule ourselves.
  shutdown_manager_.StartAsyncOperation(__func__, "HandleQueue", executor_,
                                        [self] { self->HandleQueue(); });
}

void SubscriptionSession::Cancel() {
  std::unique_lock<std::mutex> lk(mu_);
  if (refresh_timer_.valid()) refresh_timer_.cancel();
  // Schedule a nack for each pending message, using the existing executor.
  NackAll(lk, messages_.begin(), messages_.end());
  shutdown_manager_.MarkAsShutdown(__func__, {});
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
  shutdown_manager_.StartAsyncOperation(__func__, "HandleQueue", executor_,
                                        [self] { self->HandleQueue(); });

  StartRefreshAckDeadlinesTimer(
      std::move(lk), std::chrono::system_clock::now() +
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

  auto m = std::move(messages_.front());
  messages_.pop_front();

  struct MoveCapture {
    std::function<void(pubsub::Message, pubsub::AckHandler)> callback;
    pubsub::Message m;
    std::unique_ptr<pubsub::AckHandler::Impl> h;
    void operator()() {
      callback(std::move(m), pubsub::AckHandler(std::move(h)));
    }
  };
  auto const message_size = MessageSize(m.message());
  auto message = FromProto(std::move(*m.mutable_message()));
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

  shutdown_manager_.StartOperation(__func__, "OnRefreshAckDeadlines", [&] {
    stub_->AsyncModifyAckDeadline(executor_, std::move(context), request)
        .then([weak, request, new_deadline](future<Status>) {
          auto self = weak.lock();
          if (!self) return;
          self->OnRefreshAckDeadlines(request, new_deadline);
        });
  });
}

void SubscriptionSession::OnRefreshAckDeadlines(
    google::pubsub::v1::ModifyAckDeadlineRequest const& request,
    std::chrono::system_clock::time_point new_server_deadline) {
  if (shutdown_manager_.FinishedOperation(__func__)) return;
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
  auto weak = std::weak_ptr<SubscriptionSession>(shared_from_this());
  auto deadline =
      new_server_deadline - std::chrono::seconds(kAckDeadlineSlackSeconds);
  if (test_refresh_period_.count() != 0) {
    deadline = std::chrono::system_clock::now() + test_refresh_period_;
  }

  shutdown_manager_.StartOperation(__func__, "OnRefreshAckDeadlinesTimer", [&] {
    refresh_timer_ = executor_.MakeDeadlineTimer(deadline).then(
        [weak](future<StatusOr<std::chrono::system_clock::time_point>> f) {
          auto self = weak.lock();
          if (!self) return;
          self->OnRefreshAckDeadlinesTimer(f.get().ok());
        });
  });
}

void SubscriptionSession::OnRefreshAckDeadlinesTimer(bool restart) {
  std::unique_lock<std::mutex> lk(mu_);
  refreshing_deadlines_ = false;
  refresh_timer_ = {};
  if (shutdown_manager_.FinishedOperation(__func__)) return;
  if (!restart) return;
  RefreshAckDeadlines(std::move(lk));
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

std::size_t SubscriptionSession::MessageSize(
    google::pubsub::v1::PubsubMessage const& m) {
  // TODO(#4645) - use a better estimation for the message size.
  std::size_t s = m.data().size();
  for (auto const& kv : m.attributes()) {
    s += kv.first.size() + kv.second.size();
  }
  return s;
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/pubsub/subscriber_connection.h"
#include "google/cloud/pubsub/internal/default_ack_handler_impl.h"
#include "absl/memory/memory.h"
#include <algorithm>
#include <deque>
#include <memory>
#include <mutex>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

SubscriberConnection::~SubscriberConnection() = default;

std::shared_ptr<SubscriberConnection> MakeSubscriberConnection(
    ConnectionOptions const& options) {
  auto stub =
      pubsub_internal::CreateDefaultSubscriberStub(options, /*channel_id=*/0);
  return pubsub_internal::MakeSubscriberConnection(std::move(stub), options);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class SubscriptionSession
    : public std::enable_shared_from_this<SubscriptionSession> {
 public:
  SubscriptionSession(std::shared_ptr<pubsub_internal::SubscriberStub> s,
                      google::cloud::CompletionQueue executor,
                      pubsub::SubscriberConnection::SubscribeParams p)
      : stub_(std::move(s)),
        executor_(std::move(executor)),
        params_(std::move(p)) {}

  future<Status> Start() {
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

 private:
  void Cancel() {
    std::lock_guard<std::mutex> lk(mu_);
    shutdown_ = true;
  }

  void PullOne() {
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

  void OnPull(StatusOr<google::pubsub::v1::PullResponse> r) {
    auto self = shared_from_this();
    if (!r) {
      result_.set_value(std::move(r).status());
      return;
    }

    std::unique_lock<std::mutex> lk(mu_);
    std::move(r->mutable_received_messages()->begin(),
              r->mutable_received_messages()->end(),
              std::back_inserter(messages_));
    lk.unlock();

    executor_.RunAsync([self] { self->HandleQueue(); });
  }

  void HandleQueue() { HandleQueue(std::unique_lock<std::mutex>(mu_)); }

  void HandleQueue(std::unique_lock<std::mutex> lk) {
    auto self = shared_from_this();
    if (messages_.empty()) {
      executor_.RunAsync([self] { self->PullOne(); });
      return;
    }
    auto m = std::move(messages_.front());
    messages_.pop_front();
    lk.unlock();

    auto handler_impl = absl::make_unique<DefaultAckHandlerImpl>(
        stub_, params_.full_subscription_name, std::move(*m.mutable_ack_id()));
    params_.callback(FromProto(std::move(*m.mutable_message())),
                     pubsub::AckHandler(std::move(handler_impl)));

    // After the callback re-schedule ourselves.
    // TODO(#4652) - support parallel scheduling of callbacks
    executor_.RunAsync([self] { self->HandleQueue(); });
  }

  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
  google::cloud::CompletionQueue executor_;
  pubsub::SubscriberConnection::SubscribeParams params_;
  std::mutex mu_;
  bool shutdown_ = false;
  std::deque<google::pubsub::v1::ReceivedMessage> messages_;
  promise<Status> result_;
};

namespace {
class SubscriberConnectionImpl : public pubsub::SubscriberConnection {
 public:
  explicit SubscriberConnectionImpl(
      std::shared_ptr<pubsub_internal::SubscriberStub> stub,
      pubsub::ConnectionOptions const& options)
      : stub_(std::move(stub)),
        background_(options.background_threads_factory()()) {}

  ~SubscriberConnectionImpl() override = default;

  future<Status> Subscribe(SubscribeParams p) override {
    auto session = std::make_shared<SubscriptionSession>(
        stub_, background_->cq(), std::move(p));
    return session->Start();
  }

 private:
  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
  std::shared_ptr<BackgroundThreads> background_;
};
}  // namespace

std::shared_ptr<pubsub::SubscriberConnection> MakeSubscriberConnection(
    std::shared_ptr<SubscriberStub> stub,
    pubsub::ConnectionOptions const& options) {
  return std::make_shared<SubscriberConnectionImpl>(std::move(stub), options);
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

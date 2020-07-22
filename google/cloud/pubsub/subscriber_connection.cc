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
#include "absl/memory/memory.h"
#include <memory>
#include <mutex>
#include <thread>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

SubscriberConnection::~SubscriberConnection() = default;

std::shared_ptr<SubscriberConnection> MakeSubscriberConnection(
    ConnectionOptions const& options) {
  auto stub =
      pubsub_internal::CreateDefaultSubscriberStub(options, /*channel_id=*/0);
  return pubsub_internal::MakeSubscriberConnection(std::move(stub));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub

namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class AckHandlerImpl : public pubsub::AckHandler::Impl {
 public:
  AckHandlerImpl(std::shared_ptr<pubsub_internal::SubscriberStub> s,
                 std::string subsription, std::string ack_id)
      : stub_(std::move(s)),
        subscription_(std::move(subsription)),
        ack_id_(std::move(ack_id)) {}

  ~AckHandlerImpl() override = default;

  void ack() override {
    grpc::ClientContext context;
    google::pubsub::v1::AcknowledgeRequest request;
    request.set_subscription(std::move(subscription_));
    request.add_ack_ids(std::move(ack_id_));
    (void)stub_->Acknowledge(context, request);
  }
  void nack() override {
    // TODO(#4553) - implement nacks
  }

  std::string ack_id() const override { return ack_id_; }

 private:
  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
  std::string subscription_;
  std::string ack_id_;
};

class SubscriptionSession
    : public std::enable_shared_from_this<SubscriptionSession> {
 public:
  SubscriptionSession(std::shared_ptr<pubsub_internal::SubscriberStub> s,
                      pubsub::SubscriberConnection::SubscribeParams p)
      : stub_(std::move(s)), params_(std::move(p)) {}

  std::pair<std::thread, future<Status>> Start() {
    auto self = shared_from_this();
    std::weak_ptr<SubscriptionSession> w(self);
    result_ = promise<Status>{[w] {
      auto self = w.lock();
      if (!self) return;
      self->Cancel();
    }};
    // TODO(#4554) - use the completion queue and asynchronous requests
    return {std::thread([self] { self->Run(); }), result_.get_future()};
  }

  void Cancel() {
    std::lock_guard<std::mutex> lk(mu_);
    shutdown_ = true;
  }
  void Run() {
    for (;;) {
      {
        std::lock_guard<std::mutex> lk(mu_);
        if (shutdown_) {
          result_.set_value(Status{});
          return;
        }
      }
      grpc::ClientContext context;
      context.set_deadline(std::chrono::system_clock::now() +
                           std::chrono::milliseconds(500));
      google::pubsub::v1::PullRequest request;
      request.set_subscription(params_.full_subscription_name);
      request.set_max_messages(1);
      auto r = stub_->Pull(context, request);
      // For now we poll every 500ms to terminate this loop gracefully
      if (r.status().code() == StatusCode::kDeadlineExceeded) continue;
      if (!r) {
        result_.set_value(std::move(r).status());
        return;
      }
      for (auto m : *r->mutable_received_messages()) {
        auto handler = absl::make_unique<AckHandlerImpl>(
            stub_, params_.full_subscription_name,
            std::move(*m.mutable_ack_id()));
        // TODO(#4555) - these should be scheduled via the completion queue
        params_.callback(FromProto(std::move(*m.mutable_message())),
                         pubsub::AckHandler(std::move(handler)));
      }
    }
  }

 private:
  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
  pubsub::SubscriberConnection::SubscribeParams params_;
  std::mutex mu_;
  bool shutdown_ = false;
  promise<Status> result_;
};

namespace {
class SubscriberConnectionImpl : public pubsub::SubscriberConnection {
 public:
  explicit SubscriberConnectionImpl(
      std::shared_ptr<pubsub_internal::SubscriberStub> stub)
      : stub_(std::move(stub)) {}

  ~SubscriberConnectionImpl() override {
    for (auto& t : tasks_) t.join();
  }

  future<Status> Subscribe(SubscribeParams p) override {
    auto session = std::make_shared<SubscriptionSession>(stub_, std::move(p));
    auto r = session->Start();
    tasks_.push_back(std::move(r.first));
    return std::move(r.second);
  }

 private:
  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
  std::vector<std::thread> tasks_;
};
}  // namespace

std::shared_ptr<pubsub::SubscriberConnection> MakeSubscriberConnection(
    std::shared_ptr<SubscriberStub> stub) {
  return std::make_shared<SubscriberConnectionImpl>(std::move(stub));
}

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

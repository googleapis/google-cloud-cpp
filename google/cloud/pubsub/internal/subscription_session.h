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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_SESSION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_SESSION_H

#include "google/cloud/pubsub/internal/subscriber_stub.h"
#include "google/cloud/pubsub/subscriber_connection.h"
#include "google/cloud/pubsub/version.h"
#include "google/cloud/completion_queue.h"
#include "google/cloud/status_or.h"
#include <deque>
#include <memory>
#include <mutex>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class SubscriptionSession
    : public std::enable_shared_from_this<SubscriptionSession> {
 public:
  static std::shared_ptr<SubscriptionSession> Create(
      std::shared_ptr<pubsub_internal::SubscriberStub> s,
      google::cloud::CompletionQueue executor,
      pubsub::SubscriberConnection::SubscribeParams p) {
    return std::shared_ptr<SubscriptionSession>(new SubscriptionSession(
        std::move(s), std::move(executor), std::move(p)));
  }

  future<Status> Start();

  void MessageHandled(std::size_t message_size);

 private:
  SubscriptionSession(std::shared_ptr<pubsub_internal::SubscriberStub> s,
                      google::cloud::CompletionQueue executor,
                      pubsub::SubscriberConnection::SubscribeParams p)
      : stub_(std::move(s)),
        executor_(std::move(executor)),
        params_(std::move(p)) {}

  void Cancel();

  void PullOne();

  void OnPull(StatusOr<google::pubsub::v1::PullResponse> r);

  void HandleQueue();

  void HandleQueue(std::unique_lock<std::mutex> lk);

  std::shared_ptr<pubsub_internal::SubscriberStub> stub_;
  google::cloud::CompletionQueue executor_;
  pubsub::SubscriberConnection::SubscribeParams params_;
  std::mutex mu_;
  bool shutdown_ = false;
  std::deque<google::pubsub::v1::ReceivedMessage> messages_;
  promise<Status> result_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SUBSCRIPTION_SESSION_H

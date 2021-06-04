// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_FLOW_CONTROLLED_PUBLISHER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_FLOW_CONTROLLED_PUBLISHER_CONNECTION_H

#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/version.h"
#include <condition_variable>
#include <memory>
#include <mutex>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class FlowControlledPublisherConnection
    : public pubsub::PublisherConnection,
      public std::enable_shared_from_this<FlowControlledPublisherConnection> {
 public:
  static std::shared_ptr<FlowControlledPublisherConnection> Create(
      pubsub::PublisherOptions options,
      std::shared_ptr<pubsub::PublisherConnection> child) {
    return std::shared_ptr<FlowControlledPublisherConnection>(
        new FlowControlledPublisherConnection(std::move(options),
                                              std::move(child)));
  }

  future<StatusOr<std::string>> Publish(PublishParams p) override;
  void Flush(FlushParams p) override;
  void ResumePublish(ResumePublishParams p) override;

  // These two functions may appear dangerous, returning a value after
  // locking is inherently racy. Keep in mind:
  // - Other than in test, only the `PublisherConnection` member functions are
  //   used, so these functions are really "test-only"
  // - In tests the functions are just used at the end of the test, once things
  //   have quieted down.
  std::size_t max_pending_messages() const {
    std::unique_lock<std::mutex> lk(mu_);
    return max_pending_messages_;
  }
  std::size_t max_pending_bytes() const {
    std::unique_lock<std::mutex> lk(mu_);
    return max_pending_bytes_;
  }

 private:
  explicit FlowControlledPublisherConnection(
      pubsub::PublisherOptions options,
      std::shared_ptr<pubsub::PublisherConnection> child)
      : options_(std::move(options)), child_(std::move(child)) {}

  void OnPublish(std::size_t message_size);
  bool IsFull() const {
    return pending_messages_ > options_.maximum_pending_messages() ||
           pending_bytes_ > options_.maximum_pending_bytes();
  }
  bool MakesFull(std::size_t message_size) const {
    // Accept at least one message before blocking or rejecting data.
    if (pending_messages_ == 0) return false;
    return pending_messages_ + 1 > options_.maximum_pending_messages() ||
           pending_bytes_ + message_size > options_.maximum_pending_bytes();
  }
  bool RejectWhenFull() const { return options_.full_publisher_rejects(); }
  bool BlockWhenFull() const { return options_.full_publisher_blocks(); }
  std::weak_ptr<FlowControlledPublisherConnection> WeakFromThis() {
    return shared_from_this();
  }

  pubsub::PublisherOptions const options_;
  std::shared_ptr<pubsub::PublisherConnection> const child_;

  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::size_t pending_bytes_ = 0;
  std::size_t pending_messages_ = 0;
  std::size_t max_pending_bytes_ = 0;
  std::size_t max_pending_messages_ = 0;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_FLOW_CONTROLLED_PUBLISHER_CONNECTION_H

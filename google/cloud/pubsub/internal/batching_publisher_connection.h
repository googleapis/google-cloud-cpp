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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCHING_PUBLISHER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCHING_PUBLISHER_CONNECTION_H

#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/version.h"
#include <mutex>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

class BatchingPublisherConnection
    : public pubsub::PublisherConnection,
      public std::enable_shared_from_this<BatchingPublisherConnection> {
 public:
  static std::shared_ptr<BatchingPublisherConnection> Create(
      pubsub::Topic topic, pubsub::PublisherOptions options,
      std::string ordering_key,
      std::shared_ptr<pubsub_internal::PublisherStub> stub,
      google::cloud::CompletionQueue cq,
      std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
      std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy) {
    return std::shared_ptr<BatchingPublisherConnection>(
        new BatchingPublisherConnection(
            std::move(topic), std::move(options), std::move(ordering_key),
            std::move(stub), std::move(cq), std::move(retry_policy),
            std::move(backoff_policy)));
  }

  future<StatusOr<std::string>> Publish(PublishParams p) override;
  void Flush(FlushParams) override;
  void ResumePublish(ResumePublishParams p) override;

  void UnCork();
  void DiscardCorked(Status const& status);

 private:
  explicit BatchingPublisherConnection(
      pubsub::Topic topic, pubsub::PublisherOptions options,
      std::string ordering_key,
      std::shared_ptr<pubsub_internal::PublisherStub> stub,
      google::cloud::CompletionQueue cq,
      std::unique_ptr<pubsub::RetryPolicy const> retry_policy,
      std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy)
      : topic_(std::move(topic)),
        topic_full_name_(topic_.FullName()),
        options_(std::move(options)),
        ordering_key_(std::move(ordering_key)),
        stub_(std::move(stub)),
        cq_(std::move(cq)),
        retry_policy_(std::move(retry_policy)),
        backoff_policy_(std::move(backoff_policy)) {}

  void OnTimer();
  void MaybeFlush(std::unique_lock<std::mutex> lk);
  void FlushImpl(std::unique_lock<std::mutex> lk);

  pubsub::Topic const topic_;
  std::string const topic_full_name_;
  pubsub::PublisherOptions const options_;
  std::string const ordering_key_;
  std::shared_ptr<pubsub_internal::PublisherStub> stub_;
  google::cloud::CompletionQueue cq_;
  std::unique_ptr<pubsub::RetryPolicy const> retry_policy_;
  std::unique_ptr<pubsub::BackoffPolicy const> backoff_policy_;

  std::mutex mu_;
  std::vector<promise<StatusOr<std::string>>> waiters_;
  google::pubsub::v1::PublishRequest pending_;
  std::size_t current_bytes_ = 0;
  std::chrono::system_clock::time_point batch_expiration_;
  bool corked_ = false;
  Status corked_status_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCHING_PUBLISHER_CONNECTION_H

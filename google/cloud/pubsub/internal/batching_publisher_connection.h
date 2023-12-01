// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCHING_PUBLISHER_CONNECTION_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCHING_PUBLISHER_CONNECTION_H

#include "google/cloud/pubsub/internal/batch_sink.h"
#include "google/cloud/pubsub/publisher_connection.h"
#include "google/cloud/pubsub/version.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

class BatchingPublisherConnection
    : public pubsub::PublisherConnection,
      public std::enable_shared_from_this<BatchingPublisherConnection> {
 public:
  ~BatchingPublisherConnection() override;

  static std::shared_ptr<BatchingPublisherConnection> Create(
      pubsub::Topic topic, Options opts, std::string ordering_key,
      std::shared_ptr<BatchSink> sink, CompletionQueue cq) {
    return std::shared_ptr<BatchingPublisherConnection>(
        new BatchingPublisherConnection(std::move(topic), std::move(opts),
                                        std::move(ordering_key),
                                        std::move(sink), std::move(cq)));
  }

  future<StatusOr<std::string>> Publish(PublishParams p) override;
  void Flush(FlushParams) override;
  void ResumePublish(ResumePublishParams p) override;

  void HandleError(Status const& status);

 private:
  explicit BatchingPublisherConnection(pubsub::Topic topic, Options opts,
                                       std::string ordering_key,
                                       std::shared_ptr<BatchSink> sink,
                                       CompletionQueue cq)
      : topic_(std::move(topic)),
        topic_full_name_(topic_.FullName()),
        opts_(std::move(opts)),
        ordering_key_(std::move(ordering_key)),
        sink_(std::move(sink)),
        cq_(std::move(cq)) {}

  void OnTimer();
  future<StatusOr<std::string>> CorkedError();
  void MaybeFlush(std::unique_lock<std::mutex> lk);
  void FlushImpl(std::unique_lock<std::mutex> lk);

  bool RequiresOrdering() const { return !ordering_key_.empty(); }

  pubsub::Topic const topic_;
  std::string const topic_full_name_;
  Options const opts_;
  std::string const ordering_key_;
  std::shared_ptr<BatchSink> const sink_;
  CompletionQueue cq_;

  std::mutex mu_;
  std::vector<promise<StatusOr<std::string>>> waiters_;
  google::pubsub::v1::PublishRequest pending_;
  std::size_t current_bytes_ = 0;
  std::chrono::system_clock::time_point batch_expiration_;
  future<void> timer_;

  Status corked_on_status_;
};

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_BATCHING_PUBLISHER_CONNECTION_H

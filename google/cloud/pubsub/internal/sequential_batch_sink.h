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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SEQUENTIAL_BATCH_SINK_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SEQUENTIAL_BATCH_SINK_H

#include "google/cloud/pubsub/internal/batch_sink.h"
#include "google/cloud/pubsub/version.h"
#include <deque>
#include <memory>
#include <mutex>

namespace google {
namespace cloud {
namespace pubsub_internal {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {

/// Publish message batches using a stub, with retries, but no queueing.
class SequentialBatchSink
    : public BatchSink,
      public std::enable_shared_from_this<SequentialBatchSink> {
 public:
  static std::shared_ptr<SequentialBatchSink> Create(
      std::shared_ptr<pubsub_internal::BatchSink> sink) {
    return std::shared_ptr<SequentialBatchSink>(
        new SequentialBatchSink(std::move(sink)));
  }

  ~SequentialBatchSink() override = default;

  future<StatusOr<google::pubsub::v1::PublishResponse>> AsyncPublish(
      google::pubsub::v1::PublishRequest request) override;
  void ResumePublish(std::string const& ordering_key) override;

  // Useful for testing.
  std::size_t QueueDepth() {
    std::lock_guard<std::mutex> lk(mu_);
    return queue_.size();
  }

 private:
  explicit SequentialBatchSink(
      std::shared_ptr<pubsub_internal::BatchSink> sink);

  using PublishResponse = google::pubsub::v1::PublishResponse;
  using PublishRequest = google::pubsub::v1::PublishRequest;

  void OnPublish(Status s);
  std::weak_ptr<SequentialBatchSink> WeakFromThis() {
    return shared_from_this();
  }

  struct PendingRequest {
    PublishRequest request;
    google::cloud::promise<StatusOr<PublishResponse>> promise;
  };

  std::shared_ptr<pubsub_internal::BatchSink> const sink_;
  std::mutex mu_;
  std::deque<PendingRequest> queue_;
  bool corked_on_pending_ = false;
  Status corked_on_error_;
};

}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUB_INTERNAL_SEQUENTIAL_BATCH_SINK_H

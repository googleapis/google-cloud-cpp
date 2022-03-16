// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_PARTITION_PUBLISHER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_PARTITION_PUBLISHER_H

#include "google/cloud/pubsublite/internal/alarm_registry.h"
#include "google/cloud/pubsublite/internal/batching_options.h"
#include "google/cloud/pubsublite/internal/publisher.h"
#include "google/cloud/pubsublite/internal/resumable_async_streaming_read_write_rpc.h"
#include "google/cloud/pubsublite/internal/service_composite.h"
#include "absl/functional/function_ref.h"
#include <google/cloud/pubsublite/v1/publisher.pb.h>
#include <deque>
#include <mutex>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

class PartitionPublisher
    : public Publisher<google::cloud::pubsublite::v1::Cursor> {
 private:
  using ResumableStream = std::unique_ptr<ResumableAsyncStreamingReadWriteRpc<
      google::cloud::pubsublite::v1::PublishRequest,
      google::cloud::pubsublite::v1::PublishResponse>>;

 public:
  explicit PartitionPublisher(
      absl::FunctionRef<ResumableStream(
          StreamInitializer<google::cloud::pubsublite::v1::PublishRequest,
                            google::cloud::pubsublite::v1::PublishResponse>)>,
      BatchingOptions, google::cloud::pubsublite::v1::InitialPublishRequest,
      AlarmRegistry&);

  ~PartitionPublisher() override;

  future<Status> Start() override;

  future<StatusOr<google::cloud::pubsublite::v1::Cursor>> Publish(
      google::cloud::pubsublite::v1::PubSubMessage m) override;

  void Flush() override;

  future<void> Shutdown() override;

 private:
  using UnderlyingStream = std::unique_ptr<AsyncStreamingReadWriteRpc<
      google::cloud::pubsublite::v1::PublishRequest,
      google::cloud::pubsublite::v1::PublishResponse>>;

  struct MessageWithPromise {
    google::cloud::pubsublite::v1::PubSubMessage message;
    promise<StatusOr<google::cloud::pubsublite::v1::Cursor>> message_promise;
  };

  std::deque<MessageWithPromise> UnbatchAll();

  static std::deque<std::deque<MessageWithPromise>> CreateBatches(
      std::deque<MessageWithPromise> messages, BatchingOptions const& options);

  void SatisfyOutstandingMessages();

  BatchingOptions const batching_options_;
  google::cloud::pubsublite::v1::InitialPublishRequest const
      initial_publish_request_;

  std::mutex mu_;

  ResumableStream const resumable_stream_;  // ABSL_GUARDED_BY(mu_)
  ServiceComposite service_composite_;
  std::deque<MessageWithPromise> unbatched_messages_;  // ABSL_GUARDED_BY(mu_)
  std::deque<std::deque<MessageWithPromise>>
      unsent_batches_;  // ABSL_GUARDED_BY(mu_)
  std::deque<std::deque<MessageWithPromise>>
      in_flight_batches_;  // ABSL_GUARDED_BY(mu_)
  bool writing_ = false;   // ABSL_GUARDED_BY(mu_)

  std::unique_ptr<AlarmRegistry::CancelToken> cancel_token_;

  friend class PartitionPublisherBatchingTest;
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_PARTITION_PUBLISHER_H

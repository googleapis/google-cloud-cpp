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

#include "google/cloud/pubsublite/internal/alarm_registry_interface.h"
#include "google/cloud/pubsublite/internal/batching_settings.h"
#include "google/cloud/pubsublite/internal/publisher.h"
#include "google/cloud/pubsublite/internal/resumable_async_streaming_read_write_rpc.h"
#include "absl/functional/function_ref.h"
#include <google/cloud/pubsublite/v1/publisher.pb.h>
#include <deque>
#include <mutex>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

class PartitionPublisherImpl
    : public Publisher<google::cloud::pubsublite::v1::Cursor> {
 public:
  explicit PartitionPublisherImpl(
      absl::FunctionRef<std::unique_ptr<ResumableAsyncStreamingReadWriteRpc<
          google::cloud::pubsublite::v1::PublishRequest,
          google::cloud::pubsublite::v1::PublishResponse>>(
          StreamInitializer<google::cloud::pubsublite::v1::PublishRequest,
                            google::cloud::pubsublite::v1::PublishResponse>)>,
      BatchingSettings const&,
      google::cloud::pubsublite::v1::InitialPublishRequest const&,
      std::unique_ptr<AlarmRegistryInterface>, size_t);

  future<Status> Start() override;

  future<StatusOr<google::cloud::pubsublite::v1::Cursor>> Publish(
      google::cloud::pubsublite::v1::PubSubMessage m) override;

  void Flush() override;

  future<void> Shutdown() override;

 private:
  struct MessageWithFuture {
    google::cloud::pubsublite::v1::PubSubMessage message;
    promise<StatusOr<google::cloud::pubsublite::v1::Cursor>> message_promise;
  };

  static void FlattenAndMoveBatch(
      std::deque<std::deque<MessageWithFuture>>& batches,
      std::deque<MessageWithFuture>& unbatched_messages_with_futures);

  // TODO(18suresha) make batching cleaner/less redundant across other functions
  void Rebatch(bool process_in_flight);

  void WriteBatches();

  void Read();

  void SatisfyOutstandingMessages(Status const& status);

  void DestroyCancelToken();

  future<StatusOr<std::unique_ptr<AsyncStreamingReadWriteRpc<
      google::cloud::pubsublite::v1::PublishRequest,
      google::cloud::pubsublite::v1::PublishResponse>>>>
  Initializer(std::unique_ptr<AsyncStreamingReadWriteRpc<
                  google::cloud::pubsublite::v1::PublishRequest,
                  google::cloud::pubsublite::v1::PublishResponse>>
                  stream);

  void OnAlarm();

  BatchingSettings const batching_settings_;
  google::cloud::pubsublite::v1::InitialPublishRequest const ipr_;

  std::mutex mu_;

  std::unique_ptr<ResumableAsyncStreamingReadWriteRpc<
      google::cloud::pubsublite::v1::PublishRequest,
      google::cloud::pubsublite::v1::PublishResponse>> const
      resumable_stream_;  // ABSL_GUARDED_BY(mu_)
  std::deque<MessageWithFuture>
      unbatched_messages_with_futures_;  // ABSL_GUARDED_BY(mu_)
  std::deque<std::deque<MessageWithFuture>>
      unsent_batches_;  // ABSL_GUARDED_BY(mu_)
  std::deque<std::deque<MessageWithFuture>>
      in_flight_batches_;  // ABSL_GUARDED_BY(mu_)
  bool writing_ = false;   // ABSL_GUARDED_BY(mu_)
  bool shutdown_ = false;  // ABSL_GUARDED_BY(mu_)
  std::unique_ptr<AlarmRegistryInterface::CancelToken>
      cancel_token_;  // ABSL_GUARDED_BY(mu_)
};

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_PUBSUBLITE_INTERNAL_PARTITION_PUBLISHER_H

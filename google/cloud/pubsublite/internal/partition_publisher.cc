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

#include "google/cloud/pubsublite/internal/partition_publisher.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include <functional>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

using google::cloud::pubsublite::v1::Cursor;
using google::cloud::pubsublite::v1::InitialPublishRequest;
using google::cloud::pubsublite::v1::PublishRequest;
using google::cloud::pubsublite::v1::PublishResponse;
using google::cloud::pubsublite::v1::PubSubMessage;

PartitionPublisher::PartitionPublisher(
    absl::FunctionRef<
        ResumableStream(StreamInitializer<PublishRequest, PublishResponse>)>
        resumable_stream_factory,
    BatchingOptions batching_options, InitialPublishRequest ipr,
    AlarmRegistry& alarm_registry)
    : batching_options_{std::move(batching_options)},
      initial_publish_request_{std::move(ipr)},
      // TODO(18suresha) implement `Initializer`
      resumable_stream_{resumable_stream_factory(
          [](ResumableAsyncStreamingReadWriteRpcImpl<
              PublishRequest, PublishResponse>::UnderlyingStream stream) {
            return make_ready_future(
                StatusOr<ResumableAsyncStreamingReadWriteRpcImpl<
                    PublishRequest, PublishResponse>::UnderlyingStream>(
                    std::move(stream)));
          })},
      service_composite_{resumable_stream_.get()},
      cancel_token_{alarm_registry.RegisterAlarm(
          batching_options_.alarm_period(),
          std::bind(&PartitionPublisher::Flush, this))} {}

PartitionPublisher::~PartitionPublisher() {
  future<void> shutdown = Shutdown();
  if (!shutdown.is_ready()) {
    GCP_LOG(WARNING) << "`Shutdown` must be called and finished before object "
                        "goes out of scope.";
    assert(false);
  }
  shutdown.get();
}

future<Status> PartitionPublisher::Start() {
  return service_composite_.Start();
  // TODO(18suresha) call and implement `Read`
}

future<StatusOr<Cursor>> PartitionPublisher::Publish(PubSubMessage) {
  // TODO(18suresha) implement
  return make_ready_future(StatusOr<Cursor>{Status{}});
}

void PartitionPublisher::Flush() {
  // TODO(18suresha) implement
}

future<void> PartitionPublisher::Shutdown() {
  cancel_token_ = nullptr;
  return service_composite_.Shutdown().then(
      [this](future<void>) { SatisfyOutstandingMessages(); });
}

std::deque<PartitionPublisher::MessageWithPromise>
PartitionPublisher::UnbatchAll() {  // ABSL_LOCKS_REQUIRED(mu_)
  std::lock_guard<std::mutex> g{mu_};
  std::deque<MessageWithPromise> to_return;
  for (auto& batch : in_flight_batches_) {
    for (auto& message : batch) {
      to_return.push_back(std::move(message));
    }
  }
  in_flight_batches_.clear();
  for (auto& batch : unsent_batches_) {
    for (auto& message : batch) {
      to_return.push_back(std::move(message));
    }
  }
  unsent_batches_.clear();
  for (auto& message : unbatched_messages_) {
    to_return.push_back(std::move(message));
  }
  unbatched_messages_.clear();
  return to_return;
}

std::deque<std::deque<PartitionPublisher::MessageWithPromise>>
PartitionPublisher::CreateBatches(std::deque<MessageWithPromise> messages,
                                  BatchingOptions const& options) {
  std::deque<std::deque<MessageWithPromise>> batches;
  std::deque<MessageWithPromise> current_batch;
  std::int64_t current_byte_size = 0;
  std::int64_t current_messages = 0;
  for (auto& message_with_future : messages) {
    std::int64_t message_size = message_with_future.message.ByteSizeLong();
    if (current_messages + 1 > options.maximum_batch_message_count() ||
        current_byte_size + message_size > options.maximum_batch_bytes()) {
      if (!current_batch.empty()) {
        batches.push_back(std::move(current_batch));
        // clear because current_batch left in 'unspecified state' after move
        current_batch.clear();
        current_byte_size = 0;
        current_messages = 0;
      }
    }
    current_batch.push_back(std::move(message_with_future));
    current_byte_size += message_size;
    ++current_messages;
  }
  if (!current_batch.empty()) batches.push_back(std::move(current_batch));
  return batches;
}

void PartitionPublisher::SatisfyOutstandingMessages() {
  std::deque<MessageWithPromise> messages_with_futures;
  messages_with_futures = UnbatchAll();
  for (auto& message_with_future : messages_with_futures) {
    message_with_future.message_promise.set_value(
        StatusOr<Cursor>(service_composite_.status()));
  }
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

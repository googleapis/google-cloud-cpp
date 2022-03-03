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

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

using google::cloud::pubsublite::v1::Cursor;
using google::cloud::pubsublite::v1::InitialPublishRequest;
using google::cloud::pubsublite::v1::InitialPublishResponse;
using google::cloud::pubsublite::v1::MessagePublishRequest;
using google::cloud::pubsublite::v1::MessagePublishResponse;
using google::cloud::pubsublite::v1::PublishRequest;
using google::cloud::pubsublite::v1::PublishResponse;
using google::cloud::pubsublite::v1::PubSubMessage;

using UnderlyingStream = std::unique_ptr<
    AsyncStreamingReadWriteRpc<google::cloud::pubsublite::v1::PublishRequest,
                               google::cloud::pubsublite::v1::PublishResponse>>;

void PartitionPublisherImpl::FlattenAndMoveBatch(
    std::deque<std::deque<MessageWithFuture>>& batches,
    std::deque<MessageWithFuture>& unbatched_messages_with_futures) {
  for (auto& batch : batches) {
    for (auto& message_with_future : batch) {
      unbatched_messages_with_futures.emplace_back(
          MessageWithFuture{std::move(message_with_future.message),
                            std::move(message_with_future.message_promise)});
    }
  }
  batches.clear();
}

PartitionPublisherImpl::PartitionPublisherImpl(
    absl::FunctionRef<std::unique_ptr<
        ResumableAsyncStreamingReadWriteRpc<PublishRequest, PublishResponse>>(
        StreamInitializer<PublishRequest, PublishResponse>)>
        resumable_stream_factory,
    BatchingSettings const& batching_settings, InitialPublishRequest const& ipr,
    std::unique_ptr<AlarmRegistryInterface> alarm, size_t alarm_period_ms)
    : batching_settings_{batching_settings},
      ipr_{ipr},
      resumable_stream_{resumable_stream_factory(std::bind(
          &PartitionPublisherImpl::Initializer, this, std::placeholders::_1))},
      cancel_token_{alarm->RegisterAlarm(
          absl::Milliseconds(alarm_period_ms),
          std::bind(&PartitionPublisherImpl::OnAlarm, this))} {}

future<Status> PartitionPublisherImpl::Start() {
  promise<void> root;
  future<void> root_future = root.get_future();
  future<Status> return_future;
  {
    std::lock_guard<std::mutex> g{mu_};
    return_future = root_future.then(ChainFuture(resumable_stream_->Start()))
                        .then([this](future<Status> start_status) {
                          {
                            std::lock_guard<std::mutex> g{mu_};
                            if (shutdown_) return start_status.get();
                            shutdown_ = true;
                            DestroyCancelToken();
                          }
                          Status status = start_status.get();
                          SatisfyOutstandingMessages(status);
                          return status;
                        });
  }
  root.set_value();
  Read();
  return return_future;
}

future<StatusOr<Cursor>> PartitionPublisherImpl::Publish(PubSubMessage m) {
  future<StatusOr<Cursor>> message_future;
  {
    std::lock_guard<std::mutex> g{mu_};
    if (shutdown_) return make_ready_future(StatusOr<Cursor>(Status()));
    promise<StatusOr<Cursor>> message_promise;
    message_future = message_promise.get_future();
    unbatched_messages_with_futures_.emplace_back(
        MessageWithFuture{std::move(m), std::move(message_promise)});
  }
  return message_future;
}

void PartitionPublisherImpl::Flush() {
  {
    std::lock_guard<std::mutex> g{mu_};
    if (writing_ || shutdown_) return;
    writing_ = true;
  }
  Rebatch(false);
  WriteBatches();
}

future<void> PartitionPublisherImpl::Shutdown() {
  promise<void> root;
  future<void> root_future = root.get_future();
  {
    std::lock_guard<std::mutex> g{mu_};
    if (shutdown_) return make_ready_future();
    shutdown_ = true;
    DestroyCancelToken();
    root_future = root_future.then(ChainFuture(resumable_stream_->Shutdown()))
                      .then([this](future<void>) {
                        SatisfyOutstandingMessages(Status());
                      });
  }
  root.set_value();
  return root_future;
}

// TODO(18suresha) make batching cleaner/less redundant across other functions
void PartitionPublisherImpl::Rebatch(bool process_in_flight) {
  std::lock_guard<std::mutex> g{mu_};
  std::deque<MessageWithFuture> unbatched_messages_with_futures;
  if (process_in_flight) {
    FlattenAndMoveBatch(in_flight_batches_, unbatched_messages_with_futures);
  }
  FlattenAndMoveBatch(unsent_batches_, unbatched_messages_with_futures);

  for (auto& message_with_future : unbatched_messages_with_futures_) {
    unbatched_messages_with_futures.emplace_back(
        MessageWithFuture{std::move(message_with_future.message),
                          std::move(message_with_future.message_promise)});
  }
  unbatched_messages_with_futures_.clear();

  // rebatch
  std::deque<MessageWithFuture> current_batch;
  size_t current_byte_size = 0;
  size_t current_messages = 0;
  for (auto& message_with_future : unbatched_messages_with_futures) {
    size_t message_size = message_with_future.message.ByteSizeLong();
    if (current_messages + 1 > batching_settings_.GetMessageLimit() ||
        current_byte_size + message_size > batching_settings_.GetByteLimit()) {
      if (!current_batch.empty()) {
        unsent_batches_.push_back(std::move(current_batch));
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
  if (!current_batch.empty())
    unsent_batches_.push_back(std::move(current_batch));
}

void PartitionPublisherImpl::WriteBatches() {
  promise<void> root;
  future<void> root_future = root.get_future();
  {
    std::lock_guard<std::mutex> g{mu_};
    if (unsent_batches_.empty() || shutdown_) {
      writing_ = false;
      return;
    }
    if (!writing_) {
      writing_ = true;
    }
    auto batch = std::move(unsent_batches_.front());
    unsent_batches_.pop_front();
    PublishRequest publish_request;
    MessagePublishRequest& message_publish_request =
        *publish_request.mutable_message_publish_request();
    for (auto& message_with_future : batch) {
      *message_publish_request.add_messages() = message_with_future.message;
    }
    in_flight_batches_.push_back(std::move(batch));
    root_future
        .then(ChainFuture(resumable_stream_->Write(std::move(publish_request),
                                                   grpc::WriteOptions())))
        .then([this](future<bool> write_response) {
          if (write_response.get()) WriteBatches();
          std::lock_guard<std::mutex> g{mu_};
          writing_ = false;
        });
  }
  root.set_value();
}

void PartitionPublisherImpl::Read() {
  promise<void> root;
  future<void> root_future = root.get_future();
  {
    std::lock_guard<std::mutex> g{mu_};
    if (shutdown_) return;
    root_future.then(ChainFuture(resumable_stream_->Read()))
        .then([this](future<absl::optional<PublishResponse>>
                         optional_response_future) {
          auto optional_response = optional_response_future.get();
          if (!optional_response) Read();
          std::deque<MessageWithFuture> batch;
          {
            std::lock_guard<std::mutex> g{mu_};
            if (shutdown_) return;
            // unlikely order of events
            // if we have `Read()` (pending) -> `Write()` (completes
            // successfully) -> `Write()` (fails) -> previous `Read`
            // successfully completes -> `initializer_` grabs locks and
            // rebatches and unlocks -> below `Read` continuation grabs lock
            // and tries to satisfy future in nonexistent first element of
            // `in_flight_batches_`
            if (!in_flight_batches_.empty()) {
              batch = std::move(in_flight_batches_.front());
              in_flight_batches_.pop_front();
            }
          }
          size_t offset =
              optional_response->message_response().start_cursor().offset();
          for (auto& message_with_future : batch) {
            Cursor c;
            c.set_offset(offset);
            ++offset;
            message_with_future.message_promise.set_value(std::move(c));
          }
          Read();
        });
  }
  root.set_value();
}

void PartitionPublisherImpl::SatisfyOutstandingMessages(Status const& status) {
  std::vector<MessageWithFuture> messages_with_futures;
  {
    std::lock_guard<std::mutex> g{mu_};
    for (auto& batch : in_flight_batches_) {
      for (auto& message_with_future : batch) {
        messages_with_futures.push_back(std::move(message_with_future));
      }
    }
    in_flight_batches_.clear();
    for (auto& batch : unsent_batches_) {
      for (auto& message_with_future : batch) {
        messages_with_futures.push_back(std::move(message_with_future));
      }
    }
    unsent_batches_.clear();
    for (auto& message_with_future : unbatched_messages_with_futures_) {
      messages_with_futures.push_back(std::move(message_with_future));
    }
    unbatched_messages_with_futures_.clear();
  }
  for (auto& message_with_future : messages_with_futures) {
    message_with_future.message_promise.set_value(StatusOr<Cursor>(status));
  }
}

future<StatusOr<UnderlyingStream>> PartitionPublisherImpl::Initializer(
    UnderlyingStream stream) {
  // When initializer is called, no outstanding Read() or Write()
  // futures will
  // succeed.
  Rebatch(true);
  return make_ready_future(StatusOr<UnderlyingStream>(std::move(stream)));
}

void PartitionPublisherImpl::OnAlarm() {
  {
    std::lock_guard<std::mutex> g{mu_};
    if (writing_) return;
    writing_ = true;
  }
  Rebatch(false);
  WriteBatches();
}

void PartitionPublisherImpl::DestroyCancelToken() {
  auto cancel_token = std::move(cancel_token_);
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

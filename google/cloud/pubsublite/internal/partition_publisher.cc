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
using google::cloud::pubsublite::v1::MessagePublishRequest;
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
      resumable_stream_{resumable_stream_factory(std::bind(
          &PartitionPublisher::Initializer, this, std::placeholders::_1))},
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
  auto start_return = service_composite_.Start();
  Read();
  return start_return;
}

future<StatusOr<Cursor>> PartitionPublisher::Publish(PubSubMessage m) {
  if (!service_composite_.status().ok()) {
    return make_ready_future(
        StatusOr<Cursor>(Status(StatusCode::kAborted, "Already shut down.")));
  }
  MessageWithPromise unbatched{std::move(m), promise<StatusOr<Cursor>>{}};
  future<StatusOr<Cursor>> message_future =
      unbatched.message_promise.get_future();
  std::lock_guard<std::mutex> g{mu_};
  unbatched_messages_.emplace_back(std::move(unbatched));
  return message_future;
}

void PartitionPublisher::Flush() {
  if (!service_composite_.status().ok()) return;
  {
    std::lock_guard<std::mutex> g{mu_};
    for (auto& batch :
         CreateBatches(std::move(unbatched_messages_), batching_options_)) {
      unsent_batches_.push_back(std::move(batch));
    }
    unbatched_messages_.clear();
    if (writing_) return;
    writing_ = true;
  }
  WriteBatches();
}

future<void> PartitionPublisher::Shutdown() {
  cancel_token_ = nullptr;
  return service_composite_.Shutdown().then(
      [this](future<void>) { SatisfyOutstandingMessages(); });
}

void PartitionPublisher::WriteBatches() {
  AsyncRoot root;
  std::lock_guard<std::mutex> g{mu_};
  if (unsent_batches_.empty() || !service_composite_.status().ok()) {
    writing_ = false;
    return;
  }
  in_flight_batches_.push_back(std::move(unsent_batches_.front()));
  unsent_batches_.pop_front();
  PublishRequest publish_request;
  MessagePublishRequest& message_publish_request =
      *publish_request.mutable_message_publish_request();
  for (auto& message_with_future : in_flight_batches_.back()) {
    *message_publish_request.add_messages() = message_with_future.message;
  }
  root.get_future()
      .then(ChainFuture(resumable_stream_->Write(std::move(publish_request))))
      .then([this](future<bool> write_response) {
        if (write_response.get()) return WriteBatches();
        std::lock_guard<std::mutex> g{mu_};
        writing_ = false;
      });
}

void PartitionPublisher::Read() {
  AsyncRoot root;
  if (!service_composite_.status().ok()) return;
  std::lock_guard<std::mutex> g{mu_};
  root.get_future()
      .then(ChainFuture(resumable_stream_->Read()))
      .then([this](future<absl::optional<PublishResponse>>
                       optional_response_future) {
        auto optional_response = optional_response_future.get();
        // optional not engaged implies that the retry loop has finished
        if (!optional_response) return Read();
        if (!optional_response->has_message_response()) {
          // if we don't receive a `MessagePublishResponse` and/or receive an
          // `InitialPublishResponse`, we abort because this should not be the
          // case once we start `Read`ing
          service_composite_.Abort(
              Status(StatusCode::kAborted,
                     absl::StrCat("Invalid `Read` response: ",
                                  optional_response->DebugString())));
          return;
        }

        std::deque<MessageWithPromise> batch;
        {
          std::lock_guard<std::mutex> g{mu_};
          if (in_flight_batches_.empty()) {
            return service_composite_.Abort(
                Status(StatusCode::kFailedPrecondition,
                       "Server sent message response when no batches were "
                       "outstanding."));
          }
          batch = std::move(in_flight_batches_.front());
          in_flight_batches_.pop_front();
        }
        std::int64_t offset =
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

std::deque<PartitionPublisher::MessageWithPromise>
PartitionPublisher::UnbatchAll() {
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
  auto messages_with_futures = UnbatchAll();
  for (auto& message_with_future : messages_with_futures) {
    message_with_future.message_promise.set_value(
        StatusOr<Cursor>(service_composite_.status()));
  }
}

future<StatusOr<ResumableAsyncStreamingReadWriteRpcImpl<
    PublishRequest, PublishResponse>::UnderlyingStream>>
PartitionPublisher::Initializer(
    ResumableAsyncStreamingReadWriteRpcImpl<
        PublishRequest, PublishResponse>::UnderlyingStream stream) {
  // By the time initializer is called, no outstanding Read() or Write()
  // futures will be outstanding.
  auto shared_stream = std::make_shared<ResumableAsyncStreamingReadWriteRpcImpl<
      PublishRequest, PublishResponse>::UnderlyingStream>(std::move(stream));
  PublishRequest publish_request;
  *publish_request.mutable_initial_request() = initial_publish_request_;
  return (*shared_stream)
      ->Write(publish_request, grpc::WriteOptions())
      .then([shared_stream](future<bool> write_response) {
        if (!write_response.get()) {
          return make_ready_future(absl::optional<PublishResponse>());
        }
        return (*shared_stream)->Read();
      })
      .then([shared_stream](
                future<absl::optional<PublishResponse>> read_response) {
        auto optional_response = read_response.get();
        if (optional_response && optional_response->has_initial_response()) {
          return make_ready_future(Status());
        }
        return (*shared_stream)->Finish();
      })
      .then([this, shared_stream](future<Status> status_future)
                -> StatusOr<ResumableAsyncStreamingReadWriteRpcImpl<
                    PublishRequest, PublishResponse>::UnderlyingStream> {
        Status status = status_future.get();
        if (!status.ok()) return status;
        auto unsent_batches = CreateBatches(UnbatchAll(), batching_options_);
        std::lock_guard<std::mutex> g{mu_};
        // account for race between unlocking between UnbatchAll and acquiring
        // lock above in case some unsent batches are pushed in between via a
        // `Flush` call
        for (auto& batch : unsent_batches_) {
          unsent_batches.push_back(std::move(batch));
        }
        unsent_batches_ = std::move(unsent_batches);
        return std::move(*shared_stream);
      });
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

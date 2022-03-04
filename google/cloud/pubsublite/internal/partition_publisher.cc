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
#include "google/cloud/internal/log_wrapper.h"

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

using google::cloud::internal::DebugString;
using google::cloud::pubsublite::BatchingOptions;
using google::cloud::pubsublite::v1::Cursor;
using google::cloud::pubsublite::v1::InitialPublishRequest;
using google::cloud::pubsublite::v1::MessagePublishRequest;
using google::cloud::pubsublite::v1::PublishRequest;
using google::cloud::pubsublite::v1::PublishResponse;
using google::cloud::pubsublite::v1::PubSubMessage;

using UnderlyingStream = std::unique_ptr<
    AsyncStreamingReadWriteRpc<PublishRequest, PublishResponse>>;

PartitionPublisherImpl::PartitionPublisherImpl(
    absl::FunctionRef<std::unique_ptr<
        ResumableAsyncStreamingReadWriteRpc<PublishRequest, PublishResponse>>(
        StreamInitializer<PublishRequest, PublishResponse>)>
        resumable_stream_factory,
    BatchingOptions batching_options, InitialPublishRequest ipr,
    AlarmRegistryInterface& alarm_registry)
    : batching_options_{std::move(batching_options)},
      initial_publish_request_{std::move(ipr)},
      resumable_stream_{resumable_stream_factory(std::bind(
          &PartitionPublisherImpl::Initializer, this, std::placeholders::_1))},
      lifecycle_helper_{absl::make_unique<LifecycleHelper>(*resumable_stream_)},
      cancel_token_{alarm_registry.RegisterAlarm(
          batching_options_.alarm_period_ms(),
          std::bind(&PartitionPublisherImpl::Flush, this))} {}

PartitionPublisherImpl::~PartitionPublisherImpl() {
  future<void> shutdown = Shutdown();
  if (!shutdown.is_ready()) {
    GCP_LOG(WARNING) << "`Shutdown` must be called and finished before object "
                        "goes out of scope.";
    assert(false);
  }
  shutdown.get();
}

future<Status> PartitionPublisherImpl::Start() {
  std::lock_guard<std::mutex> g{mu_};
  return lifecycle_helper_->Start();
}

future<StatusOr<Cursor>> PartitionPublisherImpl::Publish(PubSubMessage m) {
  future<StatusOr<Cursor>> message_future;
  {
    std::lock_guard<std::mutex> g{mu_};
    if (!lifecycle_helper_->status().ok())
      return make_ready_future(
          StatusOr<Cursor>(Status(StatusCode::kAborted, "Already shut down.")));
    promise<StatusOr<Cursor>> message_promise;
    message_future = message_promise.get_future();
    unbatched_messages_.emplace_back(
        MessageWithFuture{std::move(m), std::move(message_promise)});
  }
  return message_future;
}

void PartitionPublisherImpl::Flush() {
  {
    std::lock_guard<std::mutex> g{mu_};
    if (writing_ || !lifecycle_helper_->status().ok()) return;
    writing_ = true;
    AppendBatchesLockHeld();
  }
  WriteBatches();
}

future<void> PartitionPublisherImpl::Shutdown() {
  cancel_token_ = nullptr;
  return lifecycle_helper_->Shutdown().then(
      [this](future<void>) { SatisfyOutstandingMessages(); });
}

void PartitionPublisherImpl::WriteBatches() {
  promise<void> root;
  future<void> root_future = root.get_future();
  {
    std::lock_guard<std::mutex> g{mu_};
    if (unsent_batches_.empty() || !lifecycle_helper_->status().ok()) {
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
    if (!lifecycle_helper_->status().ok()) return;
    root_future.then(ChainFuture(resumable_stream_->Read()))
        .then([this](future<absl::optional<PublishResponse>>
                         optional_response_future) {
          auto optional_response = optional_response_future.get();
          if (!optional_response) return Read();
          if (!optional_response->has_message_response()) {
            // if we don't receive a `MessagePublishResponse` and/or receive an
            // `InitialPublishResponse`, we abort because this should not be the
            // case once we start `Read`ing
            std::lock_guard<std::mutex> g{mu_};
            lifecycle_helper_->Abort(
                Status(StatusCode::kAborted,
                       absl::StrCat("Invalid `Read` response: ",
                                    DebugString(std::move(*optional_response),
                                                TracingOptions()))));
            return;
          }
          std::deque<MessageWithFuture> batch;
          {
            std::lock_guard<std::mutex> g{mu_};
            if (!lifecycle_helper_->status().ok()) return;
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

std::deque<PartitionPublisherImpl::MessageWithFuture>
PartitionPublisherImpl::UnbatchAllLockHeld() {  // ABSL_LOCKS_REQUIRED(mu_)
  std::deque<MessageWithFuture> to_return;
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

void PartitionPublisherImpl::
    AppendBatchesLockHeld() {  // ABSL_LOCKS_REQUIRED(mu_)
  for (auto& batch :
       CreateBatches(std::move(unbatched_messages_), batching_options_)) {
    unsent_batches_.push_back(std::move(batch));
  }
  unbatched_messages_.clear();
}

std::deque<std::deque<PartitionPublisherImpl::MessageWithFuture>>
PartitionPublisherImpl::CreateBatches(std::deque<MessageWithFuture> messages,
                                      BatchingOptions const& options) {
  std::deque<std::deque<MessageWithFuture>> batches;
  std::deque<MessageWithFuture> current_batch;
  size_t current_byte_size = 0;
  size_t current_messages = 0;
  for (auto& message_with_future : messages) {
    size_t message_size = message_with_future.message.ByteSizeLong();
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

void PartitionPublisherImpl::SatisfyOutstandingMessages() {
  std::deque<MessageWithFuture> messages_with_futures;
  {
    std::lock_guard<std::mutex> g{mu_};
    messages_with_futures = UnbatchAllLockHeld();
  }
  for (auto& message_with_future : messages_with_futures) {
    message_with_future.message_promise.set_value(StatusOr<Cursor>(
        Status(StatusCode::kAborted, "Resumable stream finished.")));
  }
}

future<StatusOr<UnderlyingStream>> PartitionPublisherImpl::Initializer(
    UnderlyingStream stream) {
  // When initializer is called, no outstanding Read() or Write()
  // futures will succeed.
  std::shared_ptr<UnderlyingStream> shared_stream =
      std::make_shared<UnderlyingStream>(std::move(stream));
  PublishRequest publish_request;
  *publish_request.mutable_initial_request() = initial_publish_request_;
  auto return_finish_status = [shared_stream]() {
    return (*shared_stream)
        ->Finish()
        .then([shared_stream](future<Status> status) {
          return StatusOr<UnderlyingStream>(status.get());
        });
  };

  auto check_read = [this, shared_stream, return_finish_status]() {
    return (*shared_stream)
        ->Read()
        .then([this, shared_stream, return_finish_status](
                  future<absl::optional<PublishResponse>> response_future) {
          auto optional_response = response_future.get();
          if (!(optional_response &&
                optional_response->has_initial_response())) {
            return return_finish_status();
          }
          {
            std::lock_guard<std::mutex> g{mu_};
            unsent_batches_ =
                CreateBatches(UnbatchAllLockHeld(), batching_options_);
          }
          return make_ready_future(
              StatusOr<UnderlyingStream>(std::move(*shared_stream)));
        });
  };

  return (*shared_stream)
      ->Write(publish_request, grpc::WriteOptions())
      .then([shared_stream, return_finish_status,
             check_read](future<bool> write_response) {
        if (!write_response.get()) {
          return return_finish_status();
        }
        return check_read();
      });
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

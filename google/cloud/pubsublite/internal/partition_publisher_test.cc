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
#include "google/cloud/pubsublite/testing/mock_alarm_registry.h"
#include "google/cloud/pubsublite/testing/mock_alarm_token.h"
#include "google/cloud/pubsublite/testing/mock_async_reader_writer.h"
#include "google/cloud/pubsublite/testing/mock_resumable_async_reader_writer_stream.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <chrono>
#include <memory>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::_;
using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

using google::cloud::pubsublite::BatchingOptions;
using google::cloud::pubsublite::v1::Cursor;
using google::cloud::pubsublite::v1::InitialPublishRequest;
using google::cloud::pubsublite::v1::InitialPublishResponse;
using google::cloud::pubsublite::v1::MessagePublishRequest;
using google::cloud::pubsublite::v1::PublishRequest;
using google::cloud::pubsublite::v1::PublishResponse;
using google::cloud::pubsublite::v1::PubSubMessage;

using ::google::cloud::pubsublite_testing::MockAlarmRegistry;
using ::google::cloud::pubsublite_testing::MockAsyncReaderWriter;
using ::google::cloud::pubsublite_testing::MockResumableAsyncReaderWriter;
using ::google::cloud::pubsublite_testing::MockToken;

using AsyncReaderWriter =
    MockAsyncReaderWriter<PublishRequest, PublishResponse>;

using AsyncReadWriteStreamReturnType = std::unique_ptr<
    AsyncStreamingReadWriteRpc<PublishRequest, PublishResponse>>;

using ResumableAsyncReadWriteStream = std::unique_ptr<
    ResumableAsyncStreamingReadWriteRpc<PublishRequest, PublishResponse>>;

auto const kAlarmDuration = std::chrono::milliseconds{1000 * 3600};

PublishRequest GetInitializerPublishRequest() {
  PublishRequest publish_request;
  *publish_request.mutable_initial_request() =
      InitialPublishRequest::default_instance();
  return publish_request;
}

PublishResponse GetInitializerPublishResponse() {
  PublishResponse publish_response;
  *publish_response.mutable_initial_response() =
      InitialPublishResponse::default_instance();
  return publish_response;
}

class PartitionPublisherBatchingTest : public ::testing::Test {
 protected:
  PartitionPublisherBatchingTest() = default;

  static std::deque<
      std::deque<std::pair<PubSubMessage, promise<StatusOr<Cursor>>>>>
  TestCreateBatches(
      std::deque<std::pair<PubSubMessage, promise<StatusOr<Cursor>>>> messages,
      google::cloud::pubsublite::BatchingOptions const& options) {
    std::deque<PartitionPublisherImpl::MessageWithFuture> message_with_futures;
    for (auto& message_with_future : messages) {
      message_with_futures.emplace_back(
          PartitionPublisherImpl::MessageWithFuture{
              std::move(message_with_future.first),
              std::move(message_with_future.second)});
    }
    auto batches = PartitionPublisherImpl::CreateBatches(
        std::move(message_with_futures), std::move(options));
    std::deque<std::deque<std::pair<PubSubMessage, promise<StatusOr<Cursor>>>>>
        ret_batches;
    for (auto& batch : batches) {
      std::deque<std::pair<PubSubMessage, promise<StatusOr<Cursor>>>>
          message_batch;
      for (auto& message_with_future : batch) {
        message_batch.emplace_back(
            std::pair<PubSubMessage, promise<StatusOr<Cursor>>>{
                std::move(message_with_future.message),
                std::move(message_with_future.message_promise)});
      }
      ret_batches.push_back(std::move(message_batch));
    }
    return ret_batches;
  }
};

// batching logic
TEST_F(PartitionPublisherBatchingTest, SingleMessageBatch) {
  std::deque<std::pair<PubSubMessage, promise<StatusOr<Cursor>>>>
      message_with_futures;
  std::deque<PubSubMessage> messages;
  for (unsigned int i = 0; i < 10; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    promise<StatusOr<Cursor>> message_promise;
    message_promise.get_future().then(
        [i](future<StatusOr<Cursor>> status_future) {
          auto status = status_future.get();
          EXPECT_FALSE(status.ok());
          EXPECT_EQ(status.status(),
                    Status(StatusCode::kUnavailable, std::to_string(i)));
        });
    message_with_futures.emplace_back(
        std::pair<PubSubMessage, promise<StatusOr<Cursor>>>{
            message, std::move(message_promise)});
    messages.push_back(std::move(message));
  }
  BatchingOptions options;
  options.set_maximum_batch_message_count(1);

  auto batches =
      TestCreateBatches(std::move(message_with_futures), std::move(options));
  EXPECT_EQ(batches.size(), messages.size());
  for (unsigned int i = 0; i < messages.size(); ++i) {
    auto& batch = batches[i];
    EXPECT_EQ(batch.size(), 1);
    EXPECT_THAT(batch[0].first, IsProtoEqual(messages[i]));
    batch[0].second.set_value(
        StatusOr<Cursor>{Status(StatusCode::kUnavailable, std::to_string(i))});
  }
}

TEST_F(PartitionPublisherBatchingTest,
       SingleMessageBatchMessageSizeRestriction) {
  std::deque<std::pair<PubSubMessage, promise<StatusOr<Cursor>>>>
      message_with_futures;
  std::deque<PubSubMessage> messages;
  for (unsigned int i = 0; i < 10; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    promise<StatusOr<Cursor>> message_promise;
    message_promise.get_future().then(
        [i](future<StatusOr<Cursor>> status_future) {
          auto status = status_future.get();
          EXPECT_FALSE(status.ok());
          EXPECT_EQ(status.status(),
                    Status(StatusCode::kUnavailable, std::to_string(i)));
        });
    message_with_futures.emplace_back(
        std::pair<PubSubMessage, promise<StatusOr<Cursor>>>{
            message, std::move(message_promise)});
    messages.push_back(std::move(message));
  }
  BatchingOptions options;
  options.set_maximum_batch_bytes(1);

  auto batches =
      TestCreateBatches(std::move(message_with_futures), std::move(options));
  EXPECT_EQ(batches.size(), messages.size());
  for (unsigned int i = 0; i < messages.size(); ++i) {
    auto& batch = batches[i];
    EXPECT_EQ(batch.size(), 1);
    EXPECT_THAT(batch[0].first, IsProtoEqual(messages[i]));
    batch[0].second.set_value(
        StatusOr<Cursor>{Status(StatusCode::kUnavailable, std::to_string(i))});
  }
}

TEST_F(PartitionPublisherBatchingTest, FullAndPartialBatches) {
  std::deque<std::pair<PubSubMessage, promise<StatusOr<Cursor>>>>
      message_with_futures;
  std::deque<PubSubMessage> messages;
  for (unsigned int i = 0; i < 10; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    promise<StatusOr<Cursor>> message_promise;
    message_promise.get_future().then(
        [i](future<StatusOr<Cursor>> status_future) {
          auto status = status_future.get();
          EXPECT_FALSE(status.ok());
          EXPECT_EQ(status.status(),
                    Status(StatusCode::kUnavailable,
                           absl::StrCat("batch:", std::to_string(i / 3),
                                        "offset:", std::to_string(i))));
        });
    message_with_futures.emplace_back(
        std::pair<PubSubMessage, promise<StatusOr<Cursor>>>{
            message, std::move(message_promise)});
    messages.push_back(std::move(message));
  }
  BatchingOptions options;
  options.set_maximum_batch_message_count(3);

  auto batches =
      TestCreateBatches(std::move(message_with_futures), std::move(options));
  EXPECT_EQ(batches.size(), ceil(((double)messages.size()) / 3.0));
  for (unsigned int i = 0; i < batches.size(); ++i) {
    auto& batch = batches[i];
    if (i < batches.size() - 1) {
      EXPECT_EQ(batch.size(), 3);
    } else {
      EXPECT_EQ(batch.size(), messages.size() % 3);
    }
    for (unsigned int j = 0; j < batch.size(); ++j) {
      EXPECT_THAT(batch[j].first, IsProtoEqual(messages[i * 3 + j]));
      batch[j].second.set_value(StatusOr<Cursor>{
          Status(StatusCode::kUnavailable,
                 absl::StrCat("batch:", std::to_string(i),
                              "offset:", std::to_string(i * 3 + j)))});
    }
  }
}

TEST_F(PartitionPublisherBatchingTest, FullBatchesMessageSizeRestriction) {
  std::deque<std::pair<PubSubMessage, promise<StatusOr<Cursor>>>>
      message_with_futures;
  std::deque<PubSubMessage> messages;
  for (unsigned int i = 0; i < 9; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    promise<StatusOr<Cursor>> message_promise;
    message_promise.get_future().then(
        [i](future<StatusOr<Cursor>> status_future) {
          auto status = status_future.get();
          EXPECT_FALSE(status.ok());
          EXPECT_EQ(status.status(),
                    Status(StatusCode::kUnavailable,
                           absl::StrCat("batch:", std::to_string(i / 3),
                                        "offset:", std::to_string(i))));
        });
    message_with_futures.emplace_back(
        std::pair<PubSubMessage, promise<StatusOr<Cursor>>>{
            message, std::move(message_promise)});
    messages.push_back(std::move(message));
  }
  BatchingOptions options;
  options.set_maximum_batch_bytes(messages[0].ByteSizeLong() * 3);

  auto batches =
      TestCreateBatches(std::move(message_with_futures), std::move(options));
  EXPECT_EQ(batches.size(), ceil(((double)messages.size()) / 3.0));
  for (unsigned int i = 0; i < batches.size(); ++i) {
    auto& batch = batches[i];
    EXPECT_EQ(batch.size(), 3);
    for (unsigned int j = 0; j < batch.size(); ++j) {
      EXPECT_THAT(batch[j].first, IsProtoEqual(messages[i * 3 + j]));
      batch[j].second.set_value(StatusOr<Cursor>{
          Status(StatusCode::kUnavailable,
                 absl::StrCat("batch:", std::to_string(i),
                              "offset:", std::to_string(i * 3 + j)))});
    }
  }
}

class PartitionPublisherTest : public ::testing::Test {
 protected:
  PartitionPublisherTest() {
    EXPECT_CALL(alarm_, RegisterAlarm(kAlarmDuration, _))
        .WillOnce(WithArg<1>([&](std::function<void()> on_alarm) {
          leaked_alarm_ = std::move(on_alarm);
          return absl::WrapUnique(alarm_token_);
        }));

    BatchingOptions options;
    options.set_maximum_batch_message_count(batch_boundary_);
    options.set_alarm_period(kAlarmDuration);

    publisher_ = absl::make_unique<PartitionPublisherImpl>(
        [&](StreamInitializer<PublishRequest, PublishResponse> const&
                initializer) {
          initializer_ = std::move(initializer);
          return absl::WrapUnique(resumable_stream_);
        },
        std::move(options), InitialPublishRequest::default_instance(), alarm_);
  }

  unsigned int batch_boundary_ = 5;
  StreamInitializer<PublishRequest, PublishResponse> initializer_;
  MockToken* alarm_token_ = new StrictMock<MockToken>;
  MockAlarmRegistry alarm_;
  std::function<void()> leaked_alarm_;
  MockResumableAsyncReaderWriter<PublishRequest, PublishResponse>*
      resumable_stream_ = new StrictMock<
          MockResumableAsyncReaderWriter<PublishRequest, PublishResponse>>;
  std::unique_ptr<Publisher<Cursor>> publisher_;
};

TEST_F(PartitionPublisherTest, SatisfyOutstandingMessages) {
  InSequence seq;

  promise<Status> start_promise;
  EXPECT_CALL(*resumable_stream_, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  promise<absl::optional<PublishResponse>> read_promise;
  // first `Read` response is nullopt because resumable stream in retry loop
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))))
      .WillOnce(Return(ByMove(read_promise.get_future())));

  future<Status> publisher_start_future = publisher_->Start();

  auto* underlying_stream = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(absl::WrapUnique(underlying_stream));

  std::vector<PubSubMessage> individual_publish_messages;
  for (unsigned int i = 0; i < 11; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    individual_publish_messages.push_back(std::move(message));
  }

  std::vector<future<StatusOr<Cursor>>> publish_message_futures;
  for (unsigned int i = 0; i < 11; ++i) {
    publish_message_futures.push_back(
        publisher_->Publish(individual_publish_messages[i]));
  }

  EXPECT_CALL(*alarm_token_, Destroy);
  EXPECT_CALL(*resumable_stream_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  auto shutdown_future = publisher_->Shutdown();
  start_promise.set_value(Status());
  read_promise.set_value(absl::optional<PublishResponse>());
  shutdown_future.get();

  for (unsigned int i = 0; i < individual_publish_messages.size(); ++i) {
    auto individual_message_publish_response = publish_message_futures[i].get();
    EXPECT_FALSE(individual_message_publish_response);
    EXPECT_EQ(individual_message_publish_response.status(),
              Status(StatusCode::kAborted, "`Shutdown` called"));
  }

  // shouldn't do anything b/c shutdown
  publisher_->Flush();
  EXPECT_EQ(publisher_start_future.get(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
}

TEST_F(PartitionPublisherTest, InvalidReadResponse) {
  InSequence seq;

  promise<Status> start_promise;
  EXPECT_CALL(*resumable_stream_, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  promise<absl::optional<PublishResponse>> read_promise;
  // first `Read` response is nullopt because resumable stream in retry loop
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))))
      .WillOnce(Return(ByMove(read_promise.get_future())));

  future<Status> publisher_start_future = publisher_->Start();

  auto* underlying_stream = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(absl::WrapUnique(underlying_stream));

  future<StatusOr<Cursor>> publish_future =
      publisher_->Publish(PubSubMessage::default_instance());

  PublishRequest publish_request;
  *publish_request.mutable_message_publish_request()->add_messages() =
      PubSubMessage::default_instance();
  EXPECT_CALL(*resumable_stream_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(make_ready_future(true))));

  leaked_alarm_();

  read_promise.set_value(absl::make_optional(GetInitializerPublishResponse()));

  EXPECT_EQ(
      publisher_start_future.get(),
      Status(StatusCode::kAborted,
             absl::StrCat("Invalid `Read` response: ",
                          GetInitializerPublishResponse().DebugString())));

  // shouldn't do anything b/c lifecycle ended
  publisher_->Flush();

  EXPECT_CALL(*alarm_token_, Destroy);
  EXPECT_CALL(*resumable_stream_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();
  start_promise.set_value(Status());

  auto individual_message_publish_response = publish_future.get();
  EXPECT_FALSE(individual_message_publish_response);
  EXPECT_EQ(
      individual_message_publish_response.status(),
      Status(StatusCode::kAborted,
             absl::StrCat("Invalid `Read` response: ",
                          GetInitializerPublishResponse().DebugString())));
}

TEST_F(PartitionPublisherTest, ReadFinishedWhenNothingInFlight) {
  InSequence seq;

  promise<Status> start_promise;
  EXPECT_CALL(*resumable_stream_, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  promise<absl::optional<PublishResponse>> read_promise;
  // first `Read` response is nullopt because resumable stream in retry loop
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))))
      .WillOnce(Return(ByMove(read_promise.get_future())));

  future<Status> publisher_start_future = publisher_->Start();

  auto* underlying_stream = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(absl::WrapUnique(underlying_stream));

  future<StatusOr<Cursor>> publish_future =
      publisher_->Publish(PubSubMessage::default_instance());

  PublishResponse publish_response1;
  publish_response1.mutable_message_response()
      ->mutable_start_cursor()
      ->set_offset(0);
  promise<absl::optional<PublishResponse>> read_promise1;
  read_promise.set_value(std::move(publish_response1));

  EXPECT_EQ(publisher_start_future.get(),
            Status(StatusCode::kFailedPrecondition,
                   "Server sent message response when no batches were "
                   "outstanding."));

  // shouldn't do anything b/c lifecycle ended
  publisher_->Flush();

  EXPECT_CALL(*alarm_token_, Destroy);
  EXPECT_CALL(*resumable_stream_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();
  start_promise.set_value(Status());

  auto individual_message_publish_response = publish_future.get();
  EXPECT_FALSE(individual_message_publish_response);
  EXPECT_EQ(individual_message_publish_response.status(),
            Status(StatusCode::kFailedPrecondition,
                   "Server sent message response when no batches were "
                   "outstanding."));
}

TEST_F(PartitionPublisherTest, PublishAfterShutdown) {
  InSequence seq;

  promise<Status> start_promise;
  EXPECT_CALL(*resumable_stream_, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  promise<absl::optional<PublishResponse>> read_promise;
  // first `Read` response is nullopt because resumable stream in retry loop
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))))
      .WillOnce(Return(ByMove(read_promise.get_future())));

  future<Status> publisher_start_future = publisher_->Start();

  auto* underlying_stream = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(absl::WrapUnique(underlying_stream));

  EXPECT_CALL(*alarm_token_, Destroy);
  EXPECT_CALL(*resumable_stream_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();
  read_promise.set_value(absl::optional<PublishResponse>());
  start_promise.set_value(Status());
  EXPECT_EQ(publisher_start_future.get(),
            Status(StatusCode::kAborted, "`Shutdown` called"));

  auto publish_future = publisher_->Publish(PubSubMessage::default_instance());
  auto invalid_publish_response = publish_future.get();
  EXPECT_FALSE(invalid_publish_response.ok());
  EXPECT_EQ(invalid_publish_response.status(),
            Status(StatusCode::kAborted, "Already shut down."));
}

TEST_F(PartitionPublisherTest, InitializerWriteFailureThenGood) {
  InSequence seq;

  promise<Status> start_promise;
  EXPECT_CALL(*resumable_stream_, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  promise<absl::optional<PublishResponse>> read_promise;
  // first `Read` response is nullopt because resumable stream in retry loop
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))))
      .WillOnce(Return(ByMove(read_promise.get_future())));

  future<Status> publisher_start_future = publisher_->Start();

  auto* underlying_stream = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(false))));
  EXPECT_CALL(*underlying_stream, Finish)
      .WillOnce(Return(ByMove(
          make_ready_future(Status(StatusCode::kUnavailable, "Unavailable")))));
  initializer_(absl::WrapUnique(underlying_stream));

  underlying_stream = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(absl::WrapUnique(underlying_stream));

  EXPECT_CALL(*alarm_token_, Destroy);
  EXPECT_CALL(*resumable_stream_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();
  start_promise.set_value(Status());
  EXPECT_EQ(publisher_start_future.get(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
}

TEST_F(PartitionPublisherTest, InitializerReadFailureThenGood) {
  InSequence seq;

  promise<Status> start_promise;
  EXPECT_CALL(*resumable_stream_, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  promise<absl::optional<PublishResponse>> read_promise;
  // first `Read` response is nullopt because resumable stream in retry loop
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))))
      .WillOnce(Return(ByMove(read_promise.get_future())));

  future<Status> publisher_start_future = publisher_->Start();

  auto* underlying_stream = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))));
  EXPECT_CALL(*underlying_stream, Finish)
      .WillOnce(Return(ByMove(
          make_ready_future(Status(StatusCode::kUnavailable, "Unavailable")))));
  initializer_(absl::WrapUnique(underlying_stream));

  underlying_stream = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(absl::WrapUnique(underlying_stream));

  EXPECT_CALL(*alarm_token_, Destroy);
  EXPECT_CALL(*resumable_stream_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();
  start_promise.set_value(Status());
  EXPECT_EQ(publisher_start_future.get(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
}

TEST_F(PartitionPublisherTest, ResumableStreamPermanentError) {
  InSequence seq;

  promise<Status> start_promise;
  EXPECT_CALL(*resumable_stream_, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  promise<absl::optional<PublishResponse>> read_promise;
  // first `Read` response is nullopt because resumable stream in retry loop
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))))
      .WillOnce(Return(ByMove(read_promise.get_future())));

  future<Status> publisher_start_future = publisher_->Start();

  auto* underlying_stream = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(absl::WrapUnique(underlying_stream));

  future<StatusOr<Cursor>> publish_future =
      publisher_->Publish(PubSubMessage::default_instance());

  PublishRequest publish_request;
  *publish_request.mutable_message_publish_request()->add_messages() =
      PubSubMessage::default_instance();
  EXPECT_CALL(*resumable_stream_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(make_ready_future(true))));

  leaked_alarm_();

  start_promise.set_value(Status(StatusCode::kInternal, "Permanent Error"));
  read_promise.set_value(absl::optional<PublishResponse>());

  EXPECT_CALL(*alarm_token_, Destroy);
  EXPECT_CALL(*resumable_stream_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();

  auto individual_message_publish_response = publish_future.get();
  EXPECT_FALSE(individual_message_publish_response);
  EXPECT_EQ(individual_message_publish_response.status(),
            Status(StatusCode::kInternal, "Permanent Error"));
}

class InitializedPartitionPublisherTest : public PartitionPublisherTest {
 protected:
  InitializedPartitionPublisherTest() {
    InSequence seq;

    EXPECT_CALL(*resumable_stream_, Start)
        .WillOnce(Return(ByMove(start_promise_.get_future())));

    // first `Read` response is nullopt because resumable stream in retry loop
    EXPECT_CALL(*resumable_stream_, Read)
        .WillOnce(Return(
            ByMove(make_ready_future(absl::optional<PublishResponse>()))))
        .WillOnce(Return(ByMove(read_promise_.get_future())));

    publisher_start_future_ = publisher_->Start();

    underlying_stream_ = new StrictMock<AsyncReaderWriter>;
    EXPECT_CALL(*underlying_stream_,
                Write(IsProtoEqual(GetInitializerPublishRequest()), _))
        .WillOnce(Return(ByMove(make_ready_future(true))));
    EXPECT_CALL(*underlying_stream_, Read)
        .WillOnce(Return(ByMove(make_ready_future(
            absl::make_optional(GetInitializerPublishResponse())))));
    initializer_(absl::WrapUnique(underlying_stream_));
  }

  ~InitializedPartitionPublisherTest() override {
    InSequence seq;

    EXPECT_CALL(*alarm_token_, Destroy);
    EXPECT_CALL(*resumable_stream_, Shutdown)
        .WillOnce(Return(ByMove(make_ready_future())));
    publisher_->Shutdown().get();
    read_promise1_.set_value(absl::optional<PublishResponse>());
    start_promise_.set_value(Status());
    EXPECT_EQ(publisher_start_future_.get(),
              Status(StatusCode::kAborted, "`Shutdown` called"));
  }

  promise<Status> start_promise_;
  promise<absl::optional<PublishResponse>> read_promise_;
  future<Status> publisher_start_future_;
  MockAsyncReaderWriter<PublishRequest, PublishResponse>* underlying_stream_;
  promise<absl::optional<PublishResponse>> read_promise1_;
};

TEST_F(InitializedPartitionPublisherTest, SinglePublishGood) {
  InSequence seq;

  future<StatusOr<Cursor>> publish_future =
      publisher_->Publish(PubSubMessage::default_instance());

  PublishRequest publish_request;
  *publish_request.mutable_message_publish_request()->add_messages() =
      PubSubMessage::default_instance();
  EXPECT_CALL(*resumable_stream_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(make_ready_future(true))));

  leaked_alarm_();

  PublishResponse publish_response1;
  publish_response1.mutable_message_response()
      ->mutable_start_cursor()
      ->set_offset(20);
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(Return(ByMove(read_promise1_.get_future())));
  read_promise_.set_value(std::move(publish_response1));

  auto individual_message_publish_response = publish_future.get();
  EXPECT_TRUE(individual_message_publish_response);
  EXPECT_EQ(individual_message_publish_response->offset(), 20);

  // shouldn't do anything b/c lifecycle ended
  publisher_->Flush();
}

TEST_F(InitializedPartitionPublisherTest, SinglePublishGoodThroughFlush) {
  InSequence seq;

  future<StatusOr<Cursor>> publish_future =
      publisher_->Publish(PubSubMessage::default_instance());

  PublishRequest publish_request;
  *publish_request.mutable_message_publish_request()->add_messages() =
      PubSubMessage::default_instance();
  EXPECT_CALL(*resumable_stream_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(make_ready_future(true))));

  publisher_->Flush();

  PublishResponse publish_response1;
  publish_response1.mutable_message_response()
      ->mutable_start_cursor()
      ->set_offset(0);
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(Return(ByMove(read_promise1_.get_future())));
  read_promise_.set_value(std::move(publish_response1));

  auto individual_message_publish_response = publish_future.get();
  EXPECT_TRUE(individual_message_publish_response);
  EXPECT_EQ(individual_message_publish_response->offset(), 0);

  // shouldn't do anything b/c no messages left
  publisher_->Flush();
}

TEST_F(InitializedPartitionPublisherTest,
       InFlightBatchAndUnsentMessageThenRetry) {
  InSequence seq;

  std::vector<PubSubMessage> individual_publish_messages;
  for (unsigned int i = 0; i < 3; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    individual_publish_messages.push_back(std::move(message));
  }

  std::vector<future<StatusOr<Cursor>>> publish_message_futures;
  publish_message_futures.push_back(
      publisher_->Publish(individual_publish_messages[0]));
  publish_message_futures.push_back(
      publisher_->Publish(individual_publish_messages[1]));

  PublishRequest publish_request;
  MessagePublishRequest& message_publish_request =
      *publish_request.mutable_message_publish_request();
  *message_publish_request.add_messages() = individual_publish_messages[0];
  *message_publish_request.add_messages() = individual_publish_messages[1];
  EXPECT_CALL(*resumable_stream_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(make_ready_future(true))));

  leaked_alarm_();

  publish_message_futures.push_back(
      publisher_->Publish(individual_publish_messages[2]));

  underlying_stream_ = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream_,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream_, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(absl::WrapUnique(underlying_stream_));

  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(Return(ByMove(read_promise1_.get_future())));
  read_promise_.set_value(absl::optional<PublishResponse>());

  PublishRequest publish_request1;
  MessagePublishRequest& message_publish_request1 =
      *publish_request1.mutable_message_publish_request();
  *message_publish_request1.add_messages() = individual_publish_messages[0];
  *message_publish_request1.add_messages() = individual_publish_messages[1];
  *message_publish_request1.add_messages() = individual_publish_messages[2];
  EXPECT_CALL(*resumable_stream_, Write(IsProtoEqual(publish_request1)))
      .WillOnce(Return(ByMove(make_ready_future(true))));

  leaked_alarm_();

  PublishResponse publish_response1;
  publish_response1.mutable_message_response()
      ->mutable_start_cursor()
      ->set_offset(0);
  auto temp_promise = std::move(read_promise1_);
  read_promise1_ = promise<absl::optional<PublishResponse>>{};
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(Return(ByMove(read_promise1_.get_future())));
  temp_promise.set_value(std::move(publish_response1));

  for (unsigned int i = 0; i < 3; ++i) {
    auto individual_message_publish_response = publish_message_futures[i].get();
    EXPECT_TRUE(individual_message_publish_response);
    EXPECT_EQ(individual_message_publish_response->offset(), i);
  }
}

TEST_F(InitializedPartitionPublisherTest,
       InFlightBatchUnsentBatchUnsentMessageThenRetry) {
  InSequence seq;

  std::vector<PubSubMessage> individual_publish_messages;
  for (unsigned int i = 0; i < 11; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    individual_publish_messages.push_back(std::move(message));
  }

  std::vector<future<StatusOr<Cursor>>> publish_message_futures;
  for (unsigned int i = 0; i < 10; ++i) {
    publish_message_futures.push_back(
        publisher_->Publish(individual_publish_messages[i]));
  }

  PublishRequest publish_request;
  for (unsigned int i = 0; i < batch_boundary_; ++i) {
    *publish_request.mutable_message_publish_request()->add_messages() =
        individual_publish_messages[i];
  }

  promise<bool> write_promise;
  EXPECT_CALL(*resumable_stream_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(write_promise.get_future())));

  leaked_alarm_();

  publish_message_futures.push_back(
      publisher_->Publish(individual_publish_messages[10]));

  underlying_stream_ = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream_,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream_, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(absl::WrapUnique(underlying_stream_));

  write_promise.set_value(false);

  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(Return(ByMove(read_promise1_.get_future())));
  read_promise_.set_value(absl::optional<PublishResponse>());

  for (unsigned int i = 0; i < individual_publish_messages.size();) {
    PublishRequest publish_request1;
    unsigned int orig_i = i;
    for (;
         i < orig_i + batch_boundary_ && i < individual_publish_messages.size();
         ++i) {
      *publish_request1.mutable_message_publish_request()->add_messages() =
          individual_publish_messages[i];
    }
    EXPECT_CALL(*resumable_stream_,
                Write(IsProtoEqual(std::move(publish_request1))))
        .WillOnce(Return(ByMove(make_ready_future(true))));
  }

  leaked_alarm_();

  for (unsigned int i = 0; i < individual_publish_messages.size();
       i += batch_boundary_) {
    PublishResponse publish_response1;
    publish_response1.mutable_message_response()
        ->mutable_start_cursor()
        ->set_offset(i);
    auto temp_read_promise = std::move(read_promise1_);
    read_promise1_ = promise<absl::optional<PublishResponse>>{};
    EXPECT_CALL(*resumable_stream_, Read)
        .WillOnce(Return(ByMove(read_promise1_.get_future())));
    temp_read_promise.set_value(std::move(publish_response1));
  }

  for (unsigned int i = 0; i < individual_publish_messages.size(); ++i) {
    auto individual_message_publish_response = publish_message_futures[i].get();
    EXPECT_TRUE(individual_message_publish_response);
    EXPECT_EQ(individual_message_publish_response->offset(), i);
  }
}

TEST_F(InitializedPartitionPublisherTest, RetryAfterSuccessfulWriteBeforeRead) {
  InSequence seq;

  std::vector<PubSubMessage> individual_publish_messages;
  for (unsigned int i = 0; i < 11; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    individual_publish_messages.push_back(std::move(message));
  }

  std::vector<future<StatusOr<Cursor>>> publish_message_futures;
  for (unsigned int i = 0; i < 10; ++i) {
    publish_message_futures.push_back(
        publisher_->Publish(individual_publish_messages[i]));
  }

  PublishRequest publish_request;
  for (unsigned int i = 0; i < batch_boundary_; ++i) {
    *publish_request.mutable_message_publish_request()->add_messages() =
        individual_publish_messages[i];
  }

  EXPECT_CALL(*resumable_stream_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(make_ready_future(true))));

  publish_request = PublishRequest();
  for (unsigned int i = 5; i < 5 + batch_boundary_; ++i) {
    *publish_request.mutable_message_publish_request()->add_messages() =
        individual_publish_messages[i];
  }
  promise<bool> write_promise;
  EXPECT_CALL(*resumable_stream_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(write_promise.get_future())));

  leaked_alarm_();

  publish_message_futures.push_back(
      publisher_->Publish(individual_publish_messages[10]));

  underlying_stream_ = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream_,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream_, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(absl::WrapUnique(underlying_stream_));

  write_promise.set_value(false);

  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(Return(ByMove(read_promise1_.get_future())));
  read_promise_.set_value(absl::optional<PublishResponse>());

  for (unsigned int i = 0; i < individual_publish_messages.size();) {
    PublishRequest publish_request1;
    unsigned int orig_i = i;
    for (;
         i < orig_i + batch_boundary_ && i < individual_publish_messages.size();
         ++i) {
      *publish_request1.mutable_message_publish_request()->add_messages() =
          individual_publish_messages[i];
    }
    EXPECT_CALL(*resumable_stream_,
                Write(IsProtoEqual(std::move(publish_request1))))
        .WillOnce(Return(ByMove(make_ready_future(true))));
  }

  leaked_alarm_();

  for (unsigned int i = 0; i < individual_publish_messages.size();
       i += batch_boundary_) {
    PublishResponse publish_response1;
    publish_response1.mutable_message_response()
        ->mutable_start_cursor()
        ->set_offset(i);
    auto temp_read_promise = std::move(read_promise1_);
    read_promise1_ = promise<absl::optional<PublishResponse>>{};
    EXPECT_CALL(*resumable_stream_, Read)
        .WillOnce(Return(ByMove(read_promise1_.get_future())));
    temp_read_promise.set_value(std::move(publish_response1));
  }

  for (unsigned int i = 0; i < individual_publish_messages.size(); ++i) {
    auto individual_message_publish_response = publish_message_futures[i].get();
    EXPECT_TRUE(individual_message_publish_response);
    EXPECT_EQ(individual_message_publish_response->offset(), i);
  }
}

TEST_F(InitializedPartitionPublisherTest, RetryAfterSuccessfulWriteAfterRead) {
  InSequence seq;

  std::vector<PubSubMessage> individual_publish_messages;
  for (unsigned int i = 0; i < 11; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    individual_publish_messages.push_back(std::move(message));
  }

  std::vector<future<StatusOr<Cursor>>> publish_message_futures;
  for (unsigned int i = 0; i < 10; ++i) {
    publish_message_futures.push_back(
        publisher_->Publish(individual_publish_messages[i]));
  }

  PublishRequest publish_request;
  for (unsigned int i = 0; i < batch_boundary_; ++i) {
    *publish_request.mutable_message_publish_request()->add_messages() =
        individual_publish_messages[i];
  }

  EXPECT_CALL(*resumable_stream_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(make_ready_future(true))));

  publish_request = PublishRequest();
  for (unsigned int i = 5; i < 5 + batch_boundary_; ++i) {
    *publish_request.mutable_message_publish_request()->add_messages() =
        individual_publish_messages[i];
  }
  promise<bool> write_promise;
  EXPECT_CALL(*resumable_stream_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(write_promise.get_future())));

  PublishResponse publish_response;
  publish_response.mutable_message_response()
      ->mutable_start_cursor()
      ->set_offset(0);

  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(Return(ByMove(read_promise1_.get_future())));

  leaked_alarm_();
  read_promise_.set_value(std::move(publish_response));

  publish_message_futures.push_back(
      publisher_->Publish(individual_publish_messages[10]));

  for (unsigned int i = 0; i < batch_boundary_; ++i) {
    auto individual_message_publish_response = publish_message_futures[i].get();
    EXPECT_TRUE(individual_message_publish_response);
    EXPECT_EQ(individual_message_publish_response->offset(), i);
  }

  underlying_stream_ = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream_,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream_, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(absl::WrapUnique(underlying_stream_));

  write_promise.set_value(false);

  auto temp_promise = std::move(read_promise1_);
  read_promise1_ = promise<absl::optional<PublishResponse>>{};
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(Return(ByMove(read_promise1_.get_future())));
  temp_promise.set_value(absl::optional<PublishResponse>());

  for (unsigned int i = 5; i < individual_publish_messages.size();) {
    PublishRequest publish_request1;
    unsigned int orig_i = i;
    for (;
         i < orig_i + batch_boundary_ && i < individual_publish_messages.size();
         ++i) {
      *publish_request1.mutable_message_publish_request()->add_messages() =
          individual_publish_messages[i];
    }
    EXPECT_CALL(*resumable_stream_,
                Write(IsProtoEqual(std::move(publish_request1))))
        .WillOnce(Return(ByMove(make_ready_future(true))));
  }

  leaked_alarm_();

  for (unsigned int i = 5; i < individual_publish_messages.size();
       i += batch_boundary_) {
    PublishResponse publish_response1;
    publish_response1.mutable_message_response()
        ->mutable_start_cursor()
        ->set_offset(i);
    auto temp_read_promise = std::move(read_promise1_);
    read_promise1_ = promise<absl::optional<PublishResponse>>{};
    EXPECT_CALL(*resumable_stream_, Read)
        .WillOnce(Return(ByMove(read_promise1_.get_future())));
    temp_read_promise.set_value(std::move(publish_response1));
  }

  for (unsigned int i = 5; i < individual_publish_messages.size(); ++i) {
    auto individual_message_publish_response = publish_message_futures[i].get();
    EXPECT_TRUE(individual_message_publish_response);
    EXPECT_EQ(individual_message_publish_response->offset(), i);
  }
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

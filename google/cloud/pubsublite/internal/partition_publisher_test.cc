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
#include "google/cloud/pubsublite/testing/mock_alarm.h"
#include "google/cloud/pubsublite/testing/mock_alarm_token.h"
#include "google/cloud/pubsublite/testing/mock_async_reader_writer.h"
#include "google/cloud/pubsublite/testing/mock_resumable_async_reader_writer_stream.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <chrono>
#include <memory>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {
namespace {

using google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::_;
using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

using google::cloud::pubsublite::BatchingOptions;
using google::cloud::pubsublite::v1::Cursor;
using google::cloud::pubsublite::v1::InitialPublishRequest;
using google::cloud::pubsublite::v1::InitialPublishResponse;
using google::cloud::pubsublite::v1::MessagePublishRequest;
using google::cloud::pubsublite::v1::MessagePublishResponse;
using google::cloud::pubsublite::v1::PublishRequest;
using google::cloud::pubsublite::v1::PublishResponse;
using google::cloud::pubsublite::v1::PubSubMessage;

using ::google::cloud::pubsublite_testing::MockAlarm;
using ::google::cloud::pubsublite_testing::MockAsyncReaderWriter;
using ::google::cloud::pubsublite_testing::MockResumableAsyncReaderWriter;
using ::google::cloud::pubsublite_testing::MockToken;

using AsyncReaderWriter =
    MockAsyncReaderWriter<PublishRequest, PublishResponse>;

using AsyncReadWriteStreamReturnType = std::unique_ptr<
    AsyncStreamingReadWriteRpc<PublishRequest, PublishResponse>>;

using ResumableAsyncReadWriteStream = std::unique_ptr<
    ResumableAsyncStreamingReadWriteRpc<PublishRequest, PublishResponse>>;

auto const kAlarmDuration = std::chrono::milliseconds{10};

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

class PartitionPublisherTest : public ::testing::Test {
 protected:
  PartitionPublisherTest() {
    EXPECT_CALL(alarm_, RegisterAlarm(kAlarmDuration, _))
        .WillOnce(WithArg<1>([&](std::function<void()> on_alarm) {
          leaked_alarm_ = std::move(on_alarm);
          return absl::WrapUnique(alarm_token_);
        }));

    BatchingOptions options;
    options.set_maximum_batch_message_count(5);

    publisher_ = absl::make_unique<PartitionPublisherImpl>(
        absl::FunctionRef<std::unique_ptr<ResumableAsyncStreamingReadWriteRpc<
            PublishRequest, PublishResponse>>(
            StreamInitializer<PublishRequest, PublishResponse>)>(
            [&](StreamInitializer<PublishRequest, PublishResponse> const&
                    initializer) {
              initializer_ = std::move(initializer);
              return absl::WrapUnique(resumable_stream_);
            }),
        std::move(options), InitialPublishRequest::default_instance(), alarm_);
  }

  StreamInitializer<PublishRequest, PublishResponse> initializer_;
  MockToken* alarm_token_ = new StrictMock<MockToken>;
  MockAlarm alarm_;
  std::function<void()> leaked_alarm_;
  MockResumableAsyncReaderWriter<PublishRequest, PublishResponse>*
      resumable_stream_ = new StrictMock<
          MockResumableAsyncReaderWriter<PublishRequest, PublishResponse>>;
  std::unique_ptr<Publisher<Cursor>> publisher_;
};

TEST_F(PartitionPublisherTest, SinglePublishGood) {
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

  PublishResponse publish_response1;
  publish_response1.mutable_message_response()
      ->mutable_start_cursor()
      ->set_offset(0);
  promise<absl::optional<PublishResponse>> read_promise1;
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(Return(ByMove(read_promise1.get_future())));
  read_promise.set_value(std::move(publish_response1));

  auto individual_message_publish_response = publish_future.get();
  EXPECT_TRUE(individual_message_publish_response);
  EXPECT_EQ(individual_message_publish_response->offset(), 0);

  // shouldn't do anything b/c no messages left
  publisher_->Flush();

  EXPECT_CALL(*alarm_token_, Destroy);
  EXPECT_CALL(*resumable_stream_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();
  read_promise1.set_value(absl::optional<PublishResponse>());
  start_promise.set_value(Status());
  EXPECT_EQ(publisher_start_future.get(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
}

TEST_F(PartitionPublisherTest, SinglePublishGoodThroughFlush) {
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

  publisher_->Flush();

  PublishResponse publish_response1;
  publish_response1.mutable_message_response()
      ->mutable_start_cursor()
      ->set_offset(0);
  promise<absl::optional<PublishResponse>> read_promise1;
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(Return(ByMove(read_promise1.get_future())));
  read_promise.set_value(std::move(publish_response1));

  auto individual_message_publish_response = publish_future.get();
  EXPECT_TRUE(individual_message_publish_response);
  EXPECT_EQ(individual_message_publish_response->offset(), 0);

  // shouldn't do anything b/c no messages left
  leaked_alarm_();

  EXPECT_CALL(*alarm_token_, Destroy);
  EXPECT_CALL(*resumable_stream_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();
  read_promise1.set_value(absl::optional<PublishResponse>());
  start_promise.set_value(Status());
  EXPECT_EQ(publisher_start_future.get(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
}

TEST_F(PartitionPublisherTest, InFlightBatchAndUnsentMessageThenRetry) {
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

  underlying_stream = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(absl::WrapUnique(underlying_stream));

  promise<absl::optional<PublishResponse>> read_promise1;
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(Return(ByMove(read_promise1.get_future())));
  read_promise.set_value(absl::optional<PublishResponse>());

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
  promise<absl::optional<PublishResponse>> read_promise2;
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(Return(ByMove(read_promise2.get_future())));
  read_promise1.set_value(std::move(publish_response1));

  for (unsigned int i = 0; i < 3; ++i) {
    auto individual_message_publish_response = publish_message_futures[i].get();
    EXPECT_TRUE(individual_message_publish_response);
    EXPECT_EQ(individual_message_publish_response->offset(), i);
  }

  // shouldn't do anything b/c no messages left
  leaked_alarm_();

  EXPECT_CALL(*alarm_token_, Destroy);
  EXPECT_CALL(*resumable_stream_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();
  read_promise2.set_value(absl::optional<PublishResponse>());
  start_promise.set_value(Status());
  EXPECT_EQ(publisher_start_future.get(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
}

TEST_F(PartitionPublisherTest, InFlightBatchUnsentBatchUnsentMessageThenRetry) {
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
  for (unsigned int i = 0; i < 10; ++i) {
    publish_message_futures.push_back(
        publisher_->Publish(individual_publish_messages[i]));
  }

  PublishRequest publish_request;
  for (unsigned int i = 0; i < 5; ++i) {
    *publish_request.mutable_message_publish_request()->add_messages() =
        individual_publish_messages[i];
  }

  promise<bool> write_promise;
  EXPECT_CALL(*resumable_stream_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(write_promise.get_future())));

  leaked_alarm_();

  publish_message_futures.push_back(
      publisher_->Publish(individual_publish_messages[10]));

  underlying_stream = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(absl::WrapUnique(underlying_stream));

  write_promise.set_value(false);

  promise<absl::optional<PublishResponse>> read_promise1;
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(Return(ByMove(read_promise1.get_future())));
  read_promise.set_value(absl::optional<PublishResponse>());

  for (unsigned int i = 0; i < individual_publish_messages.size();) {
    PublishRequest publish_request1;
    unsigned int orig_i = i;
    for (; i < orig_i + 5 && i < individual_publish_messages.size(); ++i) {
      *publish_request1.mutable_message_publish_request()->add_messages() =
          individual_publish_messages[i];
    }
    EXPECT_CALL(*resumable_stream_,
                Write(IsProtoEqual(std::move(publish_request1))))
        .WillOnce(Return(ByMove(make_ready_future(true))));
  }

  leaked_alarm_();

  for (unsigned int i = 0; i < individual_publish_messages.size(); i += 5) {
    PublishResponse publish_response1;
    publish_response1.mutable_message_response()
        ->mutable_start_cursor()
        ->set_offset(i);
    auto temp_read_promise = std::move(read_promise1);
    read_promise1 = promise<absl::optional<PublishResponse>>{};
    EXPECT_CALL(*resumable_stream_, Read)
        .WillOnce(Return(ByMove(read_promise1.get_future())));
    temp_read_promise.set_value(std::move(publish_response1));
  }

  for (unsigned int i = 0; i < individual_publish_messages.size(); ++i) {
    auto individual_message_publish_response = publish_message_futures[i].get();
    EXPECT_TRUE(individual_message_publish_response);
    EXPECT_EQ(individual_message_publish_response->offset(), i);
  }

  // shouldn't do anything b/c no messages left
  publisher_->Flush();

  EXPECT_CALL(*alarm_token_, Destroy);
  EXPECT_CALL(*resumable_stream_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();
  read_promise1.set_value(absl::optional<PublishResponse>());
  start_promise.set_value(Status());
  EXPECT_EQ(publisher_start_future.get(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
}

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
  for (unsigned int i = 0; i < 10; ++i) {
    publish_message_futures.push_back(
        publisher_->Publish(individual_publish_messages[i]));
  }

  PublishRequest publish_request;
  for (unsigned int i = 0; i < 5; ++i) {
    *publish_request.mutable_message_publish_request()->add_messages() =
        individual_publish_messages[i];
  }

  promise<bool> write_promise;
  EXPECT_CALL(*resumable_stream_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(write_promise.get_future())));

  leaked_alarm_();

  publish_message_futures.push_back(
      publisher_->Publish(individual_publish_messages[10]));

  underlying_stream = new StrictMock<AsyncReaderWriter>;
  EXPECT_CALL(*underlying_stream,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(absl::WrapUnique(underlying_stream));

  write_promise.set_value(false);

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

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

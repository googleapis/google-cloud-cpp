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
using google::protobuf::Message;

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

const Status kFailStatus = Status(StatusCode::kUnavailable, "Unavailable");

Status FailStatus() { return Status(StatusCode::kUnavailable, "Unavailable"); }

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

    publisher_ = absl::make_unique<PartitionPublisherImpl>(
        absl::FunctionRef<std::unique_ptr<ResumableAsyncStreamingReadWriteRpc<
            PublishRequest, PublishResponse>>(
            StreamInitializer<PublishRequest, PublishResponse>)>(
            [&](StreamInitializer<PublishRequest, PublishResponse> const&
                    initializer) {
              initializer_ = std::move(initializer);
              return absl::WrapUnique(resumable_stream_);
            }),
        BatchingOptions(), InitialPublishRequest::default_instance(), alarm_);
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
  MessagePublishRequest& message_publish_request =
      *publish_request.mutable_message_publish_request();
  *message_publish_request.add_messages() = PubSubMessage::default_instance();
  EXPECT_CALL(*resumable_stream_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(make_ready_future(true))));

  leaked_alarm_();

  Cursor cursor;
  cursor.set_offset(0);
  MessagePublishResponse message_publish_response;
  *message_publish_response.mutable_start_cursor() = cursor;
  PublishResponse publish_response1;
  *publish_response1.mutable_message_response() = message_publish_response;
  promise<absl::optional<PublishResponse>> read_promise1;
  EXPECT_CALL(*resumable_stream_, Read)
      .WillOnce(Return(ByMove(read_promise1.get_future())));
  read_promise.set_value(std::move(publish_response1));

  auto individual_message_publish_response = publish_future.get();
  EXPECT_TRUE(individual_message_publish_response);
  EXPECT_EQ(individual_message_publish_response->offset(), 0);

  EXPECT_CALL(*alarm_token_, Destroy);
  EXPECT_CALL(*resumable_stream_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();
  read_promise1.set_value(absl::optional<PublishResponse>());
  start_promise.set_value(Status());
  EXPECT_EQ(publisher_start_future.get(),
            Status(StatusCode::kAborted, "`Shutdown` called"));
}

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

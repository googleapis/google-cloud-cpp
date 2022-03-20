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
#include "google/cloud/pubsublite/testing/mock_async_reader_writer.h"
#include "google/cloud/pubsublite/testing/mock_resumable_async_reader_writer_stream.h"
#include "google/cloud/future.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <chrono>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {

using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::_;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

using google::cloud::pubsublite::v1::Cursor;
using google::cloud::pubsublite::v1::InitialPublishRequest;
using google::cloud::pubsublite::v1::InitialPublishResponse;

using google::cloud::pubsublite::v1::Cursor;
using google::cloud::pubsublite::v1::PublishRequest;
using google::cloud::pubsublite::v1::PublishResponse;
using google::cloud::pubsublite::v1::PubSubMessage;

using ::google::cloud::pubsublite_testing::MockAlarmRegistry;
using ::google::cloud::pubsublite_testing::MockAlarmRegistryCancelToken;
using ::google::cloud::pubsublite_testing::MockAsyncReaderWriter;
using ::google::cloud::pubsublite_testing::MockResumableAsyncReaderWriter;

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

  using MessagePromisePair =
      std::pair<PubSubMessage, promise<StatusOr<Cursor>>>;

  static std::deque<std::deque<MessagePromisePair>> TestCreateBatches(
      std::deque<MessagePromisePair> messages, BatchingOptions const& options) {
    std::deque<PartitionPublisher::MessageWithPromise> message_with_promises;
    for (auto& message_with_future : messages) {
      message_with_promises.emplace_back(PartitionPublisher::MessageWithPromise{
          std::move(message_with_future.first),
          std::move(message_with_future.second)});
    }
    auto batches = PartitionPublisher::CreateBatches(
        std::move(message_with_promises), std::move(options));
    std::deque<std::deque<MessagePromisePair>> ret_batches;
    for (auto& batch : batches) {
      std::deque<MessagePromisePair> message_batch;
      for (auto& message_with_future : batch) {
        message_batch.emplace_back(
            MessagePromisePair{std::move(message_with_future.message),
                               std::move(message_with_future.message_promise)});
      }
      ret_batches.push_back(std::move(message_batch));
    }
    return ret_batches;
  }

  static std::vector<PubSubMessage> GetMessagesFromBatch(
      std::deque<MessagePromisePair> const& batch) {
    std::vector<PubSubMessage> ret;
    std::transform(batch.begin(), batch.end(), std::back_inserter(ret),
                   [](MessagePromisePair const& message) {
                     return std::move(message.first);
                   });
    return ret;
  }
};

// batching logic
TEST_F(PartitionPublisherBatchingTest, SingleMessageBatch) {
  std::deque<MessagePromisePair> message_with_promises;
  std::deque<PubSubMessage> messages;
  unsigned int const num_messages = 10;
  for (unsigned int i = 0; i < num_messages; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    promise<StatusOr<Cursor>> message_promise;
    message_promise.get_future().then([i](future<StatusOr<Cursor>> f) {
      auto status = f.get();
      EXPECT_FALSE(status.ok());
      EXPECT_EQ(status.status(),
                Status(StatusCode::kUnavailable, std::to_string(i)));
    });
    message_with_promises.emplace_back(
        MessagePromisePair{message, std::move(message_promise)});
    messages.push_back(std::move(message));
  }
  BatchingOptions options;
  options.set_maximum_batch_message_count(1);

  auto batches =
      TestCreateBatches(std::move(message_with_promises), std::move(options));
  EXPECT_EQ(messages.size(), num_messages);
  EXPECT_EQ(batches.size(), num_messages);
  for (unsigned int i = 0; i < num_messages; ++i) {
    auto& batch = batches[i];
    EXPECT_EQ(batch.size(), 1);
    EXPECT_THAT(batch[0].first, IsProtoEqual(messages[i]));
    batch[0].second.set_value(
        StatusOr<Cursor>{Status(StatusCode::kUnavailable, std::to_string(i))});
  }
}

TEST_F(PartitionPublisherBatchingTest,
       SingleMessageBatchMessageSizeRestriction) {
  std::deque<MessagePromisePair> message_with_promises;
  std::deque<PubSubMessage> messages;
  unsigned int const num_messages = 10;
  for (unsigned int i = 0; i < num_messages; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    promise<StatusOr<Cursor>> message_promise;
    message_promise.get_future().then([i](future<StatusOr<Cursor>> f) {
      auto status = f.get();
      EXPECT_FALSE(status.ok());
      EXPECT_EQ(status.status(),
                Status(StatusCode::kUnavailable, std::to_string(i)));
    });
    message_with_promises.emplace_back(
        MessagePromisePair{message, std::move(message_promise)});
    messages.push_back(std::move(message));
  }
  BatchingOptions options;
  options.set_maximum_batch_bytes(1);

  auto batches =
      TestCreateBatches(std::move(message_with_promises), std::move(options));
  EXPECT_EQ(messages.size(), num_messages);
  EXPECT_EQ(batches.size(), num_messages);
  for (unsigned int i = 0; i < num_messages; ++i) {
    auto& batch = batches[i];
    EXPECT_EQ(batch.size(), 1);
    EXPECT_THAT(batch[0].first, IsProtoEqual(messages[i]));
    batch[0].second.set_value(
        StatusOr<Cursor>{Status(StatusCode::kUnavailable, std::to_string(i))});
  }
}

TEST_F(PartitionPublisherBatchingTest, FullAndPartialBatches) {
  std::deque<MessagePromisePair> message_with_promises;
  std::deque<PubSubMessage> messages;
  unsigned int const num_messages = 10;
  unsigned int const max_batch_message_count = 3;
  for (unsigned int i = 0; i < num_messages; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    promise<StatusOr<Cursor>> message_promise;
    message_promise.get_future().then([i](future<StatusOr<Cursor>> f) {
      auto status = f.get();
      EXPECT_FALSE(status.ok());
      EXPECT_EQ(status.status(),
                Status(StatusCode::kUnavailable,
                       absl::StrCat("batch:",
                                    std::to_string(i / max_batch_message_count),
                                    "offset:", std::to_string(i))));
    });
    message_with_promises.emplace_back(
        MessagePromisePair{message, std::move(message_promise)});
    messages.push_back(std::move(message));
  }
  BatchingOptions options;

  options.set_maximum_batch_message_count(max_batch_message_count);

  auto batches =
      TestCreateBatches(std::move(message_with_promises), std::move(options));
  EXPECT_EQ(batches.size(),
            std::ceil(num_messages / ((double)max_batch_message_count)));
  EXPECT_THAT(GetMessagesFromBatch(batches[0]),
              ElementsAre(IsProtoEqual(messages[0]), IsProtoEqual(messages[1]),
                          IsProtoEqual(messages[2])));
  EXPECT_THAT(GetMessagesFromBatch(batches[1]),
              ElementsAre(IsProtoEqual(messages[3]), IsProtoEqual(messages[4]),
                          IsProtoEqual(messages[5])));
  EXPECT_THAT(GetMessagesFromBatch(batches[2]),
              ElementsAre(IsProtoEqual(messages[6]), IsProtoEqual(messages[7]),
                          IsProtoEqual(messages[8])));
  EXPECT_THAT(GetMessagesFromBatch(batches[3]),
              ElementsAre(IsProtoEqual(messages[9])));
  for (unsigned int i = 0; i < batches.size(); ++i) {
    auto& batch = batches[i];
    for (unsigned int j = 0; j < batch.size(); ++j) {
      batch[j].second.set_value(StatusOr<Cursor>{Status(
          StatusCode::kUnavailable,
          absl::StrCat("batch:", std::to_string(i), "offset:",
                       std::to_string(i * max_batch_message_count + j)))});
    }
  }
}

TEST_F(PartitionPublisherBatchingTest, FullBatchesMessageSizeRestriction) {
  std::deque<MessagePromisePair> message_with_promises;
  std::deque<PubSubMessage> messages;
  unsigned int const num_messages = 9;
  // all messages are of same size so <message size> * 3 is enough for 3
  // messages
  unsigned int const max_batch_message_count = 3;
  for (unsigned int i = 0; i < num_messages; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    promise<StatusOr<Cursor>> message_promise;
    message_promise.get_future().then([i](future<StatusOr<Cursor>> f) {
      auto status = f.get();
      EXPECT_FALSE(status.ok());
      EXPECT_EQ(status.status(),
                Status(StatusCode::kUnavailable,
                       absl::StrCat("batch:",
                                    std::to_string(i / max_batch_message_count),
                                    "offset:", std::to_string(i))));
    });
    message_with_promises.emplace_back(
        MessagePromisePair{message, std::move(message_promise)});
    messages.push_back(std::move(message));
  }
  BatchingOptions options;

  options.set_maximum_batch_bytes(messages[0].ByteSizeLong() *
                                  max_batch_message_count);

  auto batches =
      TestCreateBatches(std::move(message_with_promises), std::move(options));
  EXPECT_EQ(batches.size(),
            std::ceil(num_messages / ((double)max_batch_message_count)));
  EXPECT_THAT(GetMessagesFromBatch(batches[0]),
              ElementsAre(IsProtoEqual(messages[0]), IsProtoEqual(messages[1]),
                          IsProtoEqual(messages[2])));
  EXPECT_THAT(GetMessagesFromBatch(batches[1]),
              ElementsAre(IsProtoEqual(messages[3]), IsProtoEqual(messages[4]),
                          IsProtoEqual(messages[5])));
  EXPECT_THAT(GetMessagesFromBatch(batches[2]),
              ElementsAre(IsProtoEqual(messages[6]), IsProtoEqual(messages[7]),
                          IsProtoEqual(messages[8])));
  for (unsigned int i = 0; i < batches.size(); ++i) {
    auto& batch = batches[i];
    for (unsigned int j = 0; j < batch.size(); ++j) {
      batch[j].second.set_value(StatusOr<Cursor>{Status(
          StatusCode::kUnavailable,
          absl::StrCat("batch:", std::to_string(i), "offset:",
                       std::to_string(i * max_batch_message_count + j)))});
    }
  }
}

class PartitionPublisherTest : public ::testing::Test {
 protected:
  PartitionPublisherTest()
      : alarm_token_ref_{*alarm_token_},
        resumable_stream_ref_{*resumable_stream_} {
    EXPECT_CALL(alarm_registry_, RegisterAlarm(kAlarmDuration, _))
        .WillOnce(WithArg<1>([&](std::function<void()> on_alarm) {
          // as this is a unit test, we mock the AlarmRegistry behavior
          // this enables the test suite to control when messages are flushed
          on_alarm_ = std::move(on_alarm);
          return std::move(alarm_token_);
        }));

    BatchingOptions options;
    options.set_maximum_batch_message_count(kBatchBoundary_);
    options.set_alarm_period(kAlarmDuration);

    publisher_ = absl::make_unique<PartitionPublisher>(
        [&](StreamInitializer<PublishRequest, PublishResponse> initializer) {
          // as this is a unit test, we mock the resumable stream behavior
          // this enables the test suite to control when underlying streams are
          // initialized
          initializer_ = std::move(initializer);
          return std::move(resumable_stream_);
        },
        std::move(options), InitialPublishRequest::default_instance(),
        alarm_registry_);
  }

  unsigned int const kBatchBoundary_ = 5;
  StreamInitializer<PublishRequest, PublishResponse> initializer_;
  std::unique_ptr<StrictMock<MockAlarmRegistryCancelToken>> alarm_token_ =
      absl::make_unique<StrictMock<MockAlarmRegistryCancelToken>>();
  // we use *ref_ to maintain access to the underlying object of `std::move`d
  // `std::unique_ptr<>`s to call `EXPECT_CALL` on them at various points in
  // different test cases
  StrictMock<MockAlarmRegistryCancelToken>& alarm_token_ref_;
  MockAlarmRegistry alarm_registry_;
  std::function<void()> on_alarm_;
  std::unique_ptr<
      MockResumableAsyncReaderWriter<PublishRequest, PublishResponse>>
      resumable_stream_ = absl::make_unique<StrictMock<
          MockResumableAsyncReaderWriter<PublishRequest, PublishResponse>>>();
  MockResumableAsyncReaderWriter<PublishRequest, PublishResponse>&
      resumable_stream_ref_;
  std::unique_ptr<StrictMock<AsyncReaderWriter>> underlying_stream_ =
      absl::make_unique<StrictMock<AsyncReaderWriter>>();
  std::unique_ptr<Publisher<Cursor>> publisher_;
};

TEST_F(PartitionPublisherTest, SatisfyOutstandingMessages) {
  InSequence seq;

  promise<Status> start_promise;
  EXPECT_CALL(resumable_stream_ref_, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  promise<absl::optional<PublishResponse>> read_promise;
  // first `Read` response is nullopt because resumable stream in retry loop
  EXPECT_CALL(resumable_stream_ref_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))))
      .WillOnce(Return(ByMove(read_promise.get_future())));

  future<Status> publisher_start_future = publisher_->Start();

  EXPECT_CALL(*underlying_stream_,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream_, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(std::move(underlying_stream_));

  std::vector<PubSubMessage> individual_publish_messages;
  unsigned int message_count =
      2 * kBatchBoundary_ + 1;  // We want two full batches and a partial one.
  for (unsigned int i = 0; i != message_count; ++i) {
    PubSubMessage message;
    *message.mutable_key() = "key";
    *message.mutable_data() = std::to_string(i);
    individual_publish_messages.push_back(std::move(message));
  }

  std::vector<future<StatusOr<Cursor>>> publish_message_futures;
  std::transform(
      individual_publish_messages.begin(), individual_publish_messages.end(),
      std::back_inserter(publish_message_futures),
      [this](PubSubMessage m) { return publisher_->Publish(std::move(m)); });

  EXPECT_CALL(alarm_token_ref_, Destroy);
  EXPECT_CALL(resumable_stream_ref_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  auto shutdown_future = publisher_->Shutdown();
  start_promise.set_value(Status());
  read_promise.set_value(absl::optional<PublishResponse>());
  shutdown_future.get();

  for (auto& publish_message_resp : publish_message_futures) {
    auto individual_message_publish_response = publish_message_resp.get();
    EXPECT_FALSE(individual_message_publish_response);
    EXPECT_EQ(individual_message_publish_response.status(),
              Status(StatusCode::kAborted, "`Shutdown` called"));
  }

  // shouldn't do anything b/c shutdown
  publisher_->Flush();
  EXPECT_EQ(publisher_start_future.get(), Status());
}

TEST_F(PartitionPublisherTest, InvalidReadResponse) {
  InSequence seq;

  promise<Status> start_promise;
  EXPECT_CALL(resumable_stream_ref_, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  promise<absl::optional<PublishResponse>> read_promise;
  // first `Read` response is nullopt because resumable stream in retry loop
  EXPECT_CALL(resumable_stream_ref_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))))
      .WillOnce(Return(ByMove(read_promise.get_future())));

  future<Status> publisher_start_future = publisher_->Start();

  EXPECT_CALL(*underlying_stream_,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream_, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(std::move(underlying_stream_));

  future<StatusOr<Cursor>> publish_future =
      publisher_->Publish(PubSubMessage::default_instance());

  PublishRequest publish_request;
  *publish_request.mutable_message_publish_request()->add_messages() =
      PubSubMessage::default_instance();
  EXPECT_CALL(resumable_stream_ref_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(make_ready_future(true))));

  on_alarm_();

  read_promise.set_value(absl::make_optional(GetInitializerPublishResponse()));

  EXPECT_EQ(
      publisher_start_future.get(),
      Status(StatusCode::kAborted,
             absl::StrCat("Invalid `Read` response: ",
                          GetInitializerPublishResponse().DebugString())));

  // shouldn't do anything b/c lifecycle ended
  publisher_->Flush();

  EXPECT_CALL(alarm_token_ref_, Destroy);
  EXPECT_CALL(resumable_stream_ref_, Shutdown)
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
  EXPECT_CALL(resumable_stream_ref_, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  promise<absl::optional<PublishResponse>> read_promise;
  // first `Read` response is nullopt because resumable stream in retry loop
  EXPECT_CALL(resumable_stream_ref_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))))
      .WillOnce(Return(ByMove(read_promise.get_future())));

  future<Status> publisher_start_future = publisher_->Start();

  EXPECT_CALL(*underlying_stream_,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream_, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(std::move(underlying_stream_));

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

  EXPECT_CALL(alarm_token_ref_, Destroy);
  EXPECT_CALL(resumable_stream_ref_, Shutdown)
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
  EXPECT_CALL(resumable_stream_ref_, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  promise<absl::optional<PublishResponse>> read_promise;
  // first `Read` response is nullopt because resumable stream in retry loop
  EXPECT_CALL(resumable_stream_ref_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))))
      .WillOnce(Return(ByMove(read_promise.get_future())));

  future<Status> publisher_start_future = publisher_->Start();

  EXPECT_CALL(*underlying_stream_,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream_, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(std::move(underlying_stream_));

  EXPECT_CALL(alarm_token_ref_, Destroy);
  EXPECT_CALL(resumable_stream_ref_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();
  read_promise.set_value(absl::optional<PublishResponse>());
  start_promise.set_value(Status());
  EXPECT_EQ(publisher_start_future.get(), Status());

  auto publish_future = publisher_->Publish(PubSubMessage::default_instance());
  auto invalid_publish_response = publish_future.get();
  EXPECT_FALSE(invalid_publish_response.ok());
  EXPECT_EQ(invalid_publish_response.status(),
            Status(StatusCode::kAborted, "Already shut down."));
}

TEST_F(PartitionPublisherTest, InitializerWriteFailureThenGood) {
  InSequence seq;

  promise<Status> start_promise;
  EXPECT_CALL(resumable_stream_ref_, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  promise<absl::optional<PublishResponse>> read_promise;
  // first `Read` response is nullopt because resumable stream in retry loop
  EXPECT_CALL(resumable_stream_ref_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))))
      .WillOnce(Return(ByMove(read_promise.get_future())));

  future<Status> publisher_start_future = publisher_->Start();

  EXPECT_CALL(*underlying_stream_,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(false))));
  EXPECT_CALL(*underlying_stream_, Finish)
      .WillOnce(Return(ByMove(
          make_ready_future(Status(StatusCode::kUnavailable, "Unavailable")))));
  initializer_(std::move(underlying_stream_));

  underlying_stream_ = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*underlying_stream_,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream_, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(std::move(underlying_stream_));

  EXPECT_CALL(alarm_token_ref_, Destroy);
  EXPECT_CALL(resumable_stream_ref_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();
  start_promise.set_value(Status());
  EXPECT_EQ(publisher_start_future.get(), Status());
}

TEST_F(PartitionPublisherTest, InitializerReadFailureThenGood) {
  InSequence seq;

  promise<Status> start_promise;
  EXPECT_CALL(resumable_stream_ref_, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  promise<absl::optional<PublishResponse>> read_promise;
  // first `Read` response is nullopt because resumable stream in retry loop
  EXPECT_CALL(resumable_stream_ref_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))))
      .WillOnce(Return(ByMove(read_promise.get_future())));

  future<Status> publisher_start_future = publisher_->Start();

  EXPECT_CALL(*underlying_stream_,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))));
  EXPECT_CALL(*underlying_stream_, Finish)
      .WillOnce(Return(ByMove(
          make_ready_future(Status(StatusCode::kUnavailable, "Unavailable")))));
  initializer_(std::move(underlying_stream_));

  underlying_stream_ = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*underlying_stream_,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream_, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(std::move(underlying_stream_));

  EXPECT_CALL(alarm_token_ref_, Destroy);
  EXPECT_CALL(resumable_stream_ref_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();
  start_promise.set_value(Status());
  EXPECT_EQ(publisher_start_future.get(), Status());
}

TEST_F(PartitionPublisherTest, ResumableStreamPermanentError) {
  InSequence seq;

  promise<Status> start_promise;
  EXPECT_CALL(resumable_stream_ref_, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  promise<absl::optional<PublishResponse>> read_promise;
  // first `Read` response is nullopt because resumable stream in retry loop
  EXPECT_CALL(resumable_stream_ref_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<PublishResponse>()))))
      .WillOnce(Return(ByMove(read_promise.get_future())));

  future<Status> publisher_start_future = publisher_->Start();

  EXPECT_CALL(*underlying_stream_,
              Write(IsProtoEqual(GetInitializerPublishRequest()), _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*underlying_stream_, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(GetInitializerPublishResponse())))));
  initializer_(std::move(underlying_stream_));

  future<StatusOr<Cursor>> publish_future =
      publisher_->Publish(PubSubMessage::default_instance());

  PublishRequest publish_request;
  *publish_request.mutable_message_publish_request()->add_messages() =
      PubSubMessage::default_instance();
  EXPECT_CALL(resumable_stream_ref_, Write(IsProtoEqual(publish_request)))
      .WillOnce(Return(ByMove(make_ready_future(true))));

  on_alarm_();

  start_promise.set_value(Status(StatusCode::kInternal, "Permanent Error"));
  read_promise.set_value(absl::optional<PublishResponse>());

  EXPECT_CALL(alarm_token_ref_, Destroy);
  EXPECT_CALL(resumable_stream_ref_, Shutdown)
      .WillOnce(Return(ByMove(make_ready_future())));
  publisher_->Shutdown().get();

  auto individual_message_publish_response = publish_future.get();
  EXPECT_FALSE(individual_message_publish_response);
  EXPECT_EQ(individual_message_publish_response.status(),
            Status(StatusCode::kInternal, "Permanent Error"));
}

}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

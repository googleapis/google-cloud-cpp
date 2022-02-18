// Copyright 2020 Google LLC
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

#include "google/cloud/pubsublite/internal/resumable_async_streaming_read_write_rpc.h"
#include "google/cloud/future.h"
#include "google/cloud/pubsublite/internal/mock_async_reader_writer.h"
#include "google/cloud/pubsublite/internal/mock_retry_policy.h"
#include "google/cloud/pubsublite/internal/mock_backoff_policy.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <chrono>
#include <deque>
#include <iostream>
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::_;
using ::testing::StrictMock;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::ByMove;

using ::google::cloud::pubsublite_internal::MockRetryPolicy;
using ::google::cloud::pubsublite_internal::MockBackoffPolicy;

using ms = std::chrono::milliseconds;
const unsigned int kFutureWaitMs = 100;

struct FakeRequest {
  std::string key;
};

struct FakeResponse {
  std::string key;
  std::string value;
};

// Used as part of the EXPECT_CALL() below:
bool operator==(FakeRequest const& lhs, FakeRequest const& rhs) {
  return lhs.key == rhs.key;
}

bool operator==(FakeResponse const& lhs, FakeResponse const& rhs) {
  return lhs.key == rhs.key && lhs.value == rhs.value;
}

using AsyncReaderWriter = ::google::cloud::pubsublite_internal::MockAsyncReaderWriter<
    FakeRequest,
    FakeResponse>;

using AsyncReadWriteStreamReturnType =
std::unique_ptr<AsyncStreamingReadWriteRpc<FakeRequest, FakeResponse>>;

using ResumableAsyncReadWriteStream = std::unique_ptr<
    ResumableAsyncStreamingReadWriteRpc<FakeRequest, FakeResponse>>;

const Status kFailStatus = Status(StatusCode::kUnavailable, "Unavailable");
const FakeResponse kBasicResponse = FakeResponse{"key0", "value0_0"};
const FakeResponse kBasicResponse1 = FakeResponse{"key0", "value0_1"};
const FakeRequest kBasicRequest = FakeRequest{"key0"};

class ResumableAsyncReadWriteStreamingRpcTest : public ::testing::Test {
 protected:
  ResumableAsyncReadWriteStreamingRpcTest() : backoff_policy_{
      std::make_shared<StrictMock<MockBackoffPolicy>>()}, stream_{
      MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest, FakeResponse>(
          retry_policy_factory_.AsStdFunction(),
          backoff_policy_,
          sleeper_.AsStdFunction(),
          stream_factory_.AsStdFunction(),
          initializer_.AsStdFunction())} {
  }

  StrictMock<MockFunction<future<void>(std::chrono::duration<double>)>>
      sleeper_;
  StrictMock<MockFunction<AsyncReadWriteStreamReturnType()>> stream_factory_;
  StrictMock<MockFunction<std::unique_ptr<MockRetryPolicy>()>>
      retry_policy_factory_;
  StrictMock<MockFunction<future<StatusOr<AsyncReadWriteStreamReturnType>>(
      AsyncReadWriteStreamReturnType)>>
      initializer_;
  std::shared_ptr<StrictMock<MockBackoffPolicy>> backoff_policy_;
  ResumableAsyncReadWriteStream stream_;

  void InitializerBehavior(unsigned int times) {
    EXPECT_CALL(initializer_, Call).Times(times).WillRepeatedly([](
        AsyncReadWriteStreamReturnType stream) {
      return make_ready_future(
          StatusOr<AsyncReadWriteStreamReturnType>(std::move(stream)));
    });
  }

  void NoRetryInvocation(unsigned int times) {
    // Return(ByMove(...)) results in runtime error on times > 1 ?
    EXPECT_CALL(retry_policy_factory_,
                Call).Times(times).WillRepeatedly([]() {
      return absl::make_unique<StrictMock<
          MockRetryPolicy>>();
    });
  }

  void StandardSingleRetryPolicy() {
    auto mock_retry_policy =
        absl::make_unique<StrictMock<MockRetryPolicy>>();
    EXPECT_CALL(*mock_retry_policy, IsExhausted).WillOnce(Return(false));
    EXPECT_CALL(*mock_retry_policy, OnFailure(_)).WillOnce(Return(true));
    EXPECT_CALL(retry_policy_factory_, Call)
        .WillOnce(Return(ByMove(absl::make_unique<StrictMock<MockRetryPolicy>>())))
        .WillOnce(Return(ByMove(std::move(mock_retry_policy))));
  }

  void NoBackoffInvocation(unsigned int times) {
    EXPECT_CALL(*backoff_policy_, clone).Times(times).WillRepeatedly([]() {
      return absl::make_unique<
          StrictMock<MockBackoffPolicy>>();
    });
  }

  void StandardSingleRetryBackoffPolicy() {
    auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
    EXPECT_CALL(*backoff_policy,
                OnCompletion).WillOnce(Return(std::chrono::milliseconds(0)));
    EXPECT_CALL(*backoff_policy_,
                clone).WillOnce(Return(ByMove(absl::make_unique<
        StrictMock<MockBackoffPolicy>>()))).WillOnce(Return(ByMove(std::move(
        backoff_policy))));
  }

  void StandardSleeperBehavior(unsigned int times) {
    EXPECT_CALL(sleeper_, Call).Times(times).WillRepeatedly(Return(ByMove(
        make_ready_future())));
  }
};

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, BasicReadWriteGood) {
  auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream,
              Start).WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce(Return(ByMove(
      make_ready_future(true))));
  EXPECT_CALL(*stream, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(kBasicResponse)))))
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(kBasicResponse1)))));
  EXPECT_CALL(*stream,
              Finish).WillOnce(Return(ByMove(make_ready_future(Status()))));
  EXPECT_CALL(stream_factory_,
              Call).WillOnce(Return(ByMove(std::move(stream))));

  InitializerBehavior(1);
  NoRetryInvocation(1);
  NoBackoffInvocation(1);

  auto start = stream_->Start();
  ASSERT_TRUE(
      stream_
          ->Write(kBasicRequest, grpc::WriteOptions().set_last_message())
          .get());
  auto response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse);
  response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse1);
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  auto finish = stream_->Finish();
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, ReadWriteAfterShutdown) {
  auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream,
              Start).WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*stream,
              Finish).WillOnce(Return(ByMove(make_ready_future(Status()))));
  EXPECT_CALL(stream_factory_,
              Call).WillOnce(Return(ByMove(std::move(stream))));

  InitializerBehavior(1);
  NoRetryInvocation(1);
  NoBackoffInvocation(1);

  auto start = stream_->Start();
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  auto finish = stream_->Finish();
  finish.get();
  ASSERT_FALSE(
      stream_
          ->Write(kBasicRequest, grpc::WriteOptions().set_last_message())
          .get());
  ASSERT_FALSE(stream_->Read().get().has_value());
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, SingleReadFailureThenGood) {
  auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream, Start).WillOnce(Return(ByMove(make_ready_future(
      true))));
  EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce(Return(ByMove(
      make_ready_future(true))));
  EXPECT_CALL(*stream,
              Read).WillOnce(Return(ByMove(make_ready_future(absl::optional<
      FakeResponse>()))));
  EXPECT_CALL(*stream, Finish).WillOnce(Return(ByMove(make_ready_future(
      kFailStatus))));

  auto stream1 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream1, Start).WillOnce(Return(ByMove(make_ready_future(
      true))));
  EXPECT_CALL(*stream1, Read)
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(kBasicResponse)))))
      .WillOnce(Return(ByMove(make_ready_future(
          absl::make_optional(kBasicResponse1)))));
  EXPECT_CALL(*stream1, Finish).WillOnce(Return(ByMove(make_ready_future(
      Status()))));

  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(stream))))
      .WillOnce(Return(ByMove(std::move(stream1))));

  StandardSingleRetryPolicy();
  InitializerBehavior(2);
  StandardSingleRetryBackoffPolicy();
  StandardSleeperBehavior(1);

  auto start = stream_->Start();
  ASSERT_TRUE(
      stream_
          ->Write(kBasicRequest, grpc::WriteOptions().set_last_message())
          .get());
  auto response0 = stream_->Read().get();
  ASSERT_FALSE(response0.has_value());
  response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse);
  response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse1);
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  auto finish = stream_->Finish();
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, SingleStartFailureThenGood) {
  auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream, Start).WillOnce(Return(ByMove(make_ready_future(
      false))));
  EXPECT_CALL(*stream, Finish).WillOnce(Return(ByMove(make_ready_future(
      kFailStatus))));

  auto stream1 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream1, Start).WillOnce(Return(ByMove(make_ready_future(
      true))));
  EXPECT_CALL(*stream1, Read).WillOnce(Return(ByMove(make_ready_future(
      absl::make_optional(kBasicResponse)))));
  EXPECT_CALL(*stream1, Finish).WillOnce(Return(ByMove(make_ready_future(
      Status()))));

  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(stream))))
      .WillOnce(Return(ByMove(std::move(stream1))));

  auto mock_retry_policy =
      absl::make_unique<StrictMock<MockRetryPolicy>>();
  EXPECT_CALL(*mock_retry_policy, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(*mock_retry_policy, OnFailure(_)).WillOnce(Return(true));
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  EXPECT_CALL(*backoff_policy,
              OnCompletion).WillOnce(Return(std::chrono::milliseconds(0)));
  EXPECT_CALL(*backoff_policy_, clone).WillOnce(Return(ByMove(std::move(
      backoff_policy))));

  InitializerBehavior(1);
  StandardSleeperBehavior(1);

  auto start = stream_->Start();
  auto response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse);
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  auto finish = stream_->Finish();
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, FinishInMiddleOfReadWrite) {
  promise<bool> write_promise;
  promise<absl::optional<FakeResponse>> read_promise;

  auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream,
              Start).WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*stream, Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  EXPECT_CALL(*stream,
              Read).WillOnce(Return(ByMove(read_promise.get_future())));
  EXPECT_CALL(*stream,
              Finish).WillOnce(Return(ByMove(make_ready_future(Status()))));

  EXPECT_CALL(stream_factory_,
              Call).WillOnce(Return(ByMove(std::move(stream))));

  NoRetryInvocation(1);
  InitializerBehavior(1);
  NoBackoffInvocation(1);

  auto start = stream_->Start();
  auto write = stream_->Write(kBasicRequest,
                              grpc::WriteOptions().set_last_message());
  auto read = stream_->Read();
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  auto finish_future = stream_->Finish();
  write_promise.set_value(true);
  EXPECT_EQ(finish_future.wait_for(ms(kFutureWaitMs)),
            std::future_status::timeout);
  read_promise.set_value(absl::make_optional(kBasicResponse));
  ASSERT_FALSE(write.get());
  ASSERT_FALSE(read.get().has_value());
  finish_future.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest,
       FinishInMiddleOfRetryAfterStart) {
  promise<bool> start_promise;

  auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream, Start).WillOnce(Return(ByMove(make_ready_future(
      true))));
  EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce(Return(ByMove(
      make_ready_future(false))));
  EXPECT_CALL(*stream, Finish).WillOnce(Return(ByMove(make_ready_future(
      kFailStatus))));

  auto stream1 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream1,
              Start).WillOnce(Return(ByMove(start_promise.get_future())));
  EXPECT_CALL(*stream1, Finish).WillOnce(Return(ByMove(make_ready_future(
      Status()))));

  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(stream))))
      .WillOnce(Return(ByMove(std::move(stream1))));

  StandardSingleRetryPolicy();
  StandardSingleRetryBackoffPolicy();
  InitializerBehavior(2);
  StandardSleeperBehavior(1);

  auto start = stream_->Start();
  auto write = stream_->Write(kBasicRequest,
                              grpc::WriteOptions().set_last_message());
  auto finish = stream_->Finish();
  EXPECT_EQ(finish.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  start_promise.set_value(true);
  finish.get();
  ASSERT_FALSE(write.get());
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest,
       FinishInMiddleOfRetryBeforeStart) {
  promise<Status> finish_promise;

  auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream,
              Start).WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce(Return(ByMove(
      make_ready_future(false))));
  EXPECT_CALL(*stream,
              Finish).WillOnce(Return(ByMove(finish_promise.get_future())));

  EXPECT_CALL(stream_factory_,
              Call).WillOnce(Return(ByMove(std::move(stream))));

  NoRetryInvocation(2);
  NoBackoffInvocation(2);
  InitializerBehavior(1);

  auto start = stream_->Start();
  auto write = stream_->Write(kBasicRequest,
                              grpc::WriteOptions().set_last_message());
  auto finish = stream_->Finish();
  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  finish_promise.set_value(kFailStatus);
  finish.get();
  ASSERT_FALSE(write.get());
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest,
       FinishInMiddleOfRetryDuringSleep) {
  promise<void> sleep_promise;

  auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream,
              Start).WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce(Return(ByMove(
      make_ready_future(false))));
  EXPECT_CALL(*stream, Finish).WillOnce(Return(ByMove(make_ready_future(
      kFailStatus))));

  EXPECT_CALL(stream_factory_,
              Call).WillOnce(Return(ByMove(std::move(stream))));

  StandardSingleRetryPolicy();
  StandardSingleRetryBackoffPolicy();
  InitializerBehavior(1);
  EXPECT_CALL(sleeper_,
              Call).WillOnce(Return(ByMove(sleep_promise.get_future())));

  auto start = stream_->Start();
  auto write = stream_->Write(kBasicRequest,
                              grpc::WriteOptions().set_last_message());
  auto finish = stream_->Finish();
  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  sleep_promise.set_value();
  finish.get();
  ASSERT_FALSE(write.get());
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, FinishWhileShutdown) {
  auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream,
              Start).WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce(Return(ByMove(
      make_ready_future(false))));
  EXPECT_CALL(*stream, Finish).WillOnce(Return(ByMove(make_ready_future(
      kFailStatus))));
  EXPECT_CALL(stream_factory_,
              Call).WillOnce(Return(ByMove(std::move(stream))));

  auto mock_retry_policy =
      absl::make_unique<StrictMock<MockRetryPolicy>>();
  EXPECT_CALL(*mock_retry_policy, IsExhausted).WillOnce(Return(true));

  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(absl::make_unique<StrictMock<MockRetryPolicy>>())))
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));
  NoBackoffInvocation(2);
  InitializerBehavior(1);

  auto start = stream_->Start();
  ASSERT_FALSE(
      stream_
          ->Write(kBasicRequest, grpc::WriteOptions().set_last_message())
          .get());
  EXPECT_EQ(start.get(), kFailStatus);
  auto finish = stream_->Finish();
  finish.get();
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, ReadFailWhileWriteInFlight) {
  promise<bool> write_promise;

  auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream, Start).WillOnce(Return(ByMove(make_ready_future(
      true))));
  EXPECT_CALL(*stream, Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  EXPECT_CALL(*stream,
              Read).WillOnce(Return(ByMove(make_ready_future(absl::optional<
      FakeResponse>()))));
  EXPECT_CALL(*stream, Finish).WillOnce(Return(ByMove(make_ready_future(
      kFailStatus))));

  auto stream1 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream1, Start).WillOnce(Return(ByMove(make_ready_future(
      true))));
  EXPECT_CALL(*stream1,
              Read).WillOnce(Return(ByMove(make_ready_future(absl::make_optional(
      kBasicResponse)))));
  EXPECT_CALL(*stream1, Finish).WillOnce(Return(ByMove(make_ready_future(
      Status()))));

  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(stream))))
      .WillOnce(Return(ByMove(std::move(stream1))));

  StandardSingleRetryPolicy();
  StandardSingleRetryBackoffPolicy();
  InitializerBehavior(2);
  StandardSleeperBehavior(1);

  auto start = stream_->Start();
  auto write = stream_->Write(kBasicRequest,
                              grpc::WriteOptions().set_last_message());
  auto read = stream_->Read();
  EXPECT_EQ(read.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  write_promise.set_value(true);
  ASSERT_FALSE(read.get().has_value());
  ASSERT_TRUE(write.get());
  auto response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse);
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  auto finish = stream_->Finish();
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, WriteFailWhileReadInFlight) {
  promise<absl::optional<FakeResponse>> read_promise;

  auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream, Start).WillOnce(Return(ByMove(make_ready_future(
      true))));
  EXPECT_CALL(*stream,
              Read).WillOnce(Return(ByMove(read_promise.get_future())));
  EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce(Return(ByMove(
      make_ready_future(false))));
  EXPECT_CALL(*stream, Finish).WillOnce(Return(ByMove(make_ready_future(
      kFailStatus))));

  auto stream1 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream1, Start).WillOnce(Return(ByMove(make_ready_future(
      true))));
  EXPECT_CALL(*stream1, Write(kBasicRequest, _)).WillOnce(Return(ByMove(
      make_ready_future(true))));
  EXPECT_CALL(*stream1, Finish).WillOnce(Return(ByMove(make_ready_future(
      Status()))));

  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(stream))))
      .WillOnce(Return(ByMove(std::move(stream1))));

  StandardSingleRetryPolicy();
  StandardSingleRetryBackoffPolicy();
  InitializerBehavior(2);
  StandardSleeperBehavior(1);

  auto start = stream_->Start();
  auto read = stream_->Read();
  auto write = stream_->Write(kBasicRequest,
                              grpc::WriteOptions().set_last_message());
  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  read_promise.set_value(absl::make_optional(kBasicResponse));
  auto read_response = read.get();
  ASSERT_TRUE(read_response.has_value());
  EXPECT_EQ(*read_response, kBasicResponse);
  ASSERT_FALSE(write.get());
  write = stream_->Write(kBasicRequest,
                         grpc::WriteOptions().set_last_message());
  ASSERT_TRUE(write.get());
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  auto finish = stream_->Finish();
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest,
       StartFailsDuringRetryPermanentError) {
  auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream, Start).WillOnce(Return(ByMove(make_ready_future(
      true))));
  EXPECT_CALL(*stream,
              Read).WillOnce(Return(ByMove(make_ready_future(absl::optional<
      FakeResponse>()))));
  EXPECT_CALL(*stream, Finish).WillOnce(Return(ByMove(make_ready_future(
      kFailStatus))));

  auto stream1 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream1, Start).WillOnce(Return(ByMove(make_ready_future(
      false))));
  EXPECT_CALL(*stream1, Finish).WillOnce(Return(ByMove(make_ready_future(
      kFailStatus))));

  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(stream))))
      .WillOnce(Return(ByMove(std::move(stream1))));

  auto mock_retry_policy =
      absl::make_unique<StrictMock<MockRetryPolicy>>();
  EXPECT_CALL(*mock_retry_policy, IsExhausted)
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_retry_policy, OnFailure(_)).WillOnce(Return(true));

  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(absl::make_unique<StrictMock<MockRetryPolicy>>())))
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  StandardSingleRetryBackoffPolicy();
  InitializerBehavior(1);
  StandardSleeperBehavior(1);

  auto start = stream_->Start();
  ASSERT_FALSE(stream_->Read().get().has_value());
  EXPECT_EQ(start.get(), kFailStatus);
  auto finish = stream_->Finish();
  finish.get();
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, ReadInMiddleOfRetryAfterStart) {
  promise<bool> start_promise;

  auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream, Start).WillOnce(Return(ByMove(make_ready_future(
      true))));
  EXPECT_CALL(*stream, Write(kBasicRequest, _)).WillOnce(Return(ByMove(
      make_ready_future(false))));
  EXPECT_CALL(*stream, Finish).WillOnce(Return(ByMove(make_ready_future(
      kFailStatus))));

  auto stream1 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  EXPECT_CALL(*stream1,
              Start).WillOnce(Return(ByMove(start_promise.get_future())));
  EXPECT_CALL(*stream1, Finish).WillOnce(Return(ByMove(make_ready_future(
      Status()))));

  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(stream))))
      .WillOnce(Return(ByMove(std::move(stream1))));

  StandardSingleRetryPolicy();
  StandardSingleRetryBackoffPolicy();
  InitializerBehavior(2);
  StandardSleeperBehavior(1);

  auto start = stream_->Start();
  auto write = stream_->Write(kBasicRequest,
                              grpc::WriteOptions().set_last_message());
  auto read = stream_->Read();
  EXPECT_EQ(read.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  start_promise.set_value(true);
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  stream_->Finish().get();
  ASSERT_FALSE(write.get());
  ASSERT_FALSE(read.get().has_value());
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, WriteFinishesAfterShutdown) {
  promise<bool> write_promise;

  auto stream = absl::make_unique<StrictMock<AsyncReaderWriter>>
      ();
  EXPECT_CALL(*stream,
              Start).WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(*stream, Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  EXPECT_CALL(*stream,
              Finish).WillOnce(Return(ByMove(make_ready_future(Status()))));

  EXPECT_CALL(stream_factory_,
              Call).WillOnce(Return(ByMove(std::move(stream))));

  NoRetryInvocation(1);
  NoBackoffInvocation(1);
  InitializerBehavior(1);

  auto start = stream_->Start();
  auto write = stream_->Write(kBasicRequest,
                              grpc::WriteOptions().set_last_message());
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  auto finish = stream_->Finish();
  EXPECT_EQ(finish.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  write_promise.set_value(true);
  finish.get();
  ASSERT_FALSE(write.get());
  EXPECT_THAT(start.get(), IsOk());
}

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

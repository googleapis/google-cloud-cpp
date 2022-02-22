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
#include "google/cloud/pubsublite/internal/mock_async_reader_writer.h"
#include "google/cloud/pubsublite/internal/mock_backoff_policy.h"
#include "google/cloud/pubsublite/internal/mock_retry_policy.h"
#include "google/cloud/future.h"
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
using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

using ::google::cloud::pubsublite_internal::MockBackoffPolicy;
using ::google::cloud::pubsublite_internal::MockRetryPolicy;

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

using AsyncReaderWriter =
    ::google::cloud::pubsublite_internal::MockAsyncReaderWriter<FakeRequest,
                                                                FakeResponse>;

using AsyncReadWriteStreamReturnType =
    std::unique_ptr<AsyncStreamingReadWriteRpc<FakeRequest, FakeResponse>>;

using ResumableAsyncReadWriteStream = std::unique_ptr<
    ResumableAsyncStreamingReadWriteRpc<FakeRequest, FakeResponse>>;

const Status kFailStatus = Status(StatusCode::kUnavailable, "Unavailable");
const FakeResponse kBasicResponse = FakeResponse{"key0", "value0_0"};
const FakeResponse kBasicResponse1 = FakeResponse{"key0", "value0_1"};
const FakeRequest kBasicRequest = FakeRequest{"key0"};

StreamInitializer<FakeRequest, FakeResponse> InitializeInline() {
  return [](AsyncReadWriteStreamReturnType stream) {
    return make_ready_future(
        StatusOr<AsyncReadWriteStreamReturnType>(std::move(stream)));
  };
}

std::function<future<void>(std::chrono::duration<double>)> ZeroSleep() {
  return [](std::chrono::duration<double>) { return make_ready_future(); };
}

std::function<std::unique_ptr<MockRetryPolicy>()> ReturnUnusedRetryPolicy() {
  return []() { return absl::make_unique<StrictMock<MockRetryPolicy>>(); };
}

class ResumableAsyncReadWriteStreamingRpcTest : public ::testing::Test {
 protected:
  ResumableAsyncReadWriteStreamingRpcTest()
      : backoff_policy_{std::make_shared<StrictMock<MockBackoffPolicy>>()},
        stream_{MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest,
                                                            FakeResponse>(
            retry_policy_factory_.AsStdFunction(), backoff_policy_,
            std::make_shared<
                std::function<future<void>(std::chrono::duration<double>)>>(
                sleeper_.AsStdFunction()),
            stream_factory_.AsStdFunction(), initializer_.AsStdFunction())} {}

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
};

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, SingleStartFailureThenGood) {
  InSequence seq;

  auto stream1 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  auto& stream1_ref = *stream1;

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(stream1))));

  promise<bool> start_promise;
  EXPECT_CALL(stream1_ref, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  auto status_future = stream_->Start();

  EXPECT_CALL(stream1_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));
  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(kFailStatus)).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call).WillOnce(ZeroSleep());

  auto stream2 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  auto& stream2_ref = *stream2;
  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(stream2))));
  EXPECT_CALL(stream2_ref, Start)
      .WillOnce(Return(ByMove(make_ready_future(true))));

  EXPECT_CALL(initializer_, Call).WillOnce(InitializeInline());

  start_promise.set_value(false);

  EXPECT_CALL(stream2_ref, Read)
      .WillOnce(Return(
          ByMove(make_ready_future(absl::make_optional(kBasicResponse)))));
  auto response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse);
  EXPECT_EQ(status_future.wait_for(ms(kFutureWaitMs)),
            std::future_status::timeout);

  EXPECT_CALL(stream2_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  auto finish = stream_->Finish();
  finish.get();
  EXPECT_THAT(status_future.get(), IsOk());
}

class ResumableAsyncReadWriteStreamingRpcTestSingleInternalStream
    : public ResumableAsyncReadWriteStreamingRpcTest {
 protected:
  ResumableAsyncReadWriteStreamingRpcTestSingleInternalStream()
      : first_stream_{absl::make_unique<StrictMock<AsyncReaderWriter>>()},
        first_stream_ref_(*first_stream_) {
    InSequence seq;
    EXPECT_CALL(*backoff_policy_, clone)
        .WillOnce(
            Return(ByMove(absl::make_unique<StrictMock<MockBackoffPolicy>>())));
    EXPECT_CALL(retry_policy_factory_, Call)
        .WillOnce(ReturnUnusedRetryPolicy());
    EXPECT_CALL(stream_factory_, Call)
        .WillOnce(Return(ByMove(std::move(first_stream_))));
    EXPECT_CALL(first_stream_ref_.get(), Start)
        .WillOnce(Return(ByMove(make_ready_future(true))));
    EXPECT_CALL(initializer_, Call).WillOnce(InitializeInline());
  }
  std::unique_ptr<StrictMock<AsyncReaderWriter>> first_stream_;
  std::reference_wrapper<StrictMock<AsyncReaderWriter>> first_stream_ref_;
};

TEST_F(ResumableAsyncReadWriteStreamingRpcTestSingleInternalStream,
       BasicReadWriteGood) {
  InSequence seq;

  auto start = stream_->Start();

  EXPECT_CALL(first_stream_ref_.get(), Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  ASSERT_TRUE(
      stream_->Write(kBasicRequest, grpc::WriteOptions().set_last_message())
          .get());

  EXPECT_CALL(first_stream_ref_.get(), Read)
      .WillOnce(Return(
          ByMove(make_ready_future(absl::make_optional(kBasicResponse)))));
  auto response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse);

  EXPECT_CALL(first_stream_ref_.get(), Read)
      .WillOnce(Return(
          ByMove(make_ready_future(absl::make_optional(kBasicResponse1)))));
  response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse1);

  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(first_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  auto finish = stream_->Finish();
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTestSingleInternalStream,
       ReadWriteAfterShutdown) {
  InSequence seq;

  auto start = stream_->Start();
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(first_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  auto finish = stream_->Finish();
  finish.get();

  ASSERT_FALSE(
      stream_->Write(kBasicRequest, grpc::WriteOptions().set_last_message())
          .get());
  ASSERT_FALSE(stream_->Read().get().has_value());
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTestSingleInternalStream,
       FinishInMiddleOfReadWrite) {
  InSequence seq;

  auto start = stream_->Start();
  promise<bool> write_promise;
  promise<absl::optional<FakeResponse>> read_promise;

  EXPECT_CALL(first_stream_ref_.get(), Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  auto write =
      stream_->Write(kBasicRequest, grpc::WriteOptions().set_last_message());

  EXPECT_CALL(first_stream_ref_.get(), Read)
      .WillOnce(Return(ByMove(read_promise.get_future())));
  auto read = stream_->Read();

  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(first_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
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

TEST_F(ResumableAsyncReadWriteStreamingRpcTestSingleInternalStream,
       FinishInMiddleOfRetryBeforeStart) {
  InSequence seq;

  auto start = stream_->Start();
  promise<Status> finish_promise;
  promise<bool> write_promise;

  EXPECT_CALL(first_stream_ref_.get(), Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  auto write =
      stream_->Write(kBasicRequest, grpc::WriteOptions().set_last_message());

  EXPECT_CALL(first_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(finish_promise.get_future())));
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(
          Return(ByMove(absl::make_unique<StrictMock<MockBackoffPolicy>>())));
  EXPECT_CALL(retry_policy_factory_, Call).WillOnce(ReturnUnusedRetryPolicy());
  write_promise.set_value(false);

  auto finish = stream_->Finish();

  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  finish_promise.set_value(kFailStatus);
  ASSERT_FALSE(write.get());
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTestSingleInternalStream,
       FinishInMiddleOfRetryDuringSleep) {
  InSequence seq;

  auto start = stream_->Start();
  promise<void> sleep_promise;
  promise<bool> write_promise;

  EXPECT_CALL(first_stream_ref_.get(), Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  auto write =
      stream_->Write(kBasicRequest, grpc::WriteOptions().set_last_message());

  EXPECT_CALL(first_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(kFailStatus)).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call)
      .WillOnce(Return(ByMove(sleep_promise.get_future())));

  write_promise.set_value(false);

  auto finish = stream_->Finish();
  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  sleep_promise.set_value();
  ASSERT_FALSE(write.get());
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTestSingleInternalStream,
       FinishWhileShutdown) {
  InSequence seq;

  auto start = stream_->Start();

  promise<bool> write_promise;

  EXPECT_CALL(first_stream_ref_.get(), Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  auto write =
      stream_->Write(kBasicRequest, grpc::WriteOptions().set_last_message());

  EXPECT_CALL(first_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(
          Return(ByMove(absl::make_unique<StrictMock<MockBackoffPolicy>>())));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& mock_retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));
  EXPECT_CALL(mock_retry_policy_ref, IsExhausted).WillOnce(Return(true));

  write_promise.set_value(false);

  EXPECT_EQ(start.get(), kFailStatus);
  auto finish = stream_->Finish();
  finish.get();
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTestSingleInternalStream,
       WriteFinishesAfterShutdown) {
  InSequence seq;

  auto start = stream_->Start();
  promise<bool> write_promise;

  EXPECT_CALL(first_stream_ref_.get(), Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  auto write =
      stream_->Write(kBasicRequest, grpc::WriteOptions().set_last_message());

  EXPECT_CALL(first_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));

  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  auto finish = stream_->Finish();
  EXPECT_EQ(finish.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  write_promise.set_value(true);
  finish.get();
  ASSERT_FALSE(write.get());
  EXPECT_THAT(start.get(), IsOk());
}

class ResumableAsyncReadWriteStreamingRpcDoubleInternalStream
    : public ResumableAsyncReadWriteStreamingRpcTestSingleInternalStream {
 protected:
  ResumableAsyncReadWriteStreamingRpcDoubleInternalStream()
      : second_stream_{absl::make_unique<StrictMock<AsyncReaderWriter>>()},
        second_stream_ref_(*second_stream_) {}

  std::unique_ptr<StrictMock<AsyncReaderWriter>> second_stream_;
  std::reference_wrapper<StrictMock<AsyncReaderWriter>> second_stream_ref_;
};

TEST_F(ResumableAsyncReadWriteStreamingRpcDoubleInternalStream,
       SingleReadFailureThenGood) {
  InSequence seq;

  auto start = stream_->Start();

  EXPECT_CALL(first_stream_ref_.get(), Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  ASSERT_TRUE(
      stream_->Write(kBasicRequest, grpc::WriteOptions().set_last_message())
          .get());

  promise<absl::optional<FakeResponse>> read_promise;
  EXPECT_CALL(first_stream_ref_.get(), Read)
      .WillOnce(Return(ByMove(read_promise.get_future())));
  auto response0 = stream_->Read();

  EXPECT_CALL(first_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(kFailStatus)).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call).WillOnce(ZeroSleep());
  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(second_stream_))));
  EXPECT_CALL(second_stream_ref_.get(), Start)
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(initializer_, Call).WillOnce(InitializeInline());

  read_promise.set_value(absl::optional<FakeResponse>());
  ASSERT_FALSE(response0.get().has_value());

  EXPECT_CALL(second_stream_ref_.get(), Read)
      .WillOnce(Return(
          ByMove(make_ready_future(absl::make_optional(kBasicResponse)))));
  auto response0_opt_val = stream_->Read().get();
  ASSERT_TRUE(response0_opt_val.has_value());
  EXPECT_EQ(*response0_opt_val, kBasicResponse);

  EXPECT_CALL(second_stream_ref_.get(), Read)
      .WillOnce(Return(
          ByMove(make_ready_future(absl::make_optional(kBasicResponse1)))));
  response0_opt_val = stream_->Read().get();
  ASSERT_TRUE(response0_opt_val.has_value());
  EXPECT_EQ(*response0_opt_val, kBasicResponse1);

  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(second_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  auto finish = stream_->Finish();
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcDoubleInternalStream,
       WriteFailWhileReadInFlight) {
  InSequence seq;

  promise<absl::optional<FakeResponse>> read_promise;
  auto start = stream_->Start();

  EXPECT_CALL(first_stream_ref_.get(), Read)
      .WillOnce(Return(ByMove(read_promise.get_future())));
  auto read = stream_->Read();

  EXPECT_CALL(first_stream_ref_.get(), Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(make_ready_future(false))));
  auto write =
      stream_->Write(kBasicRequest, grpc::WriteOptions().set_last_message());
  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(first_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(kFailStatus)).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call).WillOnce(ZeroSleep());
  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(second_stream_))));
  EXPECT_CALL(second_stream_ref_.get(), Start)
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(initializer_, Call).WillOnce(InitializeInline());

  read_promise.set_value(absl::make_optional(kBasicResponse));

  auto read_response = read.get();
  ASSERT_TRUE(read_response.has_value());
  EXPECT_EQ(*read_response, kBasicResponse);
  ASSERT_FALSE(write.get());

  EXPECT_CALL(second_stream_ref_.get(), Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  write =
      stream_->Write(kBasicRequest, grpc::WriteOptions().set_last_message());
  ASSERT_TRUE(write.get());

  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(second_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  auto finish = stream_->Finish();
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcDoubleInternalStream,
       ReadFailWhileWriteInFlight) {
  InSequence seq;

  auto start = stream_->Start();
  promise<bool> write_promise;

  EXPECT_CALL(first_stream_ref_.get(), Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  auto write =
      stream_->Write(kBasicRequest, grpc::WriteOptions().set_last_message());

  EXPECT_CALL(first_stream_ref_.get(), Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<FakeResponse>()))));
  auto read = stream_->Read();
  EXPECT_EQ(read.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(first_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(kFailStatus)).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call).WillOnce(ZeroSleep());
  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(second_stream_))));
  EXPECT_CALL(second_stream_ref_.get(), Start)
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(initializer_, Call).WillOnce(InitializeInline());

  write_promise.set_value(true);

  ASSERT_FALSE(read.get().has_value());
  ASSERT_TRUE(write.get());

  EXPECT_CALL(second_stream_ref_.get(), Read)
      .WillOnce(Return(
          ByMove(make_ready_future(absl::make_optional(kBasicResponse)))));
  auto response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse);

  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(second_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));

  auto finish = stream_->Finish();
  finish.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcDoubleInternalStream,
       FinishInMiddleOfRetryAfterStart) {
  InSequence seq;

  auto start = stream_->Start();
  promise<bool> write_promise;

  EXPECT_CALL(first_stream_ref_.get(), Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  auto write =
      stream_->Write(kBasicRequest, grpc::WriteOptions().set_last_message());

  EXPECT_CALL(first_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(kFailStatus)).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call).WillOnce(ZeroSleep());
  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(second_stream_))));
  promise<bool> start_promise;
  EXPECT_CALL(second_stream_ref_.get(), Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));
  EXPECT_CALL(initializer_, Call).WillOnce(InitializeInline());

  write_promise.set_value(false);

  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  auto finish = stream_->Finish();
  EXPECT_EQ(finish.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(second_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  start_promise.set_value(true);
  finish.get();
  ASSERT_FALSE(write.get());
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcDoubleInternalStream,
       StartFailsDuringRetryPermanentError) {
  InSequence seq;
  auto start = stream_->Start();
  promise<absl::optional<FakeResponse>> read_promise;

  EXPECT_CALL(first_stream_ref_.get(), Read)
      .WillOnce(Return(ByMove(read_promise.get_future())));
  auto read = stream_->Read();

  EXPECT_CALL(first_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& mock_retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  EXPECT_CALL(mock_retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(mock_retry_policy_ref, OnFailure(_)).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call).WillOnce(ZeroSleep());
  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(second_stream_))));
  promise<bool> start_promise;
  EXPECT_CALL(second_stream_ref_.get(), Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  read_promise.set_value(absl::optional<FakeResponse>());

  EXPECT_CALL(second_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));
  EXPECT_CALL(mock_retry_policy_ref, IsExhausted).WillOnce(Return(true));

  start_promise.set_value(false);

  ASSERT_FALSE(read.get().has_value());
  EXPECT_EQ(start.get(), kFailStatus);
  auto finish = stream_->Finish();
  finish.get();
}

TEST_F(ResumableAsyncReadWriteStreamingRpcDoubleInternalStream,
       ReadInMiddleOfRetryAfterStart) {
  InSequence seq;

  auto start = stream_->Start();
  promise<bool> write_promise;

  EXPECT_CALL(first_stream_ref_.get(), Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  auto write =
      stream_->Write(kBasicRequest, grpc::WriteOptions().set_last_message());

  EXPECT_CALL(first_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(kFailStatus)).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call).WillOnce(ZeroSleep());
  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(second_stream_))));
  promise<bool> start_promise;
  EXPECT_CALL(second_stream_ref_.get(), Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));
  EXPECT_CALL(initializer_, Call).WillOnce(InitializeInline());

  write_promise.set_value(false);

  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  auto read = stream_->Read();
  EXPECT_EQ(read.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  start_promise.set_value(true);

  EXPECT_CALL(second_stream_ref_.get(), Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  ASSERT_FALSE(write.get());
  ASSERT_FALSE(read.get().has_value());
  stream_->Finish().get();
  EXPECT_THAT(start.get(), IsOk());
}

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

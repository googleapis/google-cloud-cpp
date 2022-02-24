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
#include "google/cloud/pubsublite/testing/mock_async_reader_writer.h"
#include "google/cloud/pubsublite/testing/mock_backoff_policy.h"
#include "google/cloud/pubsublite/testing/mock_retry_policy.h"
#include "google/cloud/future.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <gmock/gmock.h>
#include <chrono>
#include <deque>
#include <memory>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace pubsublite_internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

using ::google::cloud::pubsublite_testing::MockBackoffPolicy;
using ::google::cloud::pubsublite_testing::MockRetryPolicy;

using ms = std::chrono::milliseconds;
const unsigned int kFutureWaitMs = 100;

struct FakeRequest {
  std::string key;
};

struct FakeResponse {
  std::string key;
  std::string value;
};

using AsyncReaderWriter =
    ::google::cloud::pubsublite_testing::MockAsyncReaderWriter<FakeRequest,
                                                               FakeResponse>;

using AsyncReadWriteStreamReturnType =
    std::unique_ptr<AsyncStreamingReadWriteRpc<FakeRequest, FakeResponse>>;

using ResumableAsyncReadWriteStream = std::unique_ptr<
    ResumableAsyncStreamingReadWriteRpc<FakeRequest, FakeResponse>>;

const Status kFailStatus = Status(StatusCode::kUnavailable, "Unavailable");

StreamInitializer<FakeRequest, FakeResponse> InitializeInline() {
  return [](AsyncReadWriteStreamReturnType stream) {
    return make_ready_future(
        StatusOr<AsyncReadWriteStreamReturnType>(std::move(stream)));
  };
}

std::function<future<void>(std::chrono::milliseconds)> ZeroSleep() {
  return [](std::chrono::milliseconds) { return make_ready_future(); };
}

class ResumableAsyncReadWriteStreamingRpcTest : public ::testing::Test {
 protected:
  ResumableAsyncReadWriteStreamingRpcTest()
      : backoff_policy_{std::make_shared<StrictMock<MockBackoffPolicy>>()},
        stream_{MakeResumableAsyncStreamingReadWriteRpcImpl<FakeRequest,
                                                            FakeResponse>(
            retry_policy_factory_.AsStdFunction(), backoff_policy_,

            sleeper_.AsStdFunction(), stream_factory_.AsStdFunction(),
            initializer_.AsStdFunction())} {}

  StrictMock<MockFunction<future<void>(std::chrono::milliseconds)>> sleeper_;
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

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

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

  // start future doesn't finish until
  // permanent error or `Finish`is called
  EXPECT_EQ(status_future.wait_for(ms(kFutureWaitMs)),
            std::future_status::timeout);

  EXPECT_CALL(stream2_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  auto shutdown = stream_->Shutdown();
  shutdown.get();
  EXPECT_THAT(status_future.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest,
       SingleStartFailurePermanentError) {
  InSequence seq;

  auto stream1 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  auto& stream1_ref = *stream1;

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(stream1))));

  promise<bool> start_promise;
  EXPECT_CALL(stream1_ref, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  auto status_future = stream_->Start();

  EXPECT_CALL(stream1_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));
  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(kFailStatus)).WillOnce(Return(false));

  start_promise.set_value(false);

  EXPECT_EQ(status_future.get(), kFailStatus);
  auto shutdown = stream_->Shutdown();
  shutdown.get();
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest,
       SingleStartInitializerFailurePermanentError) {
  InSequence seq;

  auto stream1 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  auto& stream1_ref = *stream1;

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(stream1))));

  EXPECT_CALL(stream1_ref, Start)
      .WillOnce(Return(ByMove(make_ready_future(true))));

  promise<StatusOr<AsyncReadWriteStreamReturnType>> initializer_promise;
  EXPECT_CALL(initializer_, Call)
      .WillOnce(Return(ByMove(initializer_promise.get_future())));
  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(kFailStatus)).WillOnce(Return(false));

  auto status_future = stream_->Start();

  initializer_promise.set_value(
      StatusOr<AsyncReadWriteStreamReturnType>(kFailStatus));

  EXPECT_EQ(status_future.get(), kFailStatus);
  auto shutdown = stream_->Shutdown();
  shutdown.get();
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, InitializerFailureThenGood) {
  InSequence seq;

  auto stream1 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  auto& stream1_ref = *stream1;

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(stream1))));

  EXPECT_CALL(stream1_ref, Start)
      .WillOnce(Return(ByMove(make_ready_future(true))));
  promise<StatusOr<AsyncReadWriteStreamReturnType>> initializer_promise;
  EXPECT_CALL(initializer_, Call)
      .WillOnce(Return(ByMove(initializer_promise.get_future())));

  auto status_future = stream_->Start();

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

  initializer_promise.set_value(
      StatusOr<AsyncReadWriteStreamReturnType>(kFailStatus));

  // start future doesn't finish until
  // permanent error or `Finish`is called
  EXPECT_EQ(status_future.wait_for(ms(kFutureWaitMs)),
            std::future_status::timeout);

  EXPECT_CALL(stream2_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  auto shutdown = stream_->Shutdown();
  shutdown.get();
  EXPECT_THAT(status_future.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest,
       FinishInMiddleOfRetryAfterStart) {
  InSequence seq;

  auto stream1 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  auto& stream1_ref = *stream1;

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

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

  promise<bool> start_promise1;
  EXPECT_CALL(stream2_ref, Start)
      .WillOnce(Return(ByMove(start_promise1.get_future())));
  EXPECT_CALL(initializer_, Call).WillOnce(InitializeInline());

  start_promise.set_value(false);

  EXPECT_CALL(stream2_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  auto shutdown = stream_->Shutdown();

  // `Shutdown` shouldn't finish until retry loop terminates
  EXPECT_EQ(shutdown.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  start_promise1.set_value(true);

  shutdown.get();
  EXPECT_THAT(status_future.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest,
       FinishInMiddleOfRetryDuringSleep) {
  InSequence seq;

  auto stream1 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  auto& stream1_ref = *stream1;

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

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

  promise<void> sleep_promise;
  EXPECT_CALL(sleeper_, Call)
      .WillOnce(Return(ByMove(sleep_promise.get_future())));

  start_promise.set_value(false);

  auto shutdown = stream_->Shutdown();

  // `Shutdown` shouldn't finish until retry loop terminates
  EXPECT_EQ(shutdown.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  sleep_promise.set_value();

  shutdown.get();
  EXPECT_THAT(status_future.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest,
       FinishInMiddleOfRetryDuringInitializer) {
  InSequence seq;

  auto stream1 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  auto& stream1_ref = *stream1;

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

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

  promise<StatusOr<AsyncReadWriteStreamReturnType>> initializer_promise;
  EXPECT_CALL(initializer_, Call)
      .WillOnce(Return(ByMove(initializer_promise.get_future())));

  start_promise.set_value(false);

  auto shutdown = stream_->Shutdown();

  // `Shutdown` shouldn't finish until retry loop terminates
  EXPECT_EQ(shutdown.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  initializer_promise.set_value(
      StatusOr<AsyncReadWriteStreamReturnType>(kFailStatus));

  shutdown.get();
  EXPECT_THAT(status_future.get(), IsOk());
}

}  // namespace
}  // namespace pubsublite_internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

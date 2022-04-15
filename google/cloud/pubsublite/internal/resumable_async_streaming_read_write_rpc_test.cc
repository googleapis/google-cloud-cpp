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
namespace pubsublite_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::_;
using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

using ::google::cloud::pubsublite_testing::MockBackoffPolicy;
using ::google::cloud::pubsublite_testing::MockRetryPolicy;

using ms = std::chrono::milliseconds;
auto const kFutureWaitMs = std::chrono::milliseconds(25);

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
    ::google::cloud::pubsublite_testing::MockAsyncReaderWriter<FakeRequest,
                                                               FakeResponse>;

using AsyncReadWriteStreamReturnType =
    std::unique_ptr<AsyncStreamingReadWriteRpc<FakeRequest, FakeResponse>>;

using ResumableAsyncReadWriteStream = std::unique_ptr<
    ResumableAsyncStreamingReadWriteRpc<FakeRequest, FakeResponse>>;

const Status kFailStatus = Status(StatusCode::kUnavailable, "Unavailable");
const FakeResponse kBasicResponse = FakeResponse{"key0", "value0_0"};
const FakeResponse kBasicResponse1 = FakeResponse{"key0", "value0_1"};
const FakeRequest kBasicRequest = FakeRequest{"key0"};

Status FailStatus() { return Status(StatusCode::kUnavailable, "Unavailable"); }

StreamInitializer<FakeRequest, FakeResponse> InitializeInline() {
  return [](AsyncReadWriteStreamReturnType stream) {
    return make_ready_future(
        StatusOr<AsyncReadWriteStreamReturnType>(std::move(stream)));
  };
}

std::function<future<void>(std::chrono::milliseconds)> ZeroSleep() {
  return [](std::chrono::milliseconds) { return make_ready_future(); };
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

TEST_F(ResumableAsyncReadWriteStreamingRpcTest, NoStartDestructorGood) {}

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
      .WillOnce(Return(ByMove(make_ready_future(FailStatus()))));
  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(FailStatus())).WillOnce(Return(true));
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

  EXPECT_CALL(stream2_ref, Cancel);
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
      .WillOnce(Return(ByMove(make_ready_future(FailStatus()))));
  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(FailStatus()))
      .WillOnce(Return(false));

  start_promise.set_value(false);

  EXPECT_EQ(status_future.get(), FailStatus());
  auto shutdown = stream_->Shutdown();
  shutdown.get();
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest,
       SingleStartFailurePermanentErrorWithAsyncLoop) {
  struct Loop {
    // this should run if and only if the stream is not shutdown
    void InvokeLoop() {
      stream->Read().then([this](future<absl::optional<FakeResponse>>) {
        to_call();
        if (status_future.is_ready() && !status_future.get().ok()) return;
        InvokeLoop();
      });
    }
    ResumableAsyncReadWriteStream stream;
    std::function<void()> to_call;
    future<Status> status_future;
  };
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

  auto& stream = *stream_;
  StrictMock<MockFunction<void()>> placeholder_func;
  Loop loop{std::move(stream_), placeholder_func.AsStdFunction(),
            stream.Start()};
  loop.InvokeLoop();

  EXPECT_CALL(stream1_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(FailStatus()))));
  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(FailStatus()))
      .WillOnce(Return(false));
  EXPECT_CALL(placeholder_func, Call);

  start_promise.set_value(false);
  auto shutdown = stream.Shutdown();
  shutdown.get();
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest,
       SingleStartFailureExhaustedPermanentError) {
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
      .WillOnce(Return(ByMove(make_ready_future(FailStatus()))));
  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(true));

  start_promise.set_value(false);

  EXPECT_EQ(status_future.get(), FailStatus());
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
  EXPECT_CALL(retry_policy_ref, OnFailure(FailStatus()))
      .WillOnce(Return(false));

  auto status_future = stream_->Start();

  initializer_promise.set_value(
      StatusOr<AsyncReadWriteStreamReturnType>(FailStatus()));

  EXPECT_EQ(status_future.get(), FailStatus());
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
  EXPECT_CALL(retry_policy_ref, OnFailure(FailStatus())).WillOnce(Return(true));
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
      StatusOr<AsyncReadWriteStreamReturnType>(FailStatus()));

  // start future doesn't finish until
  // permanent error or `Finish`is called
  EXPECT_EQ(status_future.wait_for(ms(kFutureWaitMs)),
            std::future_status::timeout);

  EXPECT_CALL(stream2_ref, Cancel);
  EXPECT_CALL(stream2_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  auto shutdown = stream_->Shutdown();
  shutdown.get();
  EXPECT_THAT(status_future.get(), IsOk());
}

TEST_F(ResumableAsyncReadWriteStreamingRpcTest,
       TooManyStartFailuresPermanentError) {
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
      .WillOnce(Return(ByMove(make_ready_future(FailStatus()))));
  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(FailStatus())).WillOnce(Return(true));
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

  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(FailStatus())).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call).WillOnce(ZeroSleep());

  auto stream3 = absl::make_unique<StrictMock<AsyncReaderWriter>>();
  auto& stream3_ref = *stream3;
  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(stream3))));

  EXPECT_CALL(stream3_ref, Start)
      .WillOnce(Return(ByMove(make_ready_future(false))));

  EXPECT_CALL(stream3_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(FailStatus()))));
  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(FailStatus()))
      .WillOnce(Return(false));

  initializer_promise.set_value(
      StatusOr<AsyncReadWriteStreamReturnType>(FailStatus()));

  EXPECT_EQ(status_future.get(), FailStatus());
  auto shutdown = stream_->Shutdown();
  shutdown.get();
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
      .WillOnce(Return(ByMove(make_ready_future(FailStatus()))));
  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(FailStatus())).WillOnce(Return(true));
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
      .WillOnce(Return(ByMove(make_ready_future(FailStatus()))));
  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(FailStatus())).WillOnce(Return(true));
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
      .WillOnce(Return(ByMove(make_ready_future(FailStatus()))));
  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(FailStatus())).WillOnce(Return(true));
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
      StatusOr<AsyncReadWriteStreamReturnType>(FailStatus()));

  shutdown.get();
  EXPECT_THAT(status_future.get(), IsOk());
}

class InitializedResumableAsyncReadWriteStreamingRpcTest
    : public ResumableAsyncReadWriteStreamingRpcTest {
 protected:
  InitializedResumableAsyncReadWriteStreamingRpcTest()
      : first_stream_{absl::make_unique<StrictMock<AsyncReaderWriter>>()},
        first_stream_ref_(*first_stream_) {
    InSequence seq;
    EXPECT_CALL(retry_policy_factory_, Call)
        .WillOnce(ReturnUnusedRetryPolicy());
    EXPECT_CALL(*backoff_policy_, clone)
        .WillOnce(
            Return(ByMove(absl::make_unique<StrictMock<MockBackoffPolicy>>())));
    EXPECT_CALL(stream_factory_, Call)
        .WillOnce(Return(ByMove(std::move(first_stream_))));
    EXPECT_CALL(first_stream_ref_, Start)
        .WillOnce(Return(ByMove(make_ready_future(true))));
    EXPECT_CALL(initializer_, Call).WillOnce(InitializeInline());
  }
  std::unique_ptr<StrictMock<AsyncReaderWriter>> first_stream_;
  StrictMock<AsyncReaderWriter>& first_stream_ref_;
};

TEST_F(InitializedResumableAsyncReadWriteStreamingRpcTest, BasicReadWriteGood) {
  InSequence seq;

  auto start = stream_->Start();

  EXPECT_CALL(first_stream_ref_, Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  ASSERT_TRUE(stream_->Write(kBasicRequest).get());

  EXPECT_CALL(first_stream_ref_, Read)
      .WillOnce(Return(
          ByMove(make_ready_future(absl::make_optional(kBasicResponse)))));
  auto response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse);

  EXPECT_CALL(first_stream_ref_, Read)
      .WillOnce(Return(
          ByMove(make_ready_future(absl::make_optional(kBasicResponse1)))));
  response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse1);

  // start future doesn't finish until
  // permanent error or `Finish`
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(first_stream_ref_, Cancel);
  EXPECT_CALL(first_stream_ref_, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  auto shutdown = stream_->Shutdown();
  shutdown.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(InitializedResumableAsyncReadWriteStreamingRpcTest,
       ReadWriteAfterShutdown) {
  InSequence seq;

  auto start = stream_->Start();

  // start future doesn't finish until
  // permanent error of `Finish`
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(first_stream_ref_, Cancel);
  EXPECT_CALL(first_stream_ref_, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  auto shutdown = stream_->Shutdown();
  shutdown.get();

  ASSERT_FALSE(stream_->Write(kBasicRequest).get());
  ASSERT_FALSE(stream_->Read().get().has_value());
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(InitializedResumableAsyncReadWriteStreamingRpcTest,
       FinishInMiddleOfReadWrite) {
  InSequence seq;

  auto start = stream_->Start();
  promise<bool> write_promise;
  promise<absl::optional<FakeResponse>> read_promise;

  EXPECT_CALL(first_stream_ref_, Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  auto write = stream_->Write(kBasicRequest);

  EXPECT_CALL(first_stream_ref_, Read)
      .WillOnce(Return(ByMove(read_promise.get_future())));
  auto read = stream_->Read();

  // start future doesn't finish until
  // permanent error or `Finish`
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(first_stream_ref_, Cancel);
  EXPECT_CALL(first_stream_ref_, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  auto finish_future = stream_->Shutdown();

  write_promise.set_value(true);

  // finish future doesn't finish until
  // read and write are finished
  EXPECT_EQ(finish_future.wait_for(ms(kFutureWaitMs)),
            std::future_status::timeout);
  read_promise.set_value(absl::make_optional(kBasicResponse));

  ASSERT_FALSE(write.get());
  ASSERT_FALSE(read.get().has_value());
  finish_future.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(InitializedResumableAsyncReadWriteStreamingRpcTest,
       FinishInMiddleOfRetryBeforeStart) {
  InSequence seq;

  auto start = stream_->Start();
  promise<Status> finish_promise;
  promise<bool> write_promise;

  EXPECT_CALL(first_stream_ref_, Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  auto write = stream_->Write(kBasicRequest);

  EXPECT_CALL(first_stream_ref_, Finish)
      .WillOnce(Return(ByMove(finish_promise.get_future())));
  EXPECT_CALL(retry_policy_factory_, Call).WillOnce(ReturnUnusedRetryPolicy());
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(
          Return(ByMove(absl::make_unique<StrictMock<MockBackoffPolicy>>())));
  write_promise.set_value(false);

  auto shutdown = stream_->Shutdown();

  // write shouldn't finish until retry
  // loop done
  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  // start shouldn't finish until permanent
  // error from retry loop or `Finish`
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  finish_promise.set_value(kFailStatus);
  ASSERT_FALSE(write.get());
  shutdown.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(InitializedResumableAsyncReadWriteStreamingRpcTest,
       FinishWhileShutdown) {
  InSequence seq;

  auto start = stream_->Start();

  promise<bool> write_promise;

  EXPECT_CALL(first_stream_ref_, Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  auto write = stream_->Write(kBasicRequest);

  EXPECT_CALL(first_stream_ref_, Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& mock_retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(
          Return(ByMove(absl::make_unique<StrictMock<MockBackoffPolicy>>())));

  EXPECT_CALL(mock_retry_policy_ref, IsExhausted).WillOnce(Return(true));

  write_promise.set_value(false);

  EXPECT_EQ(start.get(), kFailStatus);
  auto shutdown = stream_->Shutdown();
  shutdown.get();
}

TEST_F(InitializedResumableAsyncReadWriteStreamingRpcTest,
       WriteFinishesAfterShutdown) {
  InSequence seq;

  auto start = stream_->Start();
  promise<bool> write_promise;

  EXPECT_CALL(first_stream_ref_, Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  auto write = stream_->Write(kBasicRequest);

  EXPECT_CALL(first_stream_ref_, Cancel);
  EXPECT_CALL(first_stream_ref_, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));

  // start shouldn't finish until permanent
  // error from retry loop or `Finish1
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  auto shutdown = stream_->Shutdown();

  // finish shouldn't finish until
  // write finishes
  EXPECT_EQ(shutdown.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  write_promise.set_value(true);
  shutdown.get();
  ASSERT_FALSE(write.get());
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(InitializedResumableAsyncReadWriteStreamingRpcTest,
       SingleReadFailureThenGood) {
  std::unique_ptr<StrictMock<AsyncReaderWriter>> second_stream{
      absl::make_unique<StrictMock<AsyncReaderWriter>>()};
  StrictMock<AsyncReaderWriter>& second_stream_ref{*second_stream};

  InSequence seq;

  auto start = stream_->Start();

  EXPECT_CALL(first_stream_ref_, Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  ASSERT_TRUE(stream_->Write(kBasicRequest).get());

  promise<absl::optional<FakeResponse>> read_promise;
  EXPECT_CALL(first_stream_ref_, Read)
      .WillOnce(Return(ByMove(read_promise.get_future())));
  auto response0 = stream_->Read();

  EXPECT_CALL(first_stream_ref_, Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(kFailStatus)).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call).WillOnce(ZeroSleep());
  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(second_stream))));
  EXPECT_CALL(second_stream_ref, Start)
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(initializer_, Call).WillOnce(InitializeInline());

  read_promise.set_value(absl::optional<FakeResponse>());
  ASSERT_FALSE(response0.get().has_value());

  EXPECT_CALL(second_stream_ref, Read)
      .WillOnce(Return(
          ByMove(make_ready_future(absl::make_optional(kBasicResponse)))));
  auto response0_opt_val = stream_->Read().get();
  ASSERT_TRUE(response0_opt_val.has_value());
  EXPECT_EQ(*response0_opt_val, kBasicResponse);

  EXPECT_CALL(second_stream_ref, Read)
      .WillOnce(Return(
          ByMove(make_ready_future(absl::make_optional(kBasicResponse1)))));
  response0_opt_val = stream_->Read().get();
  ASSERT_TRUE(response0_opt_val.has_value());
  EXPECT_EQ(*response0_opt_val, kBasicResponse1);

  // start shouldn't finish until permanent
  // error from retry loop or `Finish`
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(second_stream_ref, Cancel);
  EXPECT_CALL(second_stream_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  auto shutdown = stream_->Shutdown();
  shutdown.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(InitializedResumableAsyncReadWriteStreamingRpcTest,
       WriteFailWhileReadInFlight) {
  std::unique_ptr<StrictMock<AsyncReaderWriter>> second_stream{
      absl::make_unique<StrictMock<AsyncReaderWriter>>()};
  StrictMock<AsyncReaderWriter>& second_stream_ref{*second_stream};

  InSequence seq;

  promise<absl::optional<FakeResponse>> read_promise;
  auto start = stream_->Start();

  EXPECT_CALL(first_stream_ref_, Read)
      .WillOnce(Return(ByMove(read_promise.get_future())));
  auto read = stream_->Read();

  EXPECT_CALL(first_stream_ref_, Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(make_ready_future(false))));
  auto write = stream_->Write(kBasicRequest);

  // write shouldn't finish until retry
  // loop done
  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(first_stream_ref_, Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(kFailStatus)).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call).WillOnce(ZeroSleep());
  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(second_stream))));
  EXPECT_CALL(second_stream_ref, Start)
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(initializer_, Call).WillOnce(InitializeInline());

  read_promise.set_value(absl::make_optional(kBasicResponse));

  auto read_response = read.get();
  ASSERT_TRUE(read_response.has_value());
  EXPECT_EQ(*read_response, kBasicResponse);
  ASSERT_FALSE(write.get());

  EXPECT_CALL(second_stream_ref, Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(make_ready_future(true))));
  write = stream_->Write(kBasicRequest);
  ASSERT_TRUE(write.get());

  // start shouldn't finish until permanent
  // error from retry loop or `Finish`
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(second_stream_ref, Cancel);
  EXPECT_CALL(second_stream_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  auto shutdown = stream_->Shutdown();
  shutdown.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(InitializedResumableAsyncReadWriteStreamingRpcTest,
       ReadFailWhileWriteInFlight) {
  std::unique_ptr<StrictMock<AsyncReaderWriter>> second_stream{
      absl::make_unique<StrictMock<AsyncReaderWriter>>()};
  StrictMock<AsyncReaderWriter>& second_stream_ref{*second_stream};

  InSequence seq;

  auto start = stream_->Start();
  promise<bool> write_promise;

  EXPECT_CALL(first_stream_ref_, Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  auto write = stream_->Write(kBasicRequest);

  EXPECT_CALL(first_stream_ref_, Read)
      .WillOnce(
          Return(ByMove(make_ready_future(absl::optional<FakeResponse>()))));
  auto read = stream_->Read();

  // read shouldn't finish until retry
  // loop done
  EXPECT_EQ(read.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(first_stream_ref_, Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(kFailStatus)).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call).WillOnce(ZeroSleep());
  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(second_stream))));
  EXPECT_CALL(second_stream_ref, Start)
      .WillOnce(Return(ByMove(make_ready_future(true))));
  EXPECT_CALL(initializer_, Call).WillOnce(InitializeInline());

  write_promise.set_value(true);

  ASSERT_FALSE(read.get().has_value());
  ASSERT_TRUE(write.get());

  EXPECT_CALL(second_stream_ref, Read)
      .WillOnce(Return(
          ByMove(make_ready_future(absl::make_optional(kBasicResponse)))));
  auto response0 = stream_->Read().get();
  ASSERT_TRUE(response0.has_value());
  EXPECT_EQ(*response0, kBasicResponse);

  // start shouldn't finish until permanent
  // error from retry loop or `Finish`
  EXPECT_EQ(start.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  EXPECT_CALL(second_stream_ref, Cancel);
  EXPECT_CALL(second_stream_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));

  auto shutdown = stream_->Shutdown();
  shutdown.get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(InitializedResumableAsyncReadWriteStreamingRpcTest,
       StartFailsDuringRetryPermanentError) {
  std::unique_ptr<StrictMock<AsyncReaderWriter>> second_stream{
      absl::make_unique<StrictMock<AsyncReaderWriter>>()};
  StrictMock<AsyncReaderWriter>& second_stream_ref{*second_stream};

  InSequence seq;
  auto start = stream_->Start();
  promise<absl::optional<FakeResponse>> read_promise;

  EXPECT_CALL(first_stream_ref_, Read)
      .WillOnce(Return(ByMove(read_promise.get_future())));
  auto read = stream_->Read();

  EXPECT_CALL(first_stream_ref_, Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& mock_retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  EXPECT_CALL(mock_retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(mock_retry_policy_ref, OnFailure(_)).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call).WillOnce(ZeroSleep());
  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(second_stream))));
  promise<bool> start_promise;
  EXPECT_CALL(second_stream_ref, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));

  read_promise.set_value(absl::optional<FakeResponse>());

  EXPECT_CALL(second_stream_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));
  EXPECT_CALL(mock_retry_policy_ref, IsExhausted).WillOnce(Return(true));

  start_promise.set_value(false);

  ASSERT_FALSE(read.get().has_value());
  EXPECT_EQ(start.get(), kFailStatus);
  auto shutdown = stream_->Shutdown();
  shutdown.get();
}

TEST_F(InitializedResumableAsyncReadWriteStreamingRpcTest,
       ReadInMiddleOfRetryAfterStart) {
  std::unique_ptr<StrictMock<AsyncReaderWriter>> second_stream{
      absl::make_unique<StrictMock<AsyncReaderWriter>>()};
  StrictMock<AsyncReaderWriter>& second_stream_ref{*second_stream};

  InSequence seq;

  auto start = stream_->Start();
  promise<bool> write_promise;

  EXPECT_CALL(first_stream_ref_, Write(kBasicRequest, _))
      .WillOnce(Return(ByMove(write_promise.get_future())));
  auto write = stream_->Write(kBasicRequest);

  EXPECT_CALL(first_stream_ref_, Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(kFailStatus)).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call).WillOnce(ZeroSleep());
  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(second_stream))));
  promise<bool> start_promise;
  EXPECT_CALL(second_stream_ref, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));
  EXPECT_CALL(initializer_, Call).WillOnce(InitializeInline());

  write_promise.set_value(false);

  // read and write shouldn't
  // finish until retry loop finishes
  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  auto read = stream_->Read();
  EXPECT_EQ(read.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  start_promise.set_value(true);

  EXPECT_CALL(second_stream_ref, Cancel);
  EXPECT_CALL(second_stream_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  ASSERT_FALSE(write.get());
  ASSERT_FALSE(read.get().has_value());
  stream_->Shutdown().get();
  EXPECT_THAT(start.get(), IsOk());
}

TEST_F(InitializedResumableAsyncReadWriteStreamingRpcTest,
       WriteInMiddleOfRetryAfterStart) {
  std::unique_ptr<StrictMock<AsyncReaderWriter>> second_stream{
      absl::make_unique<StrictMock<AsyncReaderWriter>>()};
  StrictMock<AsyncReaderWriter>& second_stream_ref{*second_stream};

  InSequence seq;

  auto start = stream_->Start();
  promise<absl::optional<FakeResponse>> read_promise;

  EXPECT_CALL(first_stream_ref_, Read)
      .WillOnce(Return(ByMove(read_promise.get_future())));
  auto read = stream_->Read();

  EXPECT_CALL(first_stream_ref_, Finish)
      .WillOnce(Return(ByMove(make_ready_future(kFailStatus))));

  auto mock_retry_policy = absl::make_unique<StrictMock<MockRetryPolicy>>();
  auto& retry_policy_ref = *mock_retry_policy;
  EXPECT_CALL(retry_policy_factory_, Call)
      .WillOnce(Return(ByMove(std::move(mock_retry_policy))));

  auto backoff_policy = absl::make_unique<StrictMock<MockBackoffPolicy>>();
  auto& backoff_policy_ref = *backoff_policy;
  EXPECT_CALL(*backoff_policy_, clone)
      .WillOnce(Return(ByMove(std::move(backoff_policy))));

  EXPECT_CALL(retry_policy_ref, IsExhausted).WillOnce(Return(false));
  EXPECT_CALL(retry_policy_ref, OnFailure(kFailStatus)).WillOnce(Return(true));
  EXPECT_CALL(backoff_policy_ref, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(7)));
  EXPECT_CALL(sleeper_, Call).WillOnce(ZeroSleep());
  EXPECT_CALL(stream_factory_, Call)
      .WillOnce(Return(ByMove(std::move(second_stream))));
  promise<bool> start_promise;
  EXPECT_CALL(second_stream_ref, Start)
      .WillOnce(Return(ByMove(start_promise.get_future())));
  EXPECT_CALL(initializer_, Call).WillOnce(InitializeInline());

  read_promise.set_value(absl::nullopt);

  // write and read shouldn't
  // finish until retry loop finishes
  EXPECT_EQ(read.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);

  auto write = stream_->Write(kBasicRequest);
  EXPECT_EQ(write.wait_for(ms(kFutureWaitMs)), std::future_status::timeout);
  start_promise.set_value(true);

  EXPECT_CALL(second_stream_ref, Cancel);
  EXPECT_CALL(second_stream_ref, Finish)
      .WillOnce(Return(ByMove(make_ready_future(Status()))));
  ASSERT_FALSE(read.get().has_value());
  ASSERT_FALSE(write.get());
  stream_->Shutdown().get();
  EXPECT_THAT(start.get(), IsOk());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google

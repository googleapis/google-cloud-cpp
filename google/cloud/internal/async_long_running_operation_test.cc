// Copyright 2021 Google LLC
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

#include "google/cloud/internal/async_long_running_operation.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include <gmock/gmock.h>
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::ImmutableOptions;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::longrunning::Operation;
using ::testing::Return;

using Request = google::protobuf::Duration;
using Response = google::protobuf::Timestamp;

struct StringOption {
  using Type = std::string;
};

class MockStub {
 public:
  MOCK_METHOD(future<StatusOr<Operation>>, AsyncCreateResponse,
              (CompletionQueue & cq, std::shared_ptr<grpc::ClientContext>,
               ImmutableOptions, Request const&),
              ());
  MOCK_METHOD(future<StatusOr<Operation>>, AsyncGetOperation,
              (CompletionQueue & cq, std::shared_ptr<grpc::ClientContext>,
               ImmutableOptions,
               google::longrunning::GetOperationRequest const&),
              ());
  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue & cq, std::shared_ptr<grpc::ClientContext>,
               ImmutableOptions,
               google::longrunning::CancelOperationRequest const&),
              ());
};

class MockPollingPolicy : public PollingPolicy {
 public:
  MOCK_METHOD(std::unique_ptr<PollingPolicy>, clone, (), (const, override));
  MOCK_METHOD(bool, OnFailure, (Status const&), (override));
  MOCK_METHOD(std::chrono::milliseconds, WaitPeriod, (), (override));
};

struct TestRetryablePolicy {
  static bool IsPermanentFailure(google::cloud::Status const& s) {
    return !s.ok() &&
           (s.code() == google::cloud::StatusCode::kPermissionDenied);
  }
};

std::unique_ptr<RetryPolicy> TestRetryPolicy() {
  return LimitedErrorCountRetryPolicy<TestRetryablePolicy>(5).clone();
}

std::unique_ptr<BackoffPolicy> TestBackoffPolicy() {
  using us = std::chrono::microseconds;
  return ExponentialBackoffPolicy(us(100), us(100), 2.0).clone();
}

using StartOperation =
    std::function<future<StatusOr<google::longrunning::Operation>>(
        CompletionQueue&, std::shared_ptr<grpc::ClientContext>,
        ImmutableOptions, Request const&)>;

using StartOperationImplicit =
    std::function<future<StatusOr<google::longrunning::Operation>>(
        CompletionQueue&, std::shared_ptr<grpc::ClientContext>,
        Request const&)>;

StartOperation MakeStart(std::shared_ptr<MockStub> const& m) {
  return [m](CompletionQueue& cq, auto context,
             // NOLINTNEXTLINE(performance-unnecessary-value-param)
             ImmutableOptions options, Request const& request) {
    return m->AsyncCreateResponse(cq, std::move(context), std::move(options),
                                  request);
  };
}

AsyncPollLongRunningOperation MakePoll(std::shared_ptr<MockStub> const& m) {
  return [m](CompletionQueue& cq, auto context,
             // NOLINTNEXTLINE(performance-unnecessary-value-param)
             ImmutableOptions options,
             google::longrunning::GetOperationRequest const& request) {
    return m->AsyncGetOperation(cq, std::move(context), std::move(options),
                                request);
  };
}

AsyncCancelLongRunningOperation MakeCancel(std::shared_ptr<MockStub> const& m) {
  return [m](CompletionQueue& cq, auto context,
             // NOLINTNEXTLINE(performance-unnecessary-value-param)
             ImmutableOptions options,
             google::longrunning::CancelOperationRequest const& request) {
    return m->AsyncCancelOperation(cq, std::move(context), std::move(options),
                                   request);
  };
}

std::string CurrentTestName() {
  return testing::UnitTest::GetInstance()->current_test_info()->name();
}

TEST(AsyncLongRunningTest, RequestPollThenSuccessMetadata) {
  Response expected;
  expected.set_seconds(123456);
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");
  google::longrunning::Operation done_op = starting_op;
  done_op.set_done(true);
  done_op.mutable_metadata()->PackFrom(expected);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncCreateResponse)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    Request const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(done_op));
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  Request request;
  request.set_seconds(123456);
  request.set_nanos(456789);

  auto current =
      MakeImmutableOptions(Options{}.set<StringOption>(CurrentTestName()));
  auto actual =
      AsyncLongRunningOperation<Response>(
          cq, current, std::move(request), MakeStart(mock), MakePoll(mock),
          MakeCancel(mock), &ExtractLongRunningResultMetadata<Response>,
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
          std::move(policy), "test-function")
          .get();
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncLongRunningTest, RequestPollThenSuccessResponse) {
  Response expected;
  expected.set_seconds(123456);
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");
  google::longrunning::Operation done_op = starting_op;
  done_op.set_done(true);
  done_op.mutable_response()->PackFrom(expected);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncCreateResponse)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    Request const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(done_op));
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  Request request;
  request.set_seconds(123456);
  request.set_nanos(456789);

  auto current =
      MakeImmutableOptions(Options{}.set<StringOption>(CurrentTestName()));
  auto actual =
      AsyncLongRunningOperation<Response>(
          cq, current, std::move(request), MakeStart(mock), MakePoll(mock),
          MakeCancel(mock), &ExtractLongRunningResultResponse<Response>,
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
          std::move(policy), "test-function")
          .get();
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncLongRunningTest, RequestPollThenCancel) {
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  AsyncSequencer<void> timer;
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillRepeatedly([&timer](std::chrono::nanoseconds) {
        return timer.PushBack().then([](future<void>) {
          return make_status_or(std::chrono::system_clock::now());
        });
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncCreateResponse)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    Request const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      })
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(StatusOr<google::longrunning::Operation>(
            Status{StatusCode::kCancelled, "cancelled"}));
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::CancelOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(Status{});
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillRepeatedly([](Status const& status) {
    return status.code() != StatusCode::kCancelled;
  });
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  Request request;
  request.set_seconds(123456);
  request.set_nanos(456789);
  auto current =
      MakeImmutableOptions(Options{}.set<StringOption>(CurrentTestName()));
  auto pending = AsyncLongRunningOperation<Response>(
      cq, current, std::move(request), MakeStart(mock), MakePoll(mock),
      MakeCancel(mock), &ExtractLongRunningResultMetadata<Response>,
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      std::move(policy), "test-function");

  // Wait until the polling loop is backing off for a second time.
  timer.PopFront().set_value();
  auto t = timer.PopFront();
  {
    // cancel the long running operation
    OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
    pending.cancel();
  }
  // release timer
  t.set_value();
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled));
}

TEST(AsyncLongRunningTest, AwaitPollThenSuccessMetadata) {
  Response expected;
  expected.set_seconds(123456);
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");
  google::longrunning::Operation done_op = starting_op;
  done_op.set_done(true);
  done_op.mutable_metadata()->PackFrom(expected);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(done_op));
      });
  auto polling_policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*polling_policy, clone()).Times(0);
  EXPECT_CALL(*polling_policy, OnFailure).Times(0);
  EXPECT_CALL(*polling_policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  auto current =
      MakeImmutableOptions(Options{}.set<StringOption>(CurrentTestName()));
  auto actual = AsyncAwaitLongRunningOperation<Response>(
                    cq, current, starting_op, MakePoll(mock), MakeCancel(mock),
                    &ExtractLongRunningResultMetadata<Response>,
                    std::move(polling_policy), "test-function")
                    .get();
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncLongRunningTest, AwaitPollThenSuccessResponse) {
  Response expected;
  expected.set_seconds(123456);
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");
  google::longrunning::Operation done_op = starting_op;
  done_op.set_done(true);
  done_op.mutable_response()->PackFrom(expected);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(done_op));
      });
  auto polling_policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*polling_policy, clone()).Times(0);
  EXPECT_CALL(*polling_policy, OnFailure).Times(0);
  EXPECT_CALL(*polling_policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  auto current =
      MakeImmutableOptions(Options{}.set<StringOption>(CurrentTestName()));
  auto actual = AsyncAwaitLongRunningOperation<Response>(
                    cq, current, starting_op, MakePoll(mock), MakeCancel(mock),
                    &ExtractLongRunningResultResponse<Response>,
                    std::move(polling_policy), "test-function")
                    .get();
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncLongRunningTest, AwaitPollThenCancel) {
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  AsyncSequencer<void> timer;
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillRepeatedly([&timer](std::chrono::nanoseconds) {
        return timer.PushBack().then([](future<void>) {
          return make_status_or(std::chrono::system_clock::now());
        });
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      })
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(StatusOr<google::longrunning::Operation>(
            Status{StatusCode::kCancelled, "cancelled"}));
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::CancelOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(Status{});
      });
  auto polling_policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*polling_policy, clone()).Times(0);
  EXPECT_CALL(*polling_policy, OnFailure)
      .WillRepeatedly([](Status const& status) {
        return status.code() != StatusCode::kCancelled;
      });
  EXPECT_CALL(*polling_policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  auto current =
      MakeImmutableOptions(Options{}.set<StringOption>(CurrentTestName()));
  auto pending = AsyncAwaitLongRunningOperation<Response>(
      cq, current, starting_op, MakePoll(mock), MakeCancel(mock),
      &ExtractLongRunningResultMetadata<Response>, std::move(polling_policy),
      "test-function");

  // Wait until the polling loop is backing off for a second time.
  timer.PopFront().set_value();
  auto t = timer.PopFront();
  {
    // cancel the long running operation
    OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
    pending.cancel();
  }
  // release timer
  t.set_value();
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

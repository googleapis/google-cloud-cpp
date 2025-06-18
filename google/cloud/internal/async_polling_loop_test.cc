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

#include "google/cloud/internal/async_polling_loop.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/protobuf/timestamp.pb.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;
using TimerType = StatusOr<std::chrono::system_clock::time_point>;
using Response = google::protobuf::Timestamp;

struct StringOption {
  using Type = std::string;
};

class MockStub {
 public:
  MOCK_METHOD(future<StatusOr<google::longrunning::Operation>>,
              AsyncGetOperation,
              (CompletionQueue&, std::shared_ptr<grpc::ClientContext>,
               ImmutableOptions,
               google::longrunning::GetOperationRequest const&),
              ());

  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue&, std::shared_ptr<grpc::ClientContext>,
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

AsyncPollLongRunningOperation MakePoll(std::shared_ptr<MockStub> const& mock) {
  return [mock](CompletionQueue& cq, auto context,
                // NOLINTNEXTLINE(performance-unnecessary-value-param)
                ImmutableOptions options,
                google::longrunning::GetOperationRequest const& request) {
    return mock->AsyncGetOperation(cq, std::move(context), std::move(options),
                                   request);
  };
}

AsyncCancelLongRunningOperation MakeCancel(
    std::shared_ptr<MockStub> const& mock) {
  return [mock](CompletionQueue& cq, auto context,
                // NOLINTNEXTLINE(performance-unnecessary-value-param)
                ImmutableOptions options,
                google::longrunning::CancelOperationRequest const& request) {
    return mock->AsyncCancelOperation(cq, std::move(context),
                                      std::move(options), request);
  };
}

std::string CurrentTestName() {
  return testing::UnitTest::GetInstance()->current_test_info()->name();
}

TEST(AsyncPollingLoopTest, ImmediateSuccess) {
  Response expected;
  expected.set_seconds(123456);
  google::longrunning::Operation op;
  op.set_name("test-op-name");
  op.set_done(true);
  op.mutable_metadata()->PackFrom(expected);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation).Times(0);
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod).Times(0);
  CompletionQueue cq;
  auto current = MakeImmutableOptions(Options{});
  auto actual =
      AsyncPollingLoop(cq, current, make_ready_future(make_status_or(op)),
                       MakePoll(mock), MakeCancel(mock), std::move(policy),
                       "test-function")
          .get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(op));
}

TEST(AsyncPollingLoopTest, ImmediateCancel) {
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);
  auto current =
      MakeImmutableOptions(Options{}.set<StringOption>(CurrentTestName()));

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([](CompletionQueue&, auto, ImmutableOptions const& options,
                   google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(Status{
                StatusCode::kCancelled, "test-function: operation cancelled"}));
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([](CompletionQueue&, auto, ImmutableOptions const& options,
                   google::longrunning::CancelOperationRequest const& request) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        EXPECT_EQ(request.name(), "test-op-name");
        return make_ready_future(Status{});
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kCancelled));
    return false;
  });
  EXPECT_CALL(*policy, WaitPeriod)
      .WillOnce(Return(std::chrono::milliseconds(1)));

  promise<StatusOr<google::longrunning::Operation>> p;
  auto pending =
      AsyncPollingLoop(cq, current, p.get_future(), MakePoll(mock),
                       MakeCancel(mock), std::move(policy), "test-function");
  {
    OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
    pending.cancel();
  }
  p.set_value(starting_op);
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled,
                               AllOf(HasSubstr("test-function"),
                                     HasSubstr("operation cancelled"))));
}

TEST(AsyncPollingLoopTest, PollThenSuccess) {
  Response response;
  response.set_seconds(123456);
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");
  google::longrunning::Operation expected = starting_op;
  expected.set_done(true);
  expected.mutable_metadata()->PackFrom(response);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);
  auto current =
      MakeImmutableOptions(Options{}.set<StringOption>(CurrentTestName()));

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(expected));
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  auto pending = AsyncPollingLoop(
      cq, current, make_ready_future(make_status_or(starting_op)),
      MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function");
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncPollingLoopTest, PollThenTimerError) {
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds) {
        return make_ready_future(
            StatusOr<std::chrono::system_clock::time_point>(
                Status{StatusCode::kCancelled, "cq shutdown"}));
      });
  CompletionQueue cq(mock_cq);
  auto current = MakeImmutableOptions(Options{});

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation).Times(0);
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  auto actual = AsyncPollingLoop(cq, current,
                                 make_ready_future(make_status_or(starting_op)),
                                 MakePoll(mock), MakeCancel(mock),
                                 std::move(policy), "test-function")
                    .get();
  EXPECT_THAT(actual,
              StatusIs(StatusCode::kCancelled, HasSubstr("cq shutdown")));
}

TEST(AsyncPollingLoopTest, PollThenEventualSuccess) {
  Response response;
  response.set_seconds(123456);
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");
  google::longrunning::Operation expected = starting_op;
  expected.set_done(true);
  expected.mutable_metadata()->PackFrom(response);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillRepeatedly([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      })
      .WillOnce([](CompletionQueue&, auto, ImmutableOptions const& options,
                   google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(StatusOr<google::longrunning::Operation>(
            Status(StatusCode::kUnavailable, "try-again")));
      })
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      })
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(expected));
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillRepeatedly(Return(true));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  auto current =
      MakeImmutableOptions(Options{}.set<StringOption>(CurrentTestName()));
  auto pending = AsyncPollingLoop(
      cq, current, make_ready_future(make_status_or(starting_op)),
      MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function");
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncPollingLoopTest, PollThenExhaustedPollingPolicy) {
  Response response;
  response.set_seconds(123456);
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");
  google::longrunning::Operation expected = starting_op;
  expected.set_done(true);
  expected.mutable_metadata()->PackFrom(response);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillRepeatedly([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .Times(AtLeast(2))
      .WillRepeatedly([&](CompletionQueue&, auto,
                          ImmutableOptions const& options,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  auto current =
      MakeImmutableOptions(Options{}.set<StringOption>(CurrentTestName()));
  auto pending = AsyncPollingLoop(
      cq, current, make_ready_future(make_status_or(starting_op)),
      MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function");
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  EXPECT_THAT(actual,
              StatusIs(Not(Eq(StatusCode::kOk)),
                       AllOf(HasSubstr("test-function"),
                             HasSubstr("terminated by polling policy"))));
}

TEST(AsyncPollingLoopTest, PollThenExhaustedPollingPolicyWithFailure) {
  Response response;
  response.set_seconds(123456);
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");
  google::longrunning::Operation expected = starting_op;
  expected.set_done(true);
  expected.mutable_metadata()->PackFrom(response);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillRepeatedly([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .Times(AtLeast(2))
      .WillRepeatedly([&](CompletionQueue&, auto,
                          ImmutableOptions const& options,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(StatusOr<google::longrunning::Operation>(
            Status{StatusCode::kUnavailable, "try-again"}));
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  auto current =
      MakeImmutableOptions(Options{}.set<StringOption>(CurrentTestName()));
  auto pending = AsyncPollingLoop(
      cq, current, make_ready_future(make_status_or(starting_op)),
      MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function");
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  ASSERT_THAT(actual,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
}

TEST(AsyncPollingLoopTest, PollLifetime) {
  Response response;
  response.set_seconds(123456);
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");
  google::longrunning::Operation expected = starting_op;
  expected.set_done(true);
  expected.mutable_metadata()->PackFrom(response);

  AsyncSequencer<TimerType> timer_sequencer;
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .Times(4)
      .WillRepeatedly(
          [&](std::chrono::nanoseconds) { return timer_sequencer.PushBack(); });
  CompletionQueue cq(mock_cq);

  AsyncSequencer<StatusOr<google::longrunning::Operation>> get_sequencer;
  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .Times(4)
      .WillRepeatedly([&](CompletionQueue&, auto,
                          ImmutableOptions const& options,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return get_sequencer.PushBack();
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillRepeatedly(Return(true));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  auto current =
      MakeImmutableOptions(Options{}.set<StringOption>(CurrentTestName()));
  auto pending = AsyncPollingLoop(
      cq, current, make_ready_future(make_status_or(starting_op)),
      MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function");

  for (int i = 0; i != 3; ++i) {
    timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
    get_sequencer.PopFront().set_value(starting_op);
  }
  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  get_sequencer.PopFront().set_value(expected);
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncPollingLoopTest, PollThenCancelDuringTimer) {
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");

  AsyncSequencer<TimerType> timer_sequencer;
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .Times(AtLeast(1))
      .WillRepeatedly(
          [&](std::chrono::nanoseconds) { return timer_sequencer.PushBack(); });
  CompletionQueue cq(mock_cq);

  AsyncSequencer<StatusOr<google::longrunning::Operation>> get_sequencer;
  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .Times(AtLeast(1))
      .WillRepeatedly([&](CompletionQueue&, auto,
                          ImmutableOptions const& options,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return get_sequencer.PushBack();
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::CancelOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(Status{});
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure)
      .Times(2)
      .WillRepeatedly([](Status const& status) {
        return status.code() != StatusCode::kCancelled;
      });
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  auto current =
      MakeImmutableOptions(Options{}.set<StringOption>(CurrentTestName()));
  auto pending = AsyncPollingLoop(
      cq, current, make_ready_future(make_status_or(starting_op)),
      MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function");

  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  get_sequencer.PopFront().set_value(starting_op);
  auto t = timer_sequencer.PopFront();
  {
    OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
    pending.cancel();
  }
  t.set_value(std::chrono::system_clock::now());
  get_sequencer.PopFront().set_value(
      Status{StatusCode::kCancelled, "test-function: operation cancelled"});

  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled,
                               AllOf(HasSubstr("test-function"),
                                     HasSubstr("operation cancelled"))));
}

TEST(AsyncPollingLoopTest, PollThenCancelDuringPoll) {
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");

  AsyncSequencer<TimerType> timer_sequencer;
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .Times(AtLeast(1))
      .WillRepeatedly(
          [&](std::chrono::nanoseconds) { return timer_sequencer.PushBack(); });
  CompletionQueue cq(mock_cq);

  AsyncSequencer<StatusOr<google::longrunning::Operation>> get_sequencer;
  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .Times(AtLeast(1))
      .WillRepeatedly([&](CompletionQueue&, auto,
                          ImmutableOptions const& options,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return get_sequencer.PushBack();
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const& options,
                    google::longrunning::CancelOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(Status{});
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure)
      .Times(2)
      .WillRepeatedly([](Status const& status) {
        return status.code() != StatusCode::kCancelled;
      });
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  auto current =
      MakeImmutableOptions(Options{}.set<StringOption>(CurrentTestName()));
  auto pending = AsyncPollingLoop(
      cq, current, make_ready_future(make_status_or(starting_op)),
      MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function");

  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  get_sequencer.PopFront().set_value(starting_op);
  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  auto g = get_sequencer.PopFront();
  {
    OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
    pending.cancel();
  }
  g.set_value(
      Status{StatusCode::kCancelled, "test-function: operation cancelled"});

  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled,
                               AllOf(HasSubstr("test-function"),
                                     HasSubstr("operation cancelled"))));
}

// This is the same test as PollThenCancelDuringPoll, but with checks that
// GrpcSetupPollOption takes effect on poll and cancel requests.
TEST(AsyncPollingLoopTest, ConfigurePollContext) {
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([](CompletionQueue&, auto context,
                   ImmutableOptions const& options,
                   google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        // Ensure that our options have taken affect on the ClientContext before
        // we start using it.
        EXPECT_EQ(GRPC_COMPRESS_DEFLATE, context->compression_algorithm());
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(Status{
                StatusCode::kCancelled, "test-function: operation cancelled"}));
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([](CompletionQueue&, auto context,
                   ImmutableOptions const& options,
                   google::longrunning::CancelOperationRequest const& request) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        // Ensure that our options have taken affect on the ClientContext before
        // we start using it.
        EXPECT_EQ(GRPC_COMPRESS_DEFLATE, context->compression_algorithm());
        EXPECT_EQ(request.name(), "test-op-name");
        return make_ready_future(Status{});
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kCancelled));
    return false;
  });
  EXPECT_CALL(*policy, WaitPeriod)
      .WillOnce(Return(std::chrono::milliseconds(1)));

  auto setup_poll = [](grpc::ClientContext& context) {
    context.set_compression_algorithm(GRPC_COMPRESS_DEFLATE);
  };
  auto current =
      MakeImmutableOptions(Options{}
                               .set<GrpcSetupPollOption>(setup_poll)
                               .set<StringOption>(CurrentTestName()));
  promise<StatusOr<google::longrunning::Operation>> p;
  auto pending =
      AsyncPollingLoop(cq, current, p.get_future(), MakePoll(mock),
                       MakeCancel(mock), std::move(policy), "test-function");
  {
    OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
    pending.cancel();
  }
  p.set_value(starting_op);
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled,
                               AllOf(HasSubstr("test-function"),
                                     HasSubstr("operation cancelled"))));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::IsActive;
using ::google::cloud::testing_util::OTelAttribute;
using ::google::cloud::testing_util::SpanHasAttributes;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::SizeIs;

TEST(AsyncPollingLoopTest, TracedAsyncBackoff) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillRepeatedly([](auto) {
    return make_ready_future<TimerType>(std::chrono::system_clock::now());
  });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation).WillRepeatedly([] {
    return make_ready_future<StatusOr<google::longrunning::Operation>>(
        UnavailableError("try again"));
  });

  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  (void)AsyncPollingLoop(cq, internal::MakeImmutableOptions(EnableTracing({})),
                         make_ready_future(make_status_or(starting_op)),
                         MakePoll(mock), MakeCancel(mock), std::move(policy),
                         "test-function")
      .get();

  // The polling loop waits once initially, and once for each of the three retry
  // attempts. So we expect a total of 4 backoffs.
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, AllOf(SizeIs(4), Each(SpanNamed("Async Backoff"))));
}

TEST(AsyncPollingLoopTest, SpanActiveDuringCancel) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = MakeSpan("span");
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");

  AsyncSequencer<TimerType> timer_sequencer;
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).Times(2).WillRepeatedly([&] {
    return timer_sequencer.PushBack();
  });
  CompletionQueue cq(mock_cq);

  AsyncSequencer<StatusOr<google::longrunning::Operation>> get_sequencer;
  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation).Times(2).WillRepeatedly([&] {
    return get_sequencer.PushBack();
  });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, auto, ImmutableOptions const&,
                    google::longrunning::CancelOperationRequest const&) {
        EXPECT_THAT(span, IsActive());
        return make_ready_future(Status{});
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure)
      .Times(2)
      .WillRepeatedly([](Status const& status) {
        return status.code() != StatusCode::kCancelled;
      });
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  OTelScope scope(span);
  auto pending = AsyncPollingLoop(
      cq, MakeImmutableOptions(EnableTracing({})),
      make_ready_future(make_status_or(starting_op)), MakePoll(mock),
      MakeCancel(mock), std::move(policy), "test-function");

  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  get_sequencer.PopFront().set_value(starting_op);
  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  auto g = get_sequencer.PopFront();
  {
    auto overlay = opentelemetry::trace::Scope(MakeSpan("overlay"));
    pending.cancel();
  }
  g.set_value(CancelledError("cancelled"));

  auto overlay = opentelemetry::trace::Scope(MakeSpan("overlay"));
  (void)pending.get();
}

TEST(AsyncPollingLoopTest, TraceCapturesOperationName) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  google::longrunning::Operation op;
  op.set_name("test-op-name");
  op.set_done(true);

  auto span = MakeSpan("span");
  auto mock = std::make_shared<MockStub>();
  auto policy = std::make_unique<MockPollingPolicy>();
  CompletionQueue cq;

  OTelScope scope(span);
  (void)AsyncPollingLoop(cq, internal::MakeImmutableOptions(EnableTracing({})),
                         make_ready_future(make_status_or(op)), MakePoll(mock),
                         MakeCancel(mock), std::move(policy), "test-function")
      .get();
  span->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(SpanNamed("span"),
                                SpanHasAttributes(OTelAttribute<std::string>(
                                    "gl-cpp.LRO_name", "test-op-name")))));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

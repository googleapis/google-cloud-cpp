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
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::bigtable::admin::v2::Instance;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::longrunning::Operation;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::HasSubstr;
using ::testing::Return;

struct StringOption {
  using Type = std::string;
};

class MockStub {
 public:
  MOCK_METHOD(future<StatusOr<Operation>>, AsyncGetOperation,
              (CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
               google::longrunning::GetOperationRequest const&),
              ());

  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
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
  return
      [mock](CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
             google::longrunning::GetOperationRequest const& request) {
        return mock->AsyncGetOperation(cq, std::move(context), request);
      };
}

AsyncCancelLongRunningOperation MakeCancel(
    std::shared_ptr<MockStub> const& mock) {
  return
      [mock](CompletionQueue& cq, std::unique_ptr<grpc::ClientContext> context,
             google::longrunning::CancelOperationRequest const& request) {
        return mock->AsyncCancelOperation(cq, std::move(context), request);
      };
}

TEST(AsyncPollingLoopTest, ImmediateSuccess) {
  Instance expected;
  expected.set_name("test-instance-name");
  google::longrunning::Operation op;
  op.set_name("test-op-name");
  op.set_done(true);
  op.mutable_metadata()->PackFrom(expected);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation).Times(0);
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod).Times(0);
  CompletionQueue cq;
  auto actual = AsyncPollingLoop(cq, make_ready_future(make_status_or(op)),
                                 MakePoll(mock), MakeCancel(mock),
                                 std::move(policy), "test-function")
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

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(), "ImmediateCancel");
        return make_ready_future(StatusOr<Operation>(Status{
            StatusCode::kCancelled, "test-function: operation cancelled"}));
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   google::longrunning::CancelOperationRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(), "ImmediateCancel");
        EXPECT_EQ(request.name(), "test-op-name");
        return make_ready_future(Status{});
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillOnce([](Status const& status) {
    EXPECT_THAT(status, StatusIs(StatusCode::kCancelled));
    return false;
  });
  EXPECT_CALL(*policy, WaitPeriod)
      .WillOnce(Return(std::chrono::milliseconds(1)));

  OptionsSpan span(Options{}.set<StringOption>("ImmediateCancel"));
  promise<StatusOr<google::longrunning::Operation>> p;
  auto pending =
      AsyncPollingLoop(cq, p.get_future(), MakePoll(mock), MakeCancel(mock),
                       std::move(policy), "test-function");
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
  Instance instance;
  instance.set_name("test-instance-name");
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");
  google::longrunning::Operation expected = starting_op;
  expected.set_done(true);
  expected.mutable_metadata()->PackFrom(instance);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(), "PollThenSuccess");
        return make_ready_future(make_status_or(expected));
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  OptionsSpan span(Options{}.set<StringOption>("PollThenSuccess"));
  auto pending = AsyncPollingLoop(
      cq, make_ready_future(make_status_or(starting_op)), MakePoll(mock),
      MakeCancel(mock), std::move(policy), "test-function");
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

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation).Times(0);
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  auto actual =
      AsyncPollingLoop(cq, make_ready_future(make_status_or(starting_op)),
                       MakePoll(mock), MakeCancel(mock), std::move(policy),
                       "test-function")
          .get();
  EXPECT_THAT(actual,
              StatusIs(StatusCode::kCancelled, HasSubstr("cq shutdown")));
}

TEST(AsyncPollingLoopTest, PollThenEventualSuccess) {
  Instance instance;
  instance.set_name("test-instance-name");
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");
  google::longrunning::Operation expected = starting_op;
  expected.set_done(true);
  expected.mutable_metadata()->PackFrom(instance);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillRepeatedly([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "PollThenEventualSuccess");
        return make_ready_future(make_status_or(starting_op));
      })
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "PollThenEventualSuccess");
        return make_ready_future(
            StatusOr<Operation>(Status(StatusCode::kUnavailable, "try-again")));
      })
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "PollThenEventualSuccess");
        return make_ready_future(make_status_or(starting_op));
      })
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "PollThenEventualSuccess");
        return make_ready_future(make_status_or(expected));
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillRepeatedly(Return(true));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  OptionsSpan span(Options{}.set<StringOption>("PollThenEventualSuccess"));
  auto pending = AsyncPollingLoop(
      cq, make_ready_future(make_status_or(starting_op)), MakePoll(mock),
      MakeCancel(mock), std::move(policy), "test-function");
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncPollingLoopTest, PollThenExhaustedPollingPolicy) {
  Instance instance;
  instance.set_name("test-instance-name");
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .Times(3)
      .WillRepeatedly([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "PollThenExhaustedPollingPolicy");
        return make_ready_future(StatusOr<Operation>(Status{
            StatusCode::kCancelled, "test-function: operation cancelled"}));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .Times(2)
      .WillRepeatedly([&](CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "PollThenExhaustedPollingPolicy");
        return make_ready_future(make_status_or(starting_op));
      })
      .RetiresOnSaturation();
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   google::longrunning::CancelOperationRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "PollThenExhaustedPollingPolicy");
        EXPECT_EQ(request.name(), "test-op-name");
        return make_ready_future(Status{});
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure)
      .WillOnce([](Status const& status) {
        EXPECT_THAT(status, IsOk());
        return true;  // retry
      })
      .WillOnce([](Status const& status) {
        EXPECT_THAT(status, IsOk());
        return false;  // exhausted
      })
      .WillOnce([](Status const& status) {
        EXPECT_THAT(status, StatusIs(StatusCode::kCancelled));
        return false;  // permanent
      });
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  OptionsSpan span(
      Options{}.set<StringOption>("PollThenExhaustedPollingPolicy"));
  auto pending = AsyncPollingLoop(
      cq, make_ready_future(make_status_or(starting_op)), MakePoll(mock),
      MakeCancel(mock), std::move(policy), "test-function");

  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled,
                               AllOf(HasSubstr("test-function"),
                                     HasSubstr("operation cancelled"))));
}

TEST(AsyncPollingLoopTest, PollThenExhaustedPollingPolicyWithFailure) {
  Instance instance;
  instance.set_name("test-instance-name");
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");
  google::longrunning::Operation expected = starting_op;
  expected.set_done(true);
  expected.mutable_metadata()->PackFrom(instance);

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
      .WillRepeatedly([&](CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "PollThenExhaustedPollingPolicyWithFailure");
        return make_ready_future(
            StatusOr<Operation>(Status{StatusCode::kUnavailable, "try-again"}));
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  OptionsSpan span(
      Options{}.set<StringOption>("PollThenExhaustedPollingPolicyWithFailure"));
  auto pending = AsyncPollingLoop(
      cq, make_ready_future(make_status_or(starting_op)), MakePoll(mock),
      MakeCancel(mock), std::move(policy), "test-function");
  OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  ASSERT_THAT(actual,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
}

TEST(AsyncPollingLoopTest, PollLifetime) {
  Instance instance;
  instance.set_name("test-instance-name");
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");
  google::longrunning::Operation expected = starting_op;
  expected.set_done(true);
  expected.mutable_metadata()->PackFrom(instance);

  using TimerType = StatusOr<std::chrono::system_clock::time_point>;
  AsyncSequencer<TimerType> timer_sequencer;
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .Times(4)
      .WillRepeatedly(
          [&](std::chrono::nanoseconds) { return timer_sequencer.PushBack(); });
  CompletionQueue cq(mock_cq);

  AsyncSequencer<StatusOr<Operation>> get_sequencer;
  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .Times(4)
      .WillRepeatedly([&](CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(), "PollLifetime");
        return get_sequencer.PushBack();
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillRepeatedly(Return(true));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  OptionsSpan span(Options{}.set<StringOption>("PollLifetime"));
  auto pending = AsyncPollingLoop(
      cq, make_ready_future(make_status_or(starting_op)), MakePoll(mock),
      MakeCancel(mock), std::move(policy), "test-function");

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

  using TimerType = StatusOr<std::chrono::system_clock::time_point>;
  AsyncSequencer<TimerType> timer_sequencer;
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .Times(AtLeast(1))
      .WillRepeatedly(
          [&](std::chrono::nanoseconds) { return timer_sequencer.PushBack(); });
  CompletionQueue cq(mock_cq);

  AsyncSequencer<StatusOr<Operation>> get_sequencer;
  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .Times(AtLeast(1))
      .WillRepeatedly([&](CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "PollThenCancelDuringTimer");
        return get_sequencer.PushBack();
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::CancelOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "PollThenCancelDuringTimer");
        return make_ready_future(Status{});
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure)
      .Times(2)
      .WillRepeatedly([](Status const& status) {
        return status.code() != StatusCode::kCancelled;
      });
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  OptionsSpan span(Options{}.set<StringOption>("PollThenCancelDuringTimer"));
  auto pending = AsyncPollingLoop(
      cq, make_ready_future(make_status_or(starting_op)), MakePoll(mock),
      MakeCancel(mock), std::move(policy), "test-function");

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

  using TimerType = StatusOr<std::chrono::system_clock::time_point>;
  AsyncSequencer<TimerType> timer_sequencer;
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .Times(AtLeast(1))
      .WillRepeatedly(
          [&](std::chrono::nanoseconds) { return timer_sequencer.PushBack(); });
  CompletionQueue cq(mock_cq);

  AsyncSequencer<StatusOr<Operation>> get_sequencer;
  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .Times(AtLeast(1))
      .WillRepeatedly([&](CompletionQueue&,
                          std::unique_ptr<grpc::ClientContext>,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "PollThenCancelDuringPoll");
        return get_sequencer.PushBack();
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::CancelOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(),
                  "PollThenCancelDuringPoll");
        return make_ready_future(Status{});
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure)
      .Times(2)
      .WillRepeatedly([](Status const& status) {
        return status.code() != StatusCode::kCancelled;
      });
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  OptionsSpan span(Options{}.set<StringOption>("PollThenCancelDuringPoll"));
  auto pending = AsyncPollingLoop(
      cq, make_ready_future(make_status_or(starting_op)), MakePoll(mock),
      MakeCancel(mock), std::move(policy), "test-function");

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
      .WillOnce([](CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext> context,
                   google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(), "ConfigurePollContext");
        // Ensure that our options have taken affect on the ClientContext before
        // we start using it.
        EXPECT_EQ(GRPC_COMPRESS_DEFLATE, context->compression_algorithm());
        return make_ready_future(StatusOr<Operation>(Status{
            StatusCode::kCancelled, "test-function: operation cancelled"}));
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([](CompletionQueue&,
                   std::unique_ptr<grpc::ClientContext> context,
                   google::longrunning::CancelOperationRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<StringOption>(), "ConfigurePollContext");
        // Ensure that our options have taken affect on the ClientContext before
        // we start using it.
        EXPECT_EQ(GRPC_COMPRESS_DEFLATE, context->compression_algorithm());
        EXPECT_EQ(request.name(), "test-op-name");
        return make_ready_future(Status{});
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
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
  OptionsSpan span(Options{}
                       .set<GrpcSetupPollOption>(setup_poll)
                       .set<StringOption>("ConfigurePollContext"));
  promise<StatusOr<google::longrunning::Operation>> p;
  auto pending =
      AsyncPollingLoop(cq, p.get_future(), MakePoll(mock), MakeCancel(mock),
                       std::move(policy), "test-function");
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

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/internal/async_polling_loop.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::bigtable::admin::v2::Instance;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::longrunning::Operation;
using ::testing::AtLeast;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Return;

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
        return make_ready_future(make_status_or(expected));
      });
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
        return make_ready_future(make_status_or(starting_op));
      })
      .WillOnce([](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                   google::longrunning::GetOperationRequest const&) {
        return make_ready_future(
            StatusOr<Operation>(Status(StatusCode::kUnavailable, "try-again")));
      })
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::GetOperationRequest const&) {
        return make_ready_future(make_status_or(starting_op));
      })
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::GetOperationRequest const&) {
        return make_ready_future(make_status_or(expected));
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillRepeatedly(Return(true));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  auto actual =
      AsyncPollingLoop(cq, make_ready_future(make_status_or(starting_op)),
                       MakePoll(mock), MakeCancel(mock), std::move(policy),
                       "test-function")
          .get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncPollingLoopTest, PollThenExhaustedPollingPolicy) {
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
        return make_ready_future(make_status_or(starting_op));
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure)
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  auto actual =
      AsyncPollingLoop(cq, make_ready_future(make_status_or(starting_op)),
                       MakePoll(mock), MakeCancel(mock), std::move(policy),
                       "test-function")
          .get();
  EXPECT_THAT(actual,
              StatusIs(Not(Eq(StatusCode::kOk)),
                       AllOf(HasSubstr("test-function"),
                             HasSubstr("terminated by polling policy"))));
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
  auto actual =
      AsyncPollingLoop(cq, make_ready_future(make_status_or(starting_op)),
                       MakePoll(mock), MakeCancel(mock), std::move(policy),
                       "test-function")
          .get();
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
        return get_sequencer.PushBack();
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillRepeatedly(Return(true));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  auto pending = AsyncPollingLoop(
      cq, make_ready_future(make_status_or(starting_op)), MakePoll(mock),
      MakeCancel(mock), std::move(policy), "test-function");

  for (int i = 0; i != 3; ++i) {
    timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
    get_sequencer.PopFront().set_value(starting_op);
  }
  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  get_sequencer.PopFront().set_value(expected);
  auto actual = pending.get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncPollingLoopTest, PollThenCancelDuringTimer) {
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
        return get_sequencer.PushBack();
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::CancelOperationRequest const&) {
        return make_ready_future(Status{});
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillRepeatedly(Return(true));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  auto pending = AsyncPollingLoop(
      cq, make_ready_future(make_status_or(starting_op)), MakePoll(mock),
      MakeCancel(mock), std::move(policy), "test-function");

  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  get_sequencer.PopFront().set_value(starting_op);
  auto t = timer_sequencer.PopFront();
  pending.cancel();
  t.set_value(std::chrono::system_clock::now());

  auto actual = pending.get();
  EXPECT_THAT(actual, StatusIs(Not(Eq(StatusCode::kOk)),
                               AllOf(HasSubstr("test-function"),
                                     HasSubstr("terminated via cancel"))));
}

TEST(AsyncPollingLoopTest, PollThenCancelDuringPoll) {
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
        return get_sequencer.PushBack();
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::CancelOperationRequest const&) {
        return make_ready_future(Status{});
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillRepeatedly(Return(true));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  auto pending = AsyncPollingLoop(
      cq, make_ready_future(make_status_or(starting_op)), MakePoll(mock),
      MakeCancel(mock), std::move(policy), "test-function");

  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  get_sequencer.PopFront().set_value(starting_op);
  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  auto g = get_sequencer.PopFront();
  pending.cancel();
  g.set_value(starting_op);

  auto actual = pending.get();
  EXPECT_THAT(actual, StatusIs(Not(Eq(StatusCode::kOk)),
                               AllOf(HasSubstr("test-function"),
                                     HasSubstr("terminated via cancel"))));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

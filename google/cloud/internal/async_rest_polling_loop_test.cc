// Copyright 2023 Google LLC
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

#include "google/cloud/internal/async_rest_polling_loop.h"
#include "google/cloud/internal/completion_queue_impl.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/opentelemetry.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
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
using TimerType = StatusOr<std::chrono::system_clock::time_point>;

struct StringOption {
  using Type = std::string;
};

class MockStub {
 public:
  MOCK_METHOD(future<StatusOr<google::longrunning::Operation>>,
              AsyncGetOperation,
              (CompletionQueue&, std::unique_ptr<RestContext>,
               google::longrunning::GetOperationRequest const&),
              ());

  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue&, std::unique_ptr<RestContext>,
               google::longrunning::CancelOperationRequest const&),
              ());
};

class MockPollingPolicy : public PollingPolicy {
 public:
  MOCK_METHOD(std::unique_ptr<PollingPolicy>, clone, (), (const, override));
  MOCK_METHOD(bool, OnFailure, (Status const&), (override));
  MOCK_METHOD(std::chrono::milliseconds, WaitPeriod, (), (override));
};

AsyncRestPollLongRunningOperation<google::longrunning::Operation,
                                  google::longrunning::GetOperationRequest>
MakePoll(std::shared_ptr<MockStub> const& mock) {
  return [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                google::longrunning::GetOperationRequest const& request) {
    return mock->AsyncGetOperation(cq, std::move(context), request);
  };
}

AsyncRestCancelLongRunningOperation<google::longrunning::CancelOperationRequest>
MakeCancel(std::shared_ptr<MockStub> const& mock) {
  return [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                google::longrunning::CancelOperationRequest const& request) {
    return mock->AsyncCancelOperation(cq, std::move(context), request);
  };
}

TEST(AsyncRestPollingLoopTest, ImmediateSuccess) {
  Instance expected;
  expected.set_name("test-instance-name");
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
  auto actual =
      AsyncRestPollingLoopAip151(
          std::move(cq), make_ready_future(make_status_or(op)), MakePoll(mock),
          MakeCancel(mock), std::move(policy), "test-function")
          .get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(op));
}

TEST(AsyncRestPollingLoopTest, ImmediateCancel) {
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
      .WillOnce([](CompletionQueue&, std::unique_ptr<RestContext>,
                   google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "ImmediateCancel");
        return make_ready_future(
            StatusOr<google::longrunning::Operation>(Status{
                StatusCode::kCancelled, "test-function: operation cancelled"}));
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([](CompletionQueue&, std::unique_ptr<RestContext>,
                   google::longrunning::CancelOperationRequest const& request) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "ImmediateCancel");
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

  internal::OptionsSpan span(Options{}.set<StringOption>("ImmediateCancel"));
  promise<StatusOr<google::longrunning::Operation>> p;
  auto pending = AsyncRestPollingLoopAip151(std::move(cq), p.get_future(),
                                            MakePoll(mock), MakeCancel(mock),
                                            std::move(policy), "test-function");
  {
    internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
    pending.cancel();
  }
  p.set_value(starting_op);
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled,
                               AllOf(HasSubstr("test-function"),
                                     HasSubstr("operation cancelled"))));
}

TEST(AsyncRestPollingLoopTest, PollThenSuccess) {
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
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "PollThenSuccess");
        return make_ready_future(make_status_or(expected));
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  internal::OptionsSpan span(Options{}.set<StringOption>("PollThenSuccess"));
  auto pending = AsyncRestPollingLoopAip151(
      std::move(cq), make_ready_future(make_status_or(starting_op)),
      MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function");
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncRestPollingLoopTest, PollThenTimerError) {
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
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  auto actual =
      AsyncRestPollingLoopAip151(
          std::move(cq), make_ready_future(make_status_or(starting_op)),
          MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function")
          .get();
  EXPECT_THAT(actual,
              StatusIs(StatusCode::kCancelled, HasSubstr("cq shutdown")));
}

TEST(AsyncRestPollingLoopTest, PollThenEventualSuccess) {
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
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "PollThenEventualSuccess");
        return make_ready_future(make_status_or(starting_op));
      })
      .WillOnce([](CompletionQueue&, std::unique_ptr<RestContext>,
                   google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "PollThenEventualSuccess");
        return make_ready_future(StatusOr<google::longrunning::Operation>(
            Status(StatusCode::kUnavailable, "try-again")));
      })
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "PollThenEventualSuccess");
        return make_ready_future(make_status_or(starting_op));
      })
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "PollThenEventualSuccess");
        return make_ready_future(make_status_or(expected));
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillRepeatedly(Return(true));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  internal::OptionsSpan span(
      Options{}.set<StringOption>("PollThenEventualSuccess"));
  auto pending = AsyncRestPollingLoopAip151(
      std::move(cq), make_ready_future(make_status_or(starting_op)),
      MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function");
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncRestPollingLoopTest, PollThenExhaustedPollingPolicy) {
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
      .WillRepeatedly([&](CompletionQueue&, std::unique_ptr<RestContext>,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "PollThenExhaustedPollingPolicy");
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
  internal::OptionsSpan span(
      Options{}.set<StringOption>("PollThenExhaustedPollingPolicy"));
  auto pending = AsyncRestPollingLoopAip151(
      std::move(cq), make_ready_future(make_status_or(starting_op)),
      MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function");
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  EXPECT_THAT(actual,
              StatusIs(Not(Eq(StatusCode::kOk)),
                       AllOf(HasSubstr("test-function"),
                             HasSubstr("terminated by polling policy"))));
}

TEST(AsyncRestPollingLoopTest, PollThenExhaustedPollingPolicyWithFailure) {
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
      .WillRepeatedly([&](CompletionQueue&, std::unique_ptr<RestContext>,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "PollThenExhaustedPollingPolicyWithFailure");
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
  internal::OptionsSpan span(
      Options{}.set<StringOption>("PollThenExhaustedPollingPolicyWithFailure"));
  auto pending = AsyncRestPollingLoopAip151(
      std::move(cq), make_ready_future(make_status_or(starting_op)),
      MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function");
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  ASSERT_THAT(actual,
              StatusIs(StatusCode::kUnavailable, HasSubstr("try-again")));
}

TEST(AsyncRestPollingLoopTest, PollLifetime) {
  Instance instance;
  instance.set_name("test-instance-name");
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");
  google::longrunning::Operation expected = starting_op;
  expected.set_done(true);
  expected.mutable_metadata()->PackFrom(instance);

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
      .WillRepeatedly([&](CompletionQueue&, std::unique_ptr<RestContext>,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "PollLifetime");
        return get_sequencer.PushBack();
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillRepeatedly(Return(true));
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));

  internal::OptionsSpan span(Options{}.set<StringOption>("PollLifetime"));
  auto pending = AsyncRestPollingLoopAip151(
      std::move(cq), make_ready_future(make_status_or(starting_op)),
      MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function");

  for (int i = 0; i != 3; ++i) {
    timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
    get_sequencer.PopFront().set_value(starting_op);
  }
  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  get_sequencer.PopFront().set_value(expected);
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncRestPollingLoopTest, PollThenCancelDuringTimer) {
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
      .WillRepeatedly([&](CompletionQueue&, std::unique_ptr<RestContext>,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "PollThenCancelDuringTimer");
        return get_sequencer.PushBack();
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    google::longrunning::CancelOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "PollThenCancelDuringTimer");
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

  internal::OptionsSpan span(
      Options{}.set<StringOption>("PollThenCancelDuringTimer"));
  auto pending = AsyncRestPollingLoopAip151(
      std::move(cq), make_ready_future(make_status_or(starting_op)),
      MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function");

  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  get_sequencer.PopFront().set_value(starting_op);
  auto t = timer_sequencer.PopFront();
  {
    internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
    pending.cancel();
  }
  t.set_value(std::chrono::system_clock::now());
  get_sequencer.PopFront().set_value(
      Status{StatusCode::kCancelled, "test-function: operation cancelled"});

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled,
                               AllOf(HasSubstr("test-function"),
                                     HasSubstr("operation cancelled"))));
}

TEST(AsyncRestPollingLoopTest, PollThenCancelDuringPoll) {
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
      .WillRepeatedly([&](CompletionQueue&, std::unique_ptr<RestContext>,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "PollThenCancelDuringPoll");
        return get_sequencer.PushBack();
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    google::longrunning::CancelOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "PollThenCancelDuringPoll");
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

  internal::OptionsSpan span(
      Options{}.set<StringOption>("PollThenCancelDuringPoll"));
  auto pending = AsyncRestPollingLoopAip151(
      std::move(cq), make_ready_future(make_status_or(starting_op)),
      MakePoll(mock), MakeCancel(mock), std::move(policy), "test-function");

  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  get_sequencer.PopFront().set_value(starting_op);
  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  auto g = get_sequencer.PopFront();
  {
    internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
    pending.cancel();
  }
  g.set_value(
      Status{StatusCode::kCancelled, "test-function: operation cancelled"});

  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled,
                               AllOf(HasSubstr("test-function"),
                                     HasSubstr("operation cancelled"))));
}

class BespokeOperationType {
 public:
  bool is_done() const { return is_done_; }
  void set_is_done(bool is_done) { is_done_ = is_done; }
  std::string const& name() const { return name_; }
  void set_name(std::string name) { name_ = std::move(name); }
  std::string* mutable_name() { return &name_; }
  bool operator==(BespokeOperationType const& other) const {
    return is_done_ == other.is_done_ && name_ == other.name_;
  }

 private:
  bool is_done_;
  std::string name_;
};

class BespokeGetOperationRequestType {
 public:
  std::string const& name() const { return name_; }
  void set_name(std::string name) { name_ = std::move(name); }

 private:
  std::string name_;
};

class BespokeCancelOperationRequestType {
 public:
  std::string const& name() const { return name_; }
  void set_name(std::string name) { name_ = std::move(name); }

 private:
  std::string name_;
};

class MockBespokeOperationStub {
 public:
  MOCK_METHOD(future<StatusOr<BespokeOperationType>>, AsyncGetOperation,
              (CompletionQueue&, std::unique_ptr<RestContext>,
               BespokeGetOperationRequestType const&),
              ());

  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue&, std::unique_ptr<RestContext>,
               BespokeCancelOperationRequestType const&),
              ());
};

AsyncRestPollLongRunningOperation<BespokeOperationType,
                                  BespokeGetOperationRequestType>
MakeBespokePoll(std::shared_ptr<MockBespokeOperationStub> const& mock) {
  return [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                BespokeGetOperationRequestType const& request) {
    return mock->AsyncGetOperation(cq, std::move(context), request);
  };
}

AsyncRestCancelLongRunningOperation<BespokeCancelOperationRequestType>
MakeBespokeCancel(std::shared_ptr<MockBespokeOperationStub> const& mock) {
  return [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                BespokeCancelOperationRequestType const& request) {
    return mock->AsyncCancelOperation(cq, std::move(context), request);
  };
}

TEST(AsyncRestPollingLoopTest, PollThenSuccessWithBespokeOperationTypes) {
  Instance instance;
  instance.set_name("test-instance-name");
  BespokeOperationType starting_op;
  starting_op.set_name("test-op-name");
  starting_op.set_is_done(false);
  BespokeOperationType expected = starting_op;
  expected.set_is_done(true);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBespokeOperationStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    BespokeGetOperationRequestType const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "PollThenSuccess");
        return make_ready_future(make_status_or(expected));
      });
  auto policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  internal::OptionsSpan span(Options{}.set<StringOption>("PollThenSuccess"));
  auto pending =
      AsyncRestPollingLoop<BespokeOperationType, BespokeGetOperationRequestType,
                           BespokeCancelOperationRequestType>(
          std::move(cq), make_ready_future(make_status_or(starting_op)),
          MakeBespokePoll(mock), MakeBespokeCancel(mock), std::move(policy),
          "test-function",
          [](BespokeOperationType const& op) { return op.is_done(); },
          [](std::string const& s, BespokeGetOperationRequestType& op) {
            op.set_name(s);
          },
          [](std::string const& s, BespokeCancelOperationRequestType& op) {
            op.set_name(s);
          });
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  StatusOr<BespokeOperationType> actual = pending.get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, Eq(expected));
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

TEST(AsyncRestPollingLoopTest, TracedAsyncBackoff) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer).WillRepeatedly([](auto) {
    return make_ready_future<TimerType>(std::chrono::system_clock::now());
  });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation).WillRepeatedly([](auto, auto, auto) {
    return make_ready_future<StatusOr<google::longrunning::Operation>>(
        internal::UnavailableError("try again"));
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

  internal::OptionsSpan o(EnableTracing(Options{}));
  (void)AsyncRestPollingLoopAip151(
      cq, make_ready_future(make_status_or(starting_op)), MakePoll(mock),
      MakeCancel(mock), std::move(policy), "test-function")
      .get();

  // The polling loop waits once initially, and once for each of the three retry
  // attempts. So we expect a total of 4 backoffs.
  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans, AllOf(SizeIs(4), Each(SpanNamed("Async Backoff"))));
}

TEST(AsyncRestPollingLoopTest, SpanActiveThroughout) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  auto span = internal::MakeSpan("span");
  google::longrunning::Operation starting_op;
  starting_op.set_name("test-op-name");

  AsyncSequencer<TimerType> timer_sequencer;
  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .Times(AtLeast(1))
      .WillRepeatedly([&](std::chrono::nanoseconds) {
        EXPECT_THAT(span, IsActive());
        return timer_sequencer.PushBack();
      });
  CompletionQueue cq(mock_cq);

  AsyncSequencer<StatusOr<google::longrunning::Operation>> get_sequencer;
  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncGetOperation)
      .Times(AtLeast(1))
      .WillRepeatedly([&](CompletionQueue&, auto,
                          google::longrunning::GetOperationRequest const&) {
        EXPECT_THAT(span, IsActive());
        return get_sequencer.PushBack();
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, auto,
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

  auto scope = opentelemetry::trace::Scope(span);
  internal::OptionsSpan o(EnableTracing(Options{}));
  auto pending = AsyncRestPollingLoopAip151(
      cq, make_ready_future(make_status_or(starting_op)), MakePoll(mock),
      MakeCancel(mock), std::move(policy), "test-function");

  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  get_sequencer.PopFront().set_value(starting_op);
  timer_sequencer.PopFront().set_value(std::chrono::system_clock::now());
  auto g = get_sequencer.PopFront();
  {
    auto overlay = opentelemetry::trace::Scope(internal::MakeSpan("overlay"));
    pending.cancel();
  }
  g.set_value(internal::CancelledError("cancelled"));

  auto overlay = opentelemetry::trace::Scope(internal::MakeSpan("overlay"));
  (void)pending.get();
}

TEST(AsyncRestPollingLoopTest, TraceCapturesOperationName) {
  auto span_catcher = testing_util::InstallSpanCatcher();

  google::longrunning::Operation op;
  op.set_name("test-op-name");
  op.set_done(true);

  auto span = internal::MakeSpan("span");
  auto mock = std::make_shared<MockStub>();
  auto policy = std::make_unique<MockPollingPolicy>();
  CompletionQueue cq;

  auto scope = opentelemetry::trace::Scope(span);
  internal::OptionsSpan o(EnableTracing(Options{}));
  (void)AsyncRestPollingLoopAip151(cq, make_ready_future(make_status_or(op)),
                                   MakePoll(mock), MakeCancel(mock),
                                   std::move(policy), "test-function")
      .get();
  span->End();

  auto spans = span_catcher->GetSpans();
  EXPECT_THAT(spans,
              ElementsAre(AllOf(SpanNamed("span"),
                                SpanHasAttributes(OTelAttribute<std::string>(
                                    "gcloud.LRO_name", "test-op-name")))));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/internal/async_rest_long_running_operation.h"
#include "google/cloud/internal/extract_long_running_result.h"
#include "google/cloud/internal/retry_policy_impl.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/timestamp.pb.h>
#include <gmock/gmock.h>
#include <utility>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::longrunning::Operation;
using ::testing::Return;

using Response = google::protobuf::Timestamp;
using Request = google::protobuf::Duration;

struct StringOption {
  using Type = std::string;
};

class MockRestStub {
 public:
  MOCK_METHOD(future<StatusOr<Operation>>, AsyncCreateResponse,
              (CompletionQueue & cq, std::unique_ptr<RestContext> context,
               Options const& options, Request const&),
              ());
  MOCK_METHOD(future<StatusOr<Operation>>, AsyncGetOperation,
              (CompletionQueue & cq, std::unique_ptr<RestContext> context,
               Options const& options,
               google::longrunning::GetOperationRequest const&),
              ());
  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue & cq, std::unique_ptr<RestContext> context,
               Options const& options,
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
  return internal::LimitedErrorCountRetryPolicy<TestRetryablePolicy>(5).clone();
}

std::unique_ptr<BackoffPolicy> TestBackoffPolicy() {
  using us = std::chrono::microseconds;
  return ExponentialBackoffPolicy(us(100), us(100), 2.0).clone();
}

std::string CurrentTestName() {
  return testing::UnitTest::GetInstance()->current_test_info()->name();
}

using StartOperation =
    std::function<future<StatusOr<google::longrunning::Operation>>(
        CompletionQueue&, std::unique_ptr<RestContext>, Options const&,
        Request const&)>;

StartOperation MakeStart(std::shared_ptr<MockRestStub> const& m) {
  return [m](CompletionQueue& cq, std::unique_ptr<RestContext> context,
             Options const& options, Request const& request) {
    return m->AsyncCreateResponse(cq, std::move(context), options, request);
  };
}

AsyncRestPollLongRunningOperation<google::longrunning::Operation,
                                  google::longrunning::GetOperationRequest>
MakePoll(std::shared_ptr<MockRestStub> const& m) {
  return [m](CompletionQueue& cq, std::unique_ptr<RestContext> context,
             Options const& options,
             google::longrunning::GetOperationRequest const& request) {
    return m->AsyncGetOperation(cq, std::move(context), options, request);
  };
}

AsyncRestCancelLongRunningOperation<google::longrunning::CancelOperationRequest>
MakeCancel(std::shared_ptr<MockRestStub> const& m) {
  return [m](CompletionQueue& cq, std::unique_ptr<RestContext> context,
             Options const& options,
             google::longrunning::CancelOperationRequest const& request) {
    return m->AsyncCancelOperation(cq, std::move(context), options, request);
  };
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

  auto mock = std::make_shared<MockRestStub>();
  EXPECT_CALL(*mock, AsyncCreateResponse)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options, Request const&) {
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
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

  auto options = internal::MakeImmutableOptions(
      Options{}.set<StringOption>(CurrentTestName()));
  auto actual =
      AsyncRestLongRunningOperation<Response>(
          cq, options, std::move(request), MakeStart(mock), MakePoll(mock),
          MakeCancel(mock),
          &internal::ExtractLongRunningResultMetadata<Response>,
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
          std::move(policy), "test-function")
          .get();
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
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

  auto mock = std::make_shared<MockRestStub>();
  EXPECT_CALL(*mock, AsyncCreateResponse)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options, Request const&) {
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
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

  auto options = internal::MakeImmutableOptions(
      Options{}.set<StringOption>(CurrentTestName()));
  auto actual =
      AsyncRestLongRunningOperation<Response>(
          cq, options, std::move(request), MakeStart(mock), MakePoll(mock),
          MakeCancel(mock),
          &internal::ExtractLongRunningResultResponse<Response>,
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
          std::move(policy), "test-function")
          .get();
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
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

  auto mock = std::make_shared<MockRestStub>();
  EXPECT_CALL(*mock, AsyncCreateResponse)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options, Request const&) {
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      })
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
        return make_ready_future(StatusOr<google::longrunning::Operation>(
            Status{StatusCode::kCancelled, "cancelled"}));
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options,
                    google::longrunning::CancelOperationRequest const&) {
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
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

  auto options = internal::MakeImmutableOptions(
      Options{}.set<StringOption>(CurrentTestName()));
  auto pending = AsyncRestLongRunningOperation<Response>(
      cq, options, std::move(request), MakeStart(mock), MakePoll(mock),
      MakeCancel(mock), &internal::ExtractLongRunningResultMetadata<Response>,
      TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
      std::move(policy), "test-function");

  // Wait until the polling loop is backing off for a second time.
  timer.PopFront().set_value();
  auto t = timer.PopFront();
  {
    // cancel the long running operation
    internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
    pending.cancel();
  }
  // release timer
  t.set_value();
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled));
}

using StartOperationImplicitOptions =
    std::function<future<StatusOr<google::longrunning::Operation>>(
        CompletionQueue&, std::unique_ptr<RestContext>, Request const&)>;

StartOperationImplicitOptions MakeStartImplicitOptions(
    std::shared_ptr<MockRestStub> const& m) {
  return [m](CompletionQueue& cq, std::unique_ptr<RestContext> context,
             Request const& request) {
    return m->AsyncCreateResponse(cq, std::move(context),
                                  internal::CurrentOptions(), request);
  };
}

AsyncRestPollLongRunningOperationImplicitOptions<
    google::longrunning::Operation, google::longrunning::GetOperationRequest>
MakePollImplicitOptions(std::shared_ptr<MockRestStub> const& m) {
  return [m](CompletionQueue& cq, std::unique_ptr<RestContext> context,
             google::longrunning::GetOperationRequest const& request) {
    return m->AsyncGetOperation(cq, std::move(context),
                                internal::CurrentOptions(), request);
  };
}

AsyncRestCancelLongRunningOperationImplicitOptions<
    google::longrunning::CancelOperationRequest>
MakeCancelImplicitOptions(std::shared_ptr<MockRestStub> const& m) {
  return [m](CompletionQueue& cq, std::unique_ptr<RestContext> context,
             google::longrunning::CancelOperationRequest const& request) {
    return m->AsyncCancelOperation(cq, std::move(context),
                                   internal::CurrentOptions(), request);
  };
}

TEST(AsyncLongRunningTest, RequestPollThenSuccessMetadataImplicitOptions) {
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

  auto mock = std::make_shared<MockRestStub>();
  EXPECT_CALL(*mock, AsyncCreateResponse)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options, Request const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  CurrentTestName());
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  CurrentTestName());
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
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

  internal::OptionsSpan span(Options{}.set<StringOption>(CurrentTestName()));
  auto actual =
      AsyncRestLongRunningOperation<Response>(
          cq, std::move(request), MakeStartImplicitOptions(mock),
          MakePollImplicitOptions(mock), MakeCancelImplicitOptions(mock),
          &internal::ExtractLongRunningResultMetadata<Response>,
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
          std::move(policy), "test-function")
          .get();
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncLongRunningTest, RequestPollThenSuccessResponseImplicitOptions) {
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

  auto mock = std::make_shared<MockRestStub>();
  EXPECT_CALL(*mock, AsyncCreateResponse)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options, Request const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  CurrentTestName());
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  CurrentTestName());
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
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

  internal::OptionsSpan span(Options{}.set<StringOption>(CurrentTestName()));
  auto actual =
      AsyncRestLongRunningOperation<Response>(
          cq, std::move(request), MakeStartImplicitOptions(mock),
          MakePollImplicitOptions(mock), MakeCancelImplicitOptions(mock),
          &internal::ExtractLongRunningResultResponse<Response>,
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
          std::move(policy), "test-function")
          .get();
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncLongRunningTest, RequestPollThenCancelImplicitOptions) {
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

  auto mock = std::make_shared<MockRestStub>();
  EXPECT_CALL(*mock, AsyncCreateResponse)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options, Request const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  CurrentTestName());
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  CurrentTestName());
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      })
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  CurrentTestName());
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
        return make_ready_future(StatusOr<google::longrunning::Operation>(
            Status{StatusCode::kCancelled, "cancelled"}));
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options,
                    google::longrunning::CancelOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  CurrentTestName());
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
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

  internal::OptionsSpan span(Options{}.set<StringOption>(CurrentTestName()));
  auto pending = AsyncRestLongRunningOperation<Response>(
      cq, std::move(request), MakeStartImplicitOptions(mock),
      MakePollImplicitOptions(mock), MakeCancelImplicitOptions(mock),
      &internal::ExtractLongRunningResultMetadata<Response>, TestRetryPolicy(),
      TestBackoffPolicy(), Idempotency::kIdempotent, std::move(policy),
      "test-function");

  // Wait until the polling loop is backing off for a second time.
  timer.PopFront().set_value();
  auto t = timer.PopFront();
  {
    // cancel the long running operation
    internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
    pending.cancel();
  }
  // release timer
  t.set_value();
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  auto actual = pending.get();
  EXPECT_THAT(actual, StatusIs(StatusCode::kCancelled));
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
  MOCK_METHOD(future<StatusOr<BespokeOperationType>>, AsyncCreateResponse,
              (CompletionQueue & cq, std::unique_ptr<RestContext> context,
               Options const&, Request const&),
              ());

  MOCK_METHOD(future<StatusOr<BespokeOperationType>>, AsyncGetOperation,
              (CompletionQueue&, std::unique_ptr<RestContext>, Options const&,
               BespokeGetOperationRequestType const&),
              ());

  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue&, std::unique_ptr<RestContext>, Options const&,
               BespokeCancelOperationRequestType const&),
              ());
};

TEST(AsyncLongRunningTest,
     RequestPollThenSuccessResponseWithBespokeOperationTypes) {
  Response expected;
  expected.set_seconds(123456);
  BespokeOperationType starting_op;
  starting_op.set_name("test-op-name");
  starting_op.set_is_done(false);
  BespokeOperationType done_op = starting_op;
  done_op.set_is_done(true);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBespokeOperationStub>();
  EXPECT_CALL(*mock, AsyncCreateResponse)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options, Request const&) {
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options,
                    BespokeGetOperationRequestType const&) {
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
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

  auto options = internal::MakeImmutableOptions(
      Options{}.set<StringOption>(CurrentTestName()));
  auto actual =
      AsyncRestLongRunningOperation<Response, BespokeOperationType,
                                    BespokeGetOperationRequestType,
                                    BespokeCancelOperationRequestType>(
          cq, options, std::move(request),
          [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                 Options const& options, Request const& request) {
            return mock->AsyncCreateResponse(cq, std::move(context), options,
                                             request);
          },
          [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                 Options const& options,
                 BespokeGetOperationRequestType const& request) {
            return mock->AsyncGetOperation(cq, std::move(context), options,
                                           request);
          },
          [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                 Options const& options,
                 BespokeCancelOperationRequestType const& request) {
            return mock->AsyncCancelOperation(cq, std::move(context), options,
                                              request);
          },
          [&](StatusOr<BespokeOperationType> const&,
              std::string const&) -> StatusOr<Response> { return expected; },
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
          std::move(policy), "test-function",
          [](BespokeOperationType const& op) { return op.is_done(); },
          [](std::string const& s, BespokeGetOperationRequestType& op) {
            op.set_name(s);
          },
          [](std::string const& s, BespokeCancelOperationRequestType& op) {
            op.set_name(s);
          })
          .get();
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncLongRunningTest,
     RequestPollThenSuccessResponseWithBespokeOperationTypesImplicitOptions) {
  Response expected;
  expected.set_seconds(123456);
  BespokeOperationType starting_op;
  starting_op.set_name("test-op-name");
  starting_op.set_is_done(false);
  BespokeOperationType done_op = starting_op;
  done_op.set_is_done(true);

  auto mock_cq = std::make_shared<MockCompletionQueueImpl>();
  EXPECT_CALL(*mock_cq, MakeRelativeTimer)
      .WillOnce([](std::chrono::nanoseconds) {
        return make_ready_future(
            make_status_or(std::chrono::system_clock::now()));
      });
  CompletionQueue cq(mock_cq);

  auto mock = std::make_shared<MockBespokeOperationStub>();
  EXPECT_CALL(*mock, AsyncCreateResponse)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options, Request const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  CurrentTestName());
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    Options const& options,
                    BespokeGetOperationRequestType const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  CurrentTestName());
        EXPECT_EQ(options.get<StringOption>(), CurrentTestName());
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

  internal::OptionsSpan span(Options{}.set<StringOption>(CurrentTestName()));
  auto actual =
      AsyncRestLongRunningOperation<Response, BespokeOperationType,
                                    BespokeGetOperationRequestType,
                                    BespokeCancelOperationRequestType>(
          cq, std::move(request),
          [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                 Request const& request) {
            return mock->AsyncCreateResponse(
                cq, std::move(context), internal::CurrentOptions(), request);
          },
          [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                 BespokeGetOperationRequestType const& request) {
            return mock->AsyncGetOperation(cq, std::move(context),
                                           internal::CurrentOptions(), request);
          },
          [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                 BespokeCancelOperationRequestType const& request) {
            return mock->AsyncCancelOperation(
                cq, std::move(context), internal::CurrentOptions(), request);
          },
          [&](StatusOr<BespokeOperationType> const&,
              std::string const&) -> StatusOr<Response> { return expected; },
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
          std::move(policy), "test-function",
          [](BespokeOperationType const& op) { return op.is_done(); },
          [](std::string const& s, BespokeGetOperationRequestType& op) {
            op.set_name(s);
          },
          [](std::string const& s, BespokeCancelOperationRequestType& op) {
            op.set_name(s);
          })
          .get();
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

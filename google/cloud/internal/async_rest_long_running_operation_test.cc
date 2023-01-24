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
#include "google/cloud/options.h"
#include "google/cloud/testing_util/async_sequencer.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::bigtable::admin::v2::CreateInstanceRequest;
using ::google::bigtable::admin::v2::Instance;
using ::google::cloud::testing_util::AsyncSequencer;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::cloud::testing_util::StatusIs;
using ::google::longrunning::Operation;
using ::testing::Return;

struct StringOption {
  using Type = std::string;
};

class MockRestStub {
 public:
  MOCK_METHOD(future<StatusOr<Operation>>, AsyncCreateInstance,
              (CompletionQueue & cq, std::unique_ptr<RestContext> context,
               CreateInstanceRequest const&),
              ());
  MOCK_METHOD(future<StatusOr<Operation>>, AsyncGetOperation,
              (CompletionQueue & cq, std::unique_ptr<RestContext> context,
               google::longrunning::GetOperationRequest const&),
              ());
  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue & cq, std::unique_ptr<RestContext> context,
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

std::unique_ptr<internal::RetryPolicy> TestRetryPolicy() {
  return internal::LimitedErrorCountRetryPolicy<TestRetryablePolicy>(5).clone();
}

std::unique_ptr<BackoffPolicy> TestBackoffPolicy() {
  using us = std::chrono::microseconds;
  return ExponentialBackoffPolicy(us(100), us(100), 2.0).clone();
}

using StartOperation =
    std::function<future<StatusOr<google::longrunning::Operation>>(
        CompletionQueue&, std::unique_ptr<RestContext>,
        CreateInstanceRequest const&)>;

StartOperation MakeStart(std::shared_ptr<MockRestStub> const& m) {
  return [m](CompletionQueue& cq, std::unique_ptr<RestContext> context,
             CreateInstanceRequest const& request) {
    return m->AsyncCreateInstance(cq, std::move(context), request);
  };
}

AsyncRestPollLongRunningOperation MakePoll(
    std::shared_ptr<MockRestStub> const& m) {
  return [m](CompletionQueue& cq, std::unique_ptr<RestContext> context,
             google::longrunning::GetOperationRequest const& request) {
    return m->AsyncGetOperation(cq, std::move(context), request);
  };
}

AsyncRestCancelLongRunningOperation MakeCancel(
    std::shared_ptr<MockRestStub> const& m) {
  return [m](CompletionQueue& cq, std::unique_ptr<RestContext> context,
             google::longrunning::CancelOperationRequest const& request) {
    return m->AsyncCancelOperation(cq, std::move(context), request);
  };
}

TEST(AsyncLongRunningTest, RequestPollThenSuccessMetadata) {
  Instance expected;
  expected.set_name("test-instance-name");
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
  EXPECT_CALL(*mock, AsyncCreateInstance)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    CreateInstanceRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "RequestPollThenSuccessMetadata");
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "RequestPollThenSuccessMetadata");
        return make_ready_future(make_status_or(done_op));
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  CreateInstanceRequest request;
  request.set_parent("test-parent");
  request.set_instance_id("test-instance-id");
  internal::OptionsSpan span(
      Options{}.set<StringOption>("RequestPollThenSuccessMetadata"));
  auto actual =
      AsyncRestLongRunningOperation<Instance>(
          cq, std::move(request), MakeStart(mock), MakePoll(mock),
          MakeCancel(mock),
          &internal::ExtractLongRunningResultMetadata<Instance>,
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
          std::move(policy), "test-function")
          .get();
  internal::OptionsSpan overlay(Options{}.set<StringOption>("uh-oh"));
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncLongRunningTest, RequestPollThenSuccessResponse) {
  Instance expected;
  expected.set_name("test-instance-name");
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
  EXPECT_CALL(*mock, AsyncCreateInstance)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    CreateInstanceRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "RequestPollThenSuccessResponse");
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "RequestPollThenSuccessResponse");
        return make_ready_future(make_status_or(done_op));
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).Times(0);
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  CreateInstanceRequest request;
  request.set_parent("test-parent");
  request.set_instance_id("test-instance-id");
  internal::OptionsSpan span(
      Options{}.set<StringOption>("RequestPollThenSuccessResponse"));
  auto actual =
      AsyncRestLongRunningOperation<Instance>(
          cq, std::move(request), MakeStart(mock), MakePoll(mock),
          MakeCancel(mock),
          &internal::ExtractLongRunningResultResponse<Instance>,
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
  EXPECT_CALL(*mock, AsyncCreateInstance)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    CreateInstanceRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "RequestPollThenCancel");
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "RequestPollThenCancel");
        return make_ready_future(make_status_or(starting_op));
      })
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    google::longrunning::GetOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "RequestPollThenCancel");
        return make_ready_future(StatusOr<google::longrunning::Operation>(
            Status{StatusCode::kCancelled, "cancelled"}));
      });
  EXPECT_CALL(*mock, AsyncCancelOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    google::longrunning::CancelOperationRequest const&) {
        EXPECT_EQ(internal::CurrentOptions().get<StringOption>(),
                  "RequestPollThenCancel");
        return make_ready_future(Status{});
      });
  auto policy = absl::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*policy, clone()).Times(0);
  EXPECT_CALL(*policy, OnFailure).WillRepeatedly([](Status const& status) {
    return status.code() != StatusCode::kCancelled;
  });
  EXPECT_CALL(*policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  CreateInstanceRequest request;
  request.set_parent("test-parent");
  request.set_instance_id("test-instance-id");
  internal::OptionsSpan span(
      Options{}.set<StringOption>("RequestPollThenCancel"));
  auto pending = AsyncRestLongRunningOperation<Instance>(
      cq, std::move(request), MakeStart(mock), MakePoll(mock), MakeCancel(mock),
      &internal::ExtractLongRunningResultMetadata<Instance>, TestRetryPolicy(),
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

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

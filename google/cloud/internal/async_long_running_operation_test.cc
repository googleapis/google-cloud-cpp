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

#include "google/cloud/internal/async_long_running_operation.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/mock_completion_queue_impl.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::google::bigtable::admin::v2::CreateInstanceRequest;
using ::google::bigtable::admin::v2::Instance;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
using ::google::longrunning::Operation;
using ::testing::Return;

class MockStub {
 public:
  MOCK_METHOD(future<StatusOr<Operation>>, AsyncCreateInstance,
              (CompletionQueue & cq, std::unique_ptr<grpc::ClientContext>,
               CreateInstanceRequest const&),
              ());
  MOCK_METHOD(future<StatusOr<Operation>>, AsyncGetOperation,
              (CompletionQueue & cq, std::unique_ptr<grpc::ClientContext>,
               google::longrunning::GetOperationRequest const&),
              ());
  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue & cq, std::unique_ptr<grpc::ClientContext>,
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

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncCreateInstance)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    CreateInstanceRequest const&) {
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::GetOperationRequest const&) {
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
  auto actual =
      AsyncLongRunningOperation<Instance>(
          cq, std::move(request),
          [mock](CompletionQueue& cq,
                 std::unique_ptr<grpc::ClientContext> context,
                 CreateInstanceRequest const& request) {
            return mock->AsyncCreateInstance(cq, std::move(context), request);
          },
          [mock](CompletionQueue& cq,
                 std::unique_ptr<grpc::ClientContext> context,
                 google::longrunning::GetOperationRequest const& request) {
            return mock->AsyncGetOperation(cq, std::move(context), request);
          },
          [mock](CompletionQueue& cq,
                 std::unique_ptr<grpc::ClientContext> context,
                 google::longrunning::CancelOperationRequest const& request) {
            return mock->AsyncCancelOperation(cq, std::move(context), request);
          },
          &ExtractLongRunningResultMetadata<Instance>, TestRetryPolicy(),
          TestBackoffPolicy(), Idempotency::kIdempotent, std::move(policy),
          "test-function")
          .get();
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

  auto mock = std::make_shared<MockStub>();
  EXPECT_CALL(*mock, AsyncCreateInstance)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    CreateInstanceRequest const&) {
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<grpc::ClientContext>,
                    google::longrunning::GetOperationRequest const&) {
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
  auto actual =
      AsyncLongRunningOperation<Instance>(
          cq, std::move(request),
          [mock](CompletionQueue& cq,
                 std::unique_ptr<grpc::ClientContext> context,
                 CreateInstanceRequest const& request) {
            return mock->AsyncCreateInstance(cq, std::move(context), request);
          },
          [mock](CompletionQueue& cq,
                 std::unique_ptr<grpc::ClientContext> context,
                 google::longrunning::GetOperationRequest const& request) {
            return mock->AsyncGetOperation(cq, std::move(context), request);
          },
          [mock](CompletionQueue& cq,
                 std::unique_ptr<grpc::ClientContext> context,
                 google::longrunning::CancelOperationRequest const& request) {
            return mock->AsyncCancelOperation(cq, std::move(context), request);
          },
          &ExtractLongRunningResultResponse<Instance>, TestRetryPolicy(),
          TestBackoffPolicy(), Idempotency::kIdempotent, std::move(policy),
          "test-function")
          .get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

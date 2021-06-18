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

#include "google/cloud/internal/async_long_running.h"
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
using ::google::cloud::testing_util::StatusIs;
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

TEST(AsyncLongRunningTest, ExtractValueDoneWithSuccess) {
  Instance expected;
  expected.set_name("test-instance-admin");
  google::longrunning::Operation op;
  op.set_done(true);
  op.mutable_metadata()->PackFrom(expected);
  auto const actual = ExtractLongRunningResult<Instance>(op, "test-function");
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(AsyncLongRunningTest, ExtractValueDoneWithError) {
  google::longrunning::Operation op;
  op.set_done(true);
  op.mutable_error()->set_code(grpc::StatusCode::PERMISSION_DENIED);
  op.mutable_error()->set_message("uh-oh");
  auto const actual = ExtractLongRunningResult<Instance>(op, "test-function");
  EXPECT_THAT(actual, StatusIs(StatusCode::kPermissionDenied));
}

TEST(AsyncLongRunningTest, ExtractValueDoneWithoutResult) {
  google::longrunning::Operation op;
  op.set_done(true);
  auto const actual = ExtractLongRunningResult<Instance>(op, "test-function");
  EXPECT_THAT(actual, StatusIs(StatusCode::kInternal));
}

TEST(AsyncLongRunningTest, ExtractValueDoneWithInvalidContent) {
  google::longrunning::Operation op;
  op.set_done(true);
  op.mutable_metadata()->PackFrom(google::protobuf::Empty{});
  auto const actual = ExtractLongRunningResult<Instance>(op, "test-function");
  EXPECT_THAT(actual, StatusIs(StatusCode::kInternal));
}

TEST(AsyncLongRunningTest, ExtractValueError) {
  auto const expected = Status{StatusCode::kPermissionDenied, "uh-oh"};
  auto const actual =
      ExtractLongRunningResult<Instance>(expected, "test-function");
  ASSERT_THAT(actual, Not(IsOk()));
  EXPECT_EQ(expected, actual.status());
}

TEST(AsyncLongRunningTest, RequestPollThenSuccess) {
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
          TestRetryPolicy(), TestBackoffPolicy(), Idempotency::kIdempotent,
          std::move(policy), "test-function")
          .get();
  ASSERT_THAT(actual, IsOk());
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

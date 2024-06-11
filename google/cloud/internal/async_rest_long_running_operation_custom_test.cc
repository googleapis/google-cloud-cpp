// Copyright 2024 Google LLC
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

#include "google/cloud/internal/async_rest_long_running_operation_custom.h"
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

using ::google::cloud::internal::ImmutableOptions;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::MockCompletionQueueImpl;
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
               ImmutableOptions options, Request const&),
              ());
  MOCK_METHOD(future<StatusOr<Operation>>, AsyncGetOperation,
              (CompletionQueue & cq, std::unique_ptr<RestContext> context,
               ImmutableOptions options,
               google::longrunning::GetOperationRequest const&),
              ());
  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue & cq, std::unique_ptr<RestContext> context,
               ImmutableOptions options,
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
               ImmutableOptions, Request const&),
              ());

  MOCK_METHOD(future<StatusOr<BespokeOperationType>>, AsyncGetOperation,
              (CompletionQueue&, std::unique_ptr<RestContext>, ImmutableOptions,
               BespokeGetOperationRequestType const&),
              ());

  MOCK_METHOD(future<Status>, AsyncCancelOperation,
              (CompletionQueue&, std::unique_ptr<RestContext>, ImmutableOptions,
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
                    ImmutableOptions const& options, Request const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(starting_op));
      });
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    ImmutableOptions const& options,
                    BespokeGetOperationRequestType const&) {
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

  auto options = internal::MakeImmutableOptions(
      Options{}.set<StringOption>(CurrentTestName()));
  auto actual =
      AsyncRestLongRunningOperation<Response, BespokeOperationType,
                                    BespokeGetOperationRequestType,
                                    BespokeCancelOperationRequestType>(
          cq, options, std::move(request),
          [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                 internal::ImmutableOptions options, Request const& request) {
            return mock->AsyncCreateResponse(cq, std::move(context),
                                             std::move(options), request);
          },
          [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                 internal::ImmutableOptions options,
                 BespokeGetOperationRequestType const& request) {
            return mock->AsyncGetOperation(cq, std::move(context),
                                           std::move(options), request);
          },
          [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                 internal::ImmutableOptions options,
                 BespokeCancelOperationRequestType const& request) {
            return mock->AsyncCancelOperation(cq, std::move(context),
                                              std::move(options), request);
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
     AwaitPollThenSuccessResponseWithBespokeOperationTypes) {
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
  EXPECT_CALL(*mock, AsyncGetOperation)
      .WillOnce([&](CompletionQueue&, std::unique_ptr<RestContext>,
                    ImmutableOptions const& options,
                    BespokeGetOperationRequestType const&) {
        EXPECT_EQ(options->get<StringOption>(), CurrentTestName());
        return make_ready_future(make_status_or(done_op));
      });
  auto polling_policy = std::make_unique<MockPollingPolicy>();
  EXPECT_CALL(*polling_policy, clone()).Times(0);
  EXPECT_CALL(*polling_policy, OnFailure).Times(0);
  EXPECT_CALL(*polling_policy, WaitPeriod)
      .WillRepeatedly(Return(std::chrono::milliseconds(1)));
  auto options = internal::MakeImmutableOptions(
      Options{}.set<StringOption>(CurrentTestName()));
  auto actual =
      AsyncRestAwaitLongRunningOperation<Response, BespokeOperationType,
                                         BespokeGetOperationRequestType,
                                         BespokeCancelOperationRequestType>(
          cq, options, starting_op,
          [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                 internal::ImmutableOptions options,
                 BespokeGetOperationRequestType const& request) {
            return mock->AsyncGetOperation(cq, std::move(context),
                                           std::move(options), request);
          },
          [mock](CompletionQueue& cq, std::unique_ptr<RestContext> context,
                 internal::ImmutableOptions options,
                 BespokeCancelOperationRequestType const& request) {
            return mock->AsyncCancelOperation(cq, std::move(context),
                                              std::move(options), request);
          },
          [&](StatusOr<BespokeOperationType> const&,
              std::string const&) -> StatusOr<Response> { return expected; },
          std::move(polling_policy), "test-function",
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

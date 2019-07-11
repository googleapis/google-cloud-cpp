// Copyright 2019 Google LLC
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

#include "google/cloud/spanner/internal/polling_loop.h"
#include "google/cloud/testing_util/assert_ok.h"
#include <google/protobuf/struct.pb.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;

std::unique_ptr<PollingPolicy> TestPollingPolicy() {
  using Policy = GenericPollingPolicy<LimitedErrorCountRetryPolicy,
                                      ExponentialBackoffPolicy>;
  return Policy(LimitedErrorCountRetryPolicy(5),
                ExponentialBackoffPolicy(std::chrono::microseconds(1),
                                         std::chrono::microseconds(5), 2.0))
      .clone();
}

TEST(PollingLoopTest, ImmediateSuccess) {
  google::protobuf::Value expected;
  expected.set_string_value("42");

  google::longrunning::Operation operation;
  operation.set_name("test-operation");
  operation.set_done(true);
  operation.mutable_response()->PackFrom(expected);

  StatusOr<google::protobuf::Value> actual =
      PollingLoop<google::protobuf::Value>(
          TestPollingPolicy(),
          [](grpc::ClientContext&,
             google::longrunning::GetOperationRequest const&) {
            // The polling operation should not be called for requests
            // FAIL() fails here (he he) because it has an embedded `return;`
            EXPECT_TRUE(false);
            return make_status_or(google::longrunning::Operation{});
          },
          operation, "location");
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(expected.string_value(), actual->string_value());
}

TEST(PollingLoopTest, ImmediateFailure) {
  google::rpc::Status error;
  error.set_code(static_cast<int>(StatusCode::kResourceExhausted));
  error.set_message("cannot complete operation");
  google::longrunning::Operation operation;
  operation.set_name("test-operation");
  operation.set_done(true);
  *operation.mutable_error() = error;

  StatusOr<google::protobuf::Value> actual =
      PollingLoop<google::protobuf::Value>(
          TestPollingPolicy(),
          [](grpc::ClientContext&,
             google::longrunning::GetOperationRequest const&) {
            // The polling operation should not be called for requests
            EXPECT_TRUE(false);
            return make_status_or(google::longrunning::Operation{});
          },
          operation, "location");
  EXPECT_EQ(StatusCode::kResourceExhausted, actual.status().code());
  EXPECT_EQ(error.message(), actual.status().message());
}

TEST(PollingLoopTest, SuccessWithSuccessfulPolling) {
  google::protobuf::Value expected;
  expected.set_string_value("42");

  google::longrunning::Operation operation;
  operation.set_name("test-operation");
  operation.set_done(false);

  int counter = 3;
  StatusOr<google::protobuf::Value> actual =
      PollingLoop<google::protobuf::Value>(
          TestPollingPolicy(),
          [expected, &counter](
              grpc::ClientContext&,
              google::longrunning::GetOperationRequest const& r) {
            google::longrunning::Operation op;
            op.set_name(r.name());
            if (--counter != 0) {
              op.set_done(false);
            } else {
              op.set_done(true);
              op.mutable_response()->PackFrom(expected);
            }
            return make_status_or(op);
          },
          operation, "location");
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(expected.string_value(), actual->string_value());
}

TEST(PollingLoopTest, FailureWithSuccessfulPolling) {
  google::rpc::Status error;
  error.set_code(static_cast<int>(StatusCode::kResourceExhausted));
  error.set_message("cannot complete operation");

  google::longrunning::Operation operation;
  operation.set_name("test-operation");
  operation.set_done(false);

  int counter = 3;
  StatusOr<google::protobuf::Value> actual =
      PollingLoop<google::protobuf::Value>(
          TestPollingPolicy(),
          [error, &counter](grpc::ClientContext&,
                            google::longrunning::GetOperationRequest const& r) {
            google::longrunning::Operation op;
            op.set_name(r.name());
            if (--counter != 0) {
              op.set_done(false);
            } else {
              op.set_done(true);
              *op.mutable_error() = error;
            }
            return make_status_or(op);
          },
          operation, "location");
  EXPECT_EQ(StatusCode::kResourceExhausted, actual.status().code());
  EXPECT_EQ(error.message(), actual.status().message());
}

TEST(PollingLoopTest, SuccessWithTransientFailures) {
  google::protobuf::Value expected;
  expected.set_string_value("42");

  google::longrunning::Operation operation;
  operation.set_name("test-operation");
  operation.set_done(false);

  int counter = 4;
  StatusOr<google::protobuf::Value> actual =
      PollingLoop<google::protobuf::Value>(
          TestPollingPolicy(),
          [expected, &counter](
              grpc::ClientContext&,
              google::longrunning::GetOperationRequest const& r) {
            google::longrunning::Operation op;
            op.set_name(r.name());
            counter--;
            if (counter >= 2) {
              return StatusOr<google::longrunning::Operation>(
                  Status(StatusCode::kUnavailable, "try again"));
            }
            if (counter > 0) {
              op.set_done(false);
            } else {
              op.set_done(true);
              op.mutable_response()->PackFrom(expected);
            }
            return make_status_or(op);
          },
          operation, "location");
  EXPECT_STATUS_OK(actual);
  EXPECT_EQ(expected.string_value(), actual->string_value());
}

TEST(PollingLoopTest, FailurePermanentError) {
  google::longrunning::Operation operation;
  operation.set_name("test-operation");
  operation.set_done(false);

  StatusOr<google::protobuf::Value> actual =
      PollingLoop<google::protobuf::Value>(
          TestPollingPolicy(),
          [](grpc::ClientContext&,
             google::longrunning::GetOperationRequest const&) {
            return StatusOr<google::longrunning::Operation>(
                Status(StatusCode::kPermissionDenied, "uh oh"));
          },
          operation, "location");
  EXPECT_EQ(StatusCode::kPermissionDenied, actual.status().code());
}

TEST(PollingLoopTest, FailureTooManyTransients) {
  google::longrunning::Operation operation;
  operation.set_name("test-operation");
  operation.set_done(false);

  StatusOr<google::protobuf::Value> actual =
      PollingLoop<google::protobuf::Value>(
          TestPollingPolicy(),
          [](grpc::ClientContext&,
             google::longrunning::GetOperationRequest const&) {
            return StatusOr<google::longrunning::Operation>(
                Status(StatusCode::kUnavailable, "just keep trying"));
          },
          operation, "location");
  EXPECT_EQ(StatusCode::kUnavailable, actual.status().code());
}

TEST(PollingLoopTest, FailureMissingResponseAndError) {
  google::longrunning::Operation operation;
  operation.set_name("test-operation");
  operation.set_done(true);

  StatusOr<google::protobuf::Value> actual =
      PollingLoop<google::protobuf::Value>(
          TestPollingPolicy(),
          [](grpc::ClientContext&,
             google::longrunning::GetOperationRequest const&) {
            return make_status_or(google::longrunning::Operation{});
          },
          operation, "test-location");
  EXPECT_EQ(StatusCode::kInternal, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("test-location"));
}

TEST(PollingLoopTest, FailureInvalidContents) {
  google::protobuf::Empty contents;
  google::longrunning::Operation operation;
  operation.set_name("test-operation");
  operation.set_done(true);
  operation.mutable_response()->PackFrom(contents);

  StatusOr<google::protobuf::Value> actual =
      PollingLoop<google::protobuf::Value>(
          TestPollingPolicy(),
          [](grpc::ClientContext&,
             google::longrunning::GetOperationRequest const&) {
            return make_status_or(google::longrunning::Operation{});
          },
          operation, "test-location");
  EXPECT_EQ(StatusCode::kInternal, actual.status().code());
  EXPECT_THAT(actual.status().message(), HasSubstr("test-location"));
}

}  // namespace
}  // namespace internal
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/grpc_error_delegate.h"
#include <gtest/gtest.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

TEST(MakeStatusFromRpcError, AllCodes) {
  using ::google::cloud::StatusCode;

  struct {
    grpc::StatusCode grpc;
    StatusCode expected;
  } expected_codes[]{
      {grpc::StatusCode::OK, StatusCode::kOk},
      {grpc::StatusCode::CANCELLED, StatusCode::kCancelled},
      {grpc::StatusCode::UNKNOWN, StatusCode::kUnknown},
      {grpc::StatusCode::INVALID_ARGUMENT, StatusCode::kInvalidArgument},
      {grpc::StatusCode::DEADLINE_EXCEEDED, StatusCode::kDeadlineExceeded},
      {grpc::StatusCode::NOT_FOUND, StatusCode::kNotFound},
      {grpc::StatusCode::ALREADY_EXISTS, StatusCode::kAlreadyExists},
      {grpc::StatusCode::PERMISSION_DENIED, StatusCode::kPermissionDenied},
      {grpc::StatusCode::UNAUTHENTICATED, StatusCode::kUnauthenticated},
      {grpc::StatusCode::RESOURCE_EXHAUSTED, StatusCode::kResourceExhausted},
      {grpc::StatusCode::FAILED_PRECONDITION, StatusCode::kFailedPrecondition},
      {grpc::StatusCode::ABORTED, StatusCode::kAborted},
      {grpc::StatusCode::OUT_OF_RANGE, StatusCode::kOutOfRange},
      {grpc::StatusCode::UNIMPLEMENTED, StatusCode::kUnimplemented},
      {grpc::StatusCode::INTERNAL, StatusCode::kInternal},
      {grpc::StatusCode::UNAVAILABLE, StatusCode::kUnavailable},
      {grpc::StatusCode::DATA_LOSS, StatusCode::kDataLoss},
  };

  for (auto const& codes : expected_codes) {
    std::string const message = "test message";
    auto const original = grpc::Status(codes.grpc, message);
    auto const expected = google::cloud::Status(codes.expected, message);
    auto const actual = MakeStatusFromRpcError(original);
    EXPECT_EQ(expected, actual);
  }
}

TEST(MakeStatusFromRpcError, ProtoValidCode) {
  using ::google::cloud::StatusCode;

  struct {
    grpc::StatusCode grpc;
    StatusCode expected;
  } expected_codes[]{
      {grpc::StatusCode::OK, StatusCode::kOk},
      {grpc::StatusCode::CANCELLED, StatusCode::kCancelled},
      {grpc::StatusCode::UNKNOWN, StatusCode::kUnknown},
      {grpc::StatusCode::INVALID_ARGUMENT, StatusCode::kInvalidArgument},
      {grpc::StatusCode::DEADLINE_EXCEEDED, StatusCode::kDeadlineExceeded},
      {grpc::StatusCode::NOT_FOUND, StatusCode::kNotFound},
      {grpc::StatusCode::ALREADY_EXISTS, StatusCode::kAlreadyExists},
      {grpc::StatusCode::PERMISSION_DENIED, StatusCode::kPermissionDenied},
      {grpc::StatusCode::UNAUTHENTICATED, StatusCode::kUnauthenticated},
      {grpc::StatusCode::RESOURCE_EXHAUSTED, StatusCode::kResourceExhausted},
      {grpc::StatusCode::FAILED_PRECONDITION, StatusCode::kFailedPrecondition},
      {grpc::StatusCode::ABORTED, StatusCode::kAborted},
      {grpc::StatusCode::OUT_OF_RANGE, StatusCode::kOutOfRange},
      {grpc::StatusCode::UNIMPLEMENTED, StatusCode::kUnimplemented},
      {grpc::StatusCode::INTERNAL, StatusCode::kInternal},
      {grpc::StatusCode::UNAVAILABLE, StatusCode::kUnavailable},
      {grpc::StatusCode::DATA_LOSS, StatusCode::kDataLoss},
  };

  for (auto const& codes : expected_codes) {
    std::string const message = "test message";
    google::rpc::Status original;
    original.set_message(message);
    original.set_code(codes.grpc);
    auto const expected = google::cloud::Status(codes.expected, message);
    auto const actual = MakeStatusFromRpcError(original);
    EXPECT_EQ(expected, actual);
  }
}

TEST(MakeStatusFromRpcError, ProtoInvalidCode) {
  using ::google::cloud::StatusCode;

  for (auto const& code : {-100, -1, 30, 100}) {
    std::string const message = "test message";
    google::rpc::Status original;
    original.set_message(message);
    original.set_code(code);
    auto const expected = google::cloud::Status(StatusCode::kUnknown, message);
    auto const actual = MakeStatusFromRpcError(original);
    EXPECT_EQ(expected, actual);
  }
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

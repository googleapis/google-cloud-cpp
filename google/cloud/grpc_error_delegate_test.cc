// Copyright 2019 Google LLC
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

#include "google/cloud/grpc_error_delegate.h"
#include "google/cloud/internal/status_payload_keys.h"
#include <google/rpc/error_details.pb.h>
#include <gtest/gtest.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

std::string MakeErrorDetails(google::rpc::Status const& proto) {
  return proto.SerializeAsString();
}

std::string MakeErrorDetails(grpc::StatusCode code, std::string message) {
  google::rpc::Status proto;
  proto.set_code(static_cast<std::int32_t>(code));
  proto.set_message(std::move(message));
  return MakeErrorDetails(proto);
}

std::string MakeErrorDetails(grpc::StatusCode code, std::string message,
                             ErrorInfo const& error_info) {
  google::rpc::Status proto;
  proto.set_code(static_cast<std::int32_t>(code));
  proto.set_message(std::move(message));

  google::rpc::ErrorInfo error_info_proto;
  error_info_proto.set_reason(error_info.reason());
  error_info_proto.set_domain(error_info.domain());
  for (auto const& e : error_info.metadata()) {
    (*error_info_proto.mutable_metadata())[e.first] = e.second;
  }

  proto.add_details()->PackFrom(error_info_proto);
  return MakeErrorDetails(proto);
}

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

TEST(MakeStatusFromRpcError, AllCodesWithPayload) {
  using ::google::cloud::StatusCode;

  struct {
    grpc::StatusCode grpc;
    StatusCode expected;
  } expected_codes[]{
      // We skip OK because that cannot have a payload.
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
    std::string const payload = MakeErrorDetails(codes.grpc, message);
    auto const original = grpc::Status(codes.grpc, message, payload);

    auto expected = google::cloud::Status(codes.expected, message);
    google::cloud::internal::SetPayload(
        expected, google::cloud::internal::StatusPayloadGrpcProto(), payload);

    auto const actual = MakeStatusFromRpcError(original);
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(message, actual.message());
    EXPECT_EQ(ErrorInfo{}, actual.error_info());

    // Make sure the actual payload is what we expect.
    auto actual_payload = google::cloud::internal::GetPayload(
        actual, google::cloud::internal::StatusPayloadGrpcProto());
    EXPECT_TRUE(actual_payload.has_value());
    EXPECT_EQ(payload, actual_payload.value());
  }
}

TEST(MakeStatusFromRpcError, AllCodesWithPayloadAndErrorDetails) {
  using ::google::cloud::StatusCode;

  struct {
    grpc::StatusCode grpc;
    StatusCode expected;
  } expected_codes[]{
      // We skip OK because that cannot have a payload.
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
    ErrorInfo const error_info{"reason", "domain", {{"key", "val"}}};
    std::string const payload =
        MakeErrorDetails(codes.grpc, message, error_info);
    auto const original = grpc::Status(codes.grpc, message, payload);

    auto expected = google::cloud::Status(codes.expected, message, error_info);
    google::cloud::internal::SetPayload(
        expected, google::cloud::internal::StatusPayloadGrpcProto(), payload);

    auto const actual = MakeStatusFromRpcError(original);
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(message, actual.message());
    EXPECT_EQ(error_info, actual.error_info());

    // Make sure the actual payload is what we expect.
    auto actual_payload = google::cloud::internal::GetPayload(
        actual, google::cloud::internal::StatusPayloadGrpcProto());
    EXPECT_TRUE(actual_payload.has_value());
    EXPECT_EQ(payload, actual_payload.value());
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
    auto const actual = MakeStatusFromRpcError(original);
    auto expected = google::cloud::Status(codes.expected, message);
    google::cloud::internal::SetPayload(
        expected, google::cloud::internal::StatusPayloadGrpcProto(),
        MakeErrorDetails(original));

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
    auto const actual = MakeStatusFromRpcError(original);
    auto expected = google::cloud::Status(StatusCode::kUnknown, message);
    google::cloud::internal::SetPayload(
        expected, google::cloud::internal::StatusPayloadGrpcProto(),
        MakeErrorDetails(original));
    EXPECT_EQ(expected, actual);
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

// Copyright 2018 Google LLC
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

#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/expect_exception.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Optional;

TEST(StatusCode, StatusCodeToString) {
  EXPECT_EQ("OK", StatusCodeToString(StatusCode::kOk));
  EXPECT_EQ("CANCELLED", StatusCodeToString(StatusCode::kCancelled));
  EXPECT_EQ("UNKNOWN", StatusCodeToString(StatusCode::kUnknown));
  EXPECT_EQ("INVALID_ARGUMENT",
            StatusCodeToString(StatusCode::kInvalidArgument));
  EXPECT_EQ("DEADLINE_EXCEEDED",
            StatusCodeToString(StatusCode::kDeadlineExceeded));
  EXPECT_EQ("NOT_FOUND", StatusCodeToString(StatusCode::kNotFound));
  EXPECT_EQ("ALREADY_EXISTS", StatusCodeToString(StatusCode::kAlreadyExists));
  EXPECT_EQ("PERMISSION_DENIED",
            StatusCodeToString(StatusCode::kPermissionDenied));
  EXPECT_EQ("UNAUTHENTICATED",
            StatusCodeToString(StatusCode::kUnauthenticated));
  EXPECT_EQ("RESOURCE_EXHAUSTED",
            StatusCodeToString(StatusCode::kResourceExhausted));
  EXPECT_EQ("FAILED_PRECONDITION",
            StatusCodeToString(StatusCode::kFailedPrecondition));
  EXPECT_EQ("ABORTED", StatusCodeToString(StatusCode::kAborted));
  EXPECT_EQ("OUT_OF_RANGE", StatusCodeToString(StatusCode::kOutOfRange));
  EXPECT_EQ("UNIMPLEMENTED", StatusCodeToString(StatusCode::kUnimplemented));
  EXPECT_EQ("INTERNAL", StatusCodeToString(StatusCode::kInternal));
  EXPECT_EQ("UNAVAILABLE", StatusCodeToString(StatusCode::kUnavailable));
  EXPECT_EQ("DATA_LOSS", StatusCodeToString(StatusCode::kDataLoss));
  EXPECT_EQ("UNEXPECTED_STATUS_CODE=42",
            StatusCodeToString(static_cast<StatusCode>(42)));
}

TEST(ErrorInfo, Basics) {
  ErrorInfo e;
  EXPECT_EQ(e, ErrorInfo{});

  std::unordered_map<std::string, std::string> metadata = {{"key", "val"}};
  e = ErrorInfo{"the reason", "the domain", metadata};
  EXPECT_NE(e, ErrorInfo{});
  EXPECT_EQ("the reason", e.reason());
  EXPECT_EQ("the domain", e.domain());
  EXPECT_EQ(metadata, e.metadata());

  auto copy = e;
  EXPECT_EQ(copy, e);
}

TEST(Status, Basics) {
  Status s;
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kOk);
  EXPECT_EQ(s.message(), "");
  EXPECT_EQ(s.error_info(), ErrorInfo{});
  EXPECT_EQ(s, Status());

  ErrorInfo const error_info{"the reason", "the domain", {}};
  // The error properties are ignored on OK statuses.
  auto ok = Status(StatusCode::kOk, "message ignored", error_info);
  EXPECT_EQ(s, ok);
  EXPECT_EQ("", ok.message());
  EXPECT_EQ(s.error_info(), ErrorInfo{});
  EXPECT_EQ(StatusCode::kOk, ok.code());

  s = Status(StatusCode::kUnknown, "foo", error_info);
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kUnknown);
  EXPECT_EQ(s.message(), "foo");
  EXPECT_EQ(s.error_info(), error_info);
  EXPECT_NE(s, Status());
  EXPECT_NE(s, Status(StatusCode::kUnknown, ""));
  EXPECT_NE(s, Status(StatusCode::kUnknown, "bar"));
  EXPECT_NE(s, Status(StatusCode::kUnknown, "foo"));
  EXPECT_EQ(s, Status(StatusCode::kUnknown, "foo", error_info));
}

TEST(Status, SelfAssignWorks) {
  auto s = Status(StatusCode::kUnknown, "foo");
  auto& r = s;  // Hide the self-assign from linter.
  s = r;
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kUnknown);
  EXPECT_EQ(s.message(), "foo");

  s = std::move(r);
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kUnknown);
  EXPECT_EQ(s.message(), "foo");
}

TEST(Status, OperatorOutput) {
  auto status = Status{};
  auto ss = std::stringstream{};
  ss << status;
  EXPECT_EQ("OK", ss.str());

  ss = std::stringstream{};
  status = Status{StatusCode::kUnknown, "foo"};
  ss << status;
  EXPECT_EQ("UNKNOWN: foo", ss.str());
}

TEST(Status, OperatorOutputWithErrorInfo) {
  ErrorInfo error_info{"the reason", "the domain", {{"key", "val"}}};
  auto status = Status{StatusCode::kUnknown, "foo", error_info};
  auto ss = std::stringstream{};
  ss << status;
  EXPECT_EQ(ss.str(),
            "UNKNOWN: foo error_info={reason=the reason, "
            "domain=the domain, metadata={key=val}}");
}

TEST(Status, PayloadIgnoredWithOk) {
  Status const ok{};
  Status s;
  EXPECT_EQ(ok, s);
  internal::SetPayload(s, "key1", "payload1");
  EXPECT_EQ(ok, s);
  auto v = internal::GetPayload(s, "key1");
  EXPECT_FALSE(v.has_value());
}

TEST(Status, Payload) {
  Status const err{StatusCode::kUnknown, "some error"};
  Status s = err;
  EXPECT_EQ(err, s);
  internal::SetPayload(s, "key1", "payload1");
  EXPECT_NE(err, s);
  auto v = internal::GetPayload(s, "key1");
  EXPECT_TRUE(v.has_value());
  EXPECT_EQ(*v, "payload1");

  Status copy = s;
  EXPECT_EQ(copy, s);
  internal::SetPayload(s, "key2", "payload2");
  EXPECT_NE(copy, s);
  v = internal::GetPayload(s, "key2");
  EXPECT_TRUE(v.has_value());
  EXPECT_EQ(*v, "payload2");

  internal::SetPayload(copy, "key2", "payload2");
  EXPECT_EQ(copy, s);
}

TEST(Status, RetryInfoIgnoredWithOk) {
  Status const ok{};
  Status s;
  EXPECT_EQ(ok, s);
  internal::SetRetryInfo(s, internal::RetryInfo{std::chrono::minutes(5)});
  EXPECT_EQ(ok, s);
  auto ri = internal::GetRetryInfo(s);
  EXPECT_EQ(ri, absl::nullopt);
}

TEST(Status, RetryInfo) {
  internal::RetryInfo const expected{std::chrono::minutes(5)};
  Status const err{StatusCode::kUnknown, "some error"};
  Status s = err;
  EXPECT_EQ(err, s);
  internal::SetRetryInfo(s, expected);
  EXPECT_NE(err, s);
  auto actual = internal::GetRetryInfo(s);
  EXPECT_THAT(actual, Optional(expected));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

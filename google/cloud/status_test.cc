// Copyright 2018 Google LLC
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

#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/expect_exception.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include <gmock/gmock.h>
#include <unistd.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

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

TEST(Status, Basics) {
  Status s;
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kOk);
  EXPECT_EQ(s.message(), "");
  EXPECT_EQ(s, Status());

  s = Status(StatusCode::kUnknown, "foo");
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.code(), StatusCode::kUnknown);
  EXPECT_EQ(s.message(), "foo");
  EXPECT_NE(s, Status());
  EXPECT_NE(s, Status(StatusCode::kUnknown, ""));
  EXPECT_NE(s, Status(StatusCode::kUnknown, "bar"));
  EXPECT_EQ(s, Status(StatusCode::kUnknown, "foo"));
}

TEST(Status, ToAbseilStatus) {
  Status s;
  absl::Status a = internal::ToAbslStatus(s);
  EXPECT_EQ(a.code(), absl::StatusCode::kOk);
  EXPECT_EQ(a.message(), "");

  s = Status{StatusCode::kUnknown, "foo"};
  a = internal::ToAbslStatus(s);
  EXPECT_EQ(a.code(), absl::StatusCode::kUnknown);
  EXPECT_EQ(a.message(), "foo");
}

TEST(Status, FromAbseilStatus) {
  absl::Status a;
  Status s = internal::MakeStatus(a);
  EXPECT_EQ(s.code(), StatusCode::kOk);
  EXPECT_EQ(s.message(), "");

  a = absl::Status{absl::StatusCode::kUnknown, "bar"};
  s = internal::MakeStatus(a);
  EXPECT_EQ(s.code(), StatusCode::kUnknown);
  EXPECT_EQ(s.message(), "bar");
}

TEST(Status, RoundTripAbseil) {
  absl::Status a;
  EXPECT_EQ(a, internal::ToAbslStatus(internal::MakeStatus(a)));

  a = absl::Status{absl::StatusCode::kUnknown, "bar"};
  EXPECT_EQ(a, internal::ToAbslStatus(internal::MakeStatus(a)));

  // Round tripping doesn't drop the payload.
  a.SetPayload("key", absl::Cord("the payload"));
  EXPECT_EQ(a, internal::ToAbslStatus(internal::MakeStatus(a)));
  a = internal::ToAbslStatus(internal::MakeStatus(a));
  absl::optional<absl::Cord> c = a.GetPayload("key");
  EXPECT_TRUE(c.has_value());
  EXPECT_EQ(*c, "the payload");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

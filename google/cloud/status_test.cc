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

#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/expect_exception.h"
#include "google/cloud/testing_util/testing_types.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {
using ::testing::HasSubstr;

TEST(Status, StatusCodeToString) {
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
  EXPECT_EQ("UNEXPECTED_STATUS_CODE=-1",
            StatusCodeToString(StatusCode::kDoNotUse));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

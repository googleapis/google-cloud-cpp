// Copyright 2025 Google LLC
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

#include "google/cloud/storage/async/retry_policy.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

TEST(RetryPolicyTest, PermanentFailure) {
  EXPECT_TRUE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kCancelled, "cancelled")));
  EXPECT_TRUE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kUnknown, "unknown")));
  EXPECT_TRUE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kInvalidArgument, "invalid argument")));
  EXPECT_TRUE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kNotFound, "not found")));
  EXPECT_TRUE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kAlreadyExists, "already exists")));
  EXPECT_TRUE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kPermissionDenied, "permission denied")));
  EXPECT_TRUE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kFailedPrecondition, "failed precondition")));
  EXPECT_TRUE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kOutOfRange, "out of range")));
  EXPECT_TRUE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kUnimplemented, "unimplemented")));
  EXPECT_TRUE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kDataLoss, "data loss")));
  EXPECT_TRUE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kUnauthenticated, "unauthenticated")));

  EXPECT_FALSE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kDeadlineExceeded, "deadline exceeded")));
  EXPECT_FALSE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kResourceExhausted, "resource exhausted")));
  EXPECT_FALSE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kAborted, "aborted")));
  EXPECT_FALSE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kInternal, "internal")));
  EXPECT_FALSE(AsyncStatusTraits::IsPermanentFailure(
      Status(StatusCode::kUnavailable, "unavailable")));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

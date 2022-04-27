// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsublite/internal/stream_retry_policy.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsublite_internal {

constexpr unsigned int kNumStatusCodes = 17;

TEST(StreamRetryPolicyTest, Codes) {
  StreamRetryPolicy retry_policy;
  struct Case {
    StatusCode code;
    bool expected;
  } cases[] = {
      {StatusCode::kOk, false},
      {StatusCode::kCancelled, true},
      {StatusCode::kUnknown, true},
      {StatusCode::kInvalidArgument, false},
      {StatusCode::kDeadlineExceeded, true},
      {StatusCode::kNotFound, false},
      {StatusCode::kAlreadyExists, false},
      {StatusCode::kPermissionDenied, false},
      {StatusCode::kUnauthenticated, false},
      {StatusCode::kResourceExhausted, true},
      {StatusCode::kFailedPrecondition, false},
      {StatusCode::kAborted, true},
      {StatusCode::kOutOfRange, false},
      {StatusCode::kUnimplemented, false},
      {StatusCode::kInternal, true},
      {StatusCode::kUnavailable, true},
      {StatusCode::kDataLoss, false},
  };

  for (auto const& c : cases) {
    SCOPED_TRACE("Testing " + StatusCodeToString(c.code));
    EXPECT_EQ(retry_policy.OnFailure(Status(c.code, "")), c.expected);
    EXPECT_EQ(retry_policy.IsPermanentFailure(Status(c.code, "")), !c.expected);
    EXPECT_FALSE(retry_policy.IsExhausted());
  }
}

}  // namespace pubsublite_internal
}  // namespace cloud
}  // namespace google

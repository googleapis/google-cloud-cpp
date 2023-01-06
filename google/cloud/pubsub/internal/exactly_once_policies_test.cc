// Copyright 2022 Google LLC
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

#include "google/cloud/pubsub/internal/exactly_once_policies.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

TEST(ExactlyOnceRetryPolicy, PermanentFailure) {
  auto const uut = ExactlyOnceRetryPolicy("test-only-ack-id");
  Status transient_cases[] = {
      Status(StatusCode::kDeadlineExceeded, "deadline"),
      Status(StatusCode::kAborted, "aborted"),
      Status(StatusCode::kInternal, "ooops"),
      Status(StatusCode::kUnavailable, "try-again"),
      Status(StatusCode::kUnknown, "unknown with match + transient",
             ErrorInfo("test-only-reasons", "test-only-domain",
                       {
                           {"some-other-id", "PERMANENT_"},
                           {"test-only-ack-id", "TRANSIENT_FAILURE_NO_BIGGIE"},
                       })),
  };

  Status permanent_cases[] = {
      Status(StatusCode::kNotFound, "not found"),
      Status(StatusCode::kPermissionDenied, "permission denied"),

      Status(StatusCode::kUnknown, "unknown without match",
             ErrorInfo("test-only-reasons", "test-only-domain",
                       {
                           {"some-other-id", "PERMANENT_"},
                       })),

      Status(StatusCode::kUnknown, "unknown with match + permanent",
             ErrorInfo(
                 "test-only-reasons", "test-only-domain",
                 {
                     {"some-other-id", "PERMANENT_"},
                     {"test-only-ack-id", "PERMANENT_FAILURE_INVALID_ACK_ID"},
                 })),
  };

  for (auto const& status : transient_cases) {
    SCOPED_TRACE("Testing transient error + " + status.message());
    EXPECT_FALSE(uut.IsPermanentFailure(status));
  }

  for (auto const& status : permanent_cases) {
    SCOPED_TRACE("Testing permanent error + " + status.message());
    EXPECT_TRUE(uut.IsPermanentFailure(status));
  }
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

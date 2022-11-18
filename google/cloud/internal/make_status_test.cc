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

#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {

using ::google::cloud::testing_util::StatusIs;

TEST(MakeStatus, Basic) {
  auto error_info = ErrorInfo("REASON", "domain", {{"key", "value"}});

  struct TestCase {
    StatusCode code;
    Status status;
  } cases[] = {
      {StatusCode::kCancelled, CancelledError("test", error_info)},
      {StatusCode::kUnknown, UnknownError("test", error_info)},
      {StatusCode::kInvalidArgument, InvalidArgumentError("test", error_info)},
      {StatusCode::kDeadlineExceeded,
       DeadlineExceededError("test", error_info)},
      {StatusCode::kNotFound, NotFoundError("test", error_info)},
      {StatusCode::kAlreadyExists, AlreadyExistsError("test", error_info)},
      {StatusCode::kPermissionDenied,
       PermissionDeniedError("test", error_info)},
      {StatusCode::kUnauthenticated, UnauthenticatedError("test", error_info)},
      {StatusCode::kResourceExhausted,
       ResourceExhaustedError("test", error_info)},
      {StatusCode::kFailedPrecondition,
       FailedPreconditionError("test", error_info)},
      {StatusCode::kAborted, AbortedError("test", error_info)},
      {StatusCode::kOutOfRange, OutOfRangeError("test", error_info)},
      {StatusCode::kUnimplemented, UnimplementedError("test", error_info)},
      {StatusCode::kInternal, InternalError("test", error_info)},
      {StatusCode::kUnavailable, UnavailableError("test", error_info)},
      {StatusCode::kDataLoss, DataLossError("test", error_info)},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing for " + StatusCodeToString(test.code));
    EXPECT_THAT(test.status, StatusIs(test.code));
    EXPECT_EQ(test.status.message(), "test");
    EXPECT_EQ(test.status.error_info(), error_info);
  }
}

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

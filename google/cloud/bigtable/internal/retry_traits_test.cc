// Copyright 2023 Google LLC
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

#include "google/cloud/bigtable/internal/retry_traits.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

/// @test Verify that certain known internal errors are retryable.
TEST(SafeGrpcRetry, RstStreamRetried) {
  EXPECT_FALSE(SafeGrpcRetry::IsTransientFailure(
      Status(StatusCode::kInternal, "non-retryable")));
  EXPECT_TRUE(SafeGrpcRetry::IsTransientFailure(
      Status(StatusCode::kInternal, "RST_STREAM")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

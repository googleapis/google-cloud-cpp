// Copyright 2026 Google LLC
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

#include "google/cloud/storage/internal/metrics_error_type.h"
#include "google/cloud/internal/make_status.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(MetricsErrorTypeTest, OkAndCanonicalCodes) {
  EXPECT_EQ(ComputeErrorType(Status{}), "OK");
  for (int i = static_cast<int>(StatusCode::kCancelled);
       i <= static_cast<int>(StatusCode::kUnauthenticated); ++i) {
    auto const code = static_cast<StatusCode>(i);
    EXPECT_EQ(ComputeErrorType(Status(code, "server error")),
              StatusCodeToString(code));
  }
}

TEST(MetricsErrorTypeTest, ChecksumMismatch) {
  struct TestCase {
    StatusCode code;
    char const* msg;
    char const* expected;
  } const cases[] = {
      {StatusCode::kInvalidArgument, "mismatched crc32c checksum",
       "CHECKSUM_MISMATCH"},
      {StatusCode::kDataLoss,
       "Permanent error, with a last message of mismatched checksums detected",
       "CHECKSUM_MISMATCH"},
      {StatusCode::kDataLoss, "Close(): mismatched hashes in download",
       "CHECKSUM_MISMATCH"},
      {StatusCode::kInvalidArgument,
       "Provided CRC32C \"abc\" doesn't match calculated CRC32C.",
       "CHECKSUM_MISMATCH"},
      {StatusCode::kDataLoss,
       "Uploaded MD5 hash \"abc\" doesn't match calculated MD5.",
       "CHECKSUM_MISMATCH"},
      {StatusCode::kNotFound, "Object my-crc32c-match-object not found",
       "NOT_FOUND"},
      {StatusCode::kFailedPrecondition, "crc32c metadata did not match",
       "FAILED_PRECONDITION"},
  };
  for (auto const& c : cases) {
    EXPECT_EQ(ComputeErrorType(Status(c.code, c.msg)), c.expected);
  }
}

TEST(MetricsErrorTypeTest, HttpStatusMetadataOverridesCoalescedUnavailable) {
  struct TestCase {
    char const* http_code;
    char const* expected;
  } const cases[] = {
      {"429", "RESOURCE_EXHAUSTED"}, {"408", "DEADLINE_EXCEEDED"},
      {"504", "DEADLINE_EXCEEDED"},  {"500", "INTERNAL"},
      {"502", "UNAVAILABLE"},        {"503", "UNAVAILABLE"},
  };
  for (auto const& c : cases) {
    Status const status(
        StatusCode::kUnavailable, "http error",
        ErrorInfo{"", "googleapis.com", {{"http_status_code", c.http_code}}});
    EXPECT_EQ(ComputeErrorType(status), c.expected);
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

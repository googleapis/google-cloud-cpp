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
#include "google/cloud/status.h"
#include <gtest/gtest.h>

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
              StatusCodeToString(code))
        << "code=" << StatusCodeToString(code);
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
      {StatusCode::kInvalidArgument, "mismatched md5 checksum",
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
       "Provided CRC32C \"abc\" does not match calculated CRC32C.",
       "CHECKSUM_MISMATCH"},
      {StatusCode::kDataLoss,
       "Uploaded MD5 hash \"abc\" doesn't match calculated MD5.",
       "CHECKSUM_MISMATCH"},
      {StatusCode::kInvalidArgument,
       "Object crc32c mismatch: server computed: 123, client provided: 456",
       "CHECKSUM_MISMATCH"},
      {StatusCode::kInvalidArgument,
       "Checksummed data message crc32c mismatch: server computed: 123",
       "CHECKSUM_MISMATCH"},
      {StatusCode::kInvalidArgument,
       "Object MD5 mismatch: server computed: abc, client provided: def",
       "CHECKSUM_MISMATCH"},
      {StatusCode::kNotFound, "mismatched crc32c checksum", "NOT_FOUND"},
      {StatusCode::kNotFound, "Object my-crc32c-match-object not found",
       "NOT_FOUND"},
      {StatusCode::kFailedPrecondition, "crc32c metadata did not match",
       "FAILED_PRECONDITION"},
      {StatusCode::kInvalidArgument, "mismatched offset", "INVALID_ARGUMENT"},
      {StatusCode::kInvalidArgument, "Invalid bucket name: 'md5-match'",
       "INVALID_ARGUMENT"},
      {StatusCode::kInvalidArgument, "length doesn't match calculated size",
       "INVALID_ARGUMENT"},
      {StatusCode::kDataLoss, "invalid crc32c header", "DATA_LOSS"},
  };
  for (auto const& c : cases) {
    EXPECT_EQ(ComputeErrorType(Status(c.code, c.msg)), c.expected)
        << "code=" << StatusCodeToString(c.code) << ", msg=" << c.msg;
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
      {"", "UNAVAILABLE"},
  };
  for (auto const& c : cases) {
    Status const status(
        StatusCode::kUnavailable, "http error",
        ErrorInfo{"", "googleapis.com", {{"http_status_code", c.http_code}}});
    EXPECT_EQ(ComputeErrorType(status), c.expected)
        << "http_code=" << c.http_code;
  }
  Status const non_unavailable(
      StatusCode::kPermissionDenied, "denied",
      ErrorInfo{"", "googleapis.com", {{"http_status_code", "429"}}});
  EXPECT_EQ(ComputeErrorType(non_unavailable), "PERMISSION_DENIED");
}

TEST(MetricsErrorTypeTest, ClientTimeoutsAndCurlTransportErrors) {
  EXPECT_EQ(ComputeErrorType(google::cloud::internal::DeadlineExceededError(
                "Retry policy exhausted before first request attempt",
                GCP_ERROR_INFO())),
            "TIMEOUT");
  EXPECT_EQ(ComputeErrorType(google::cloud::internal::UnavailableError(
                "timeout [30s] while waiting for Write()", GCP_ERROR_INFO())),
            "UNAVAILABLE");
  EXPECT_EQ(ComputeErrorType(
                Status(StatusCode::kDeadlineExceeded,
                       "Deadline expired before operation could complete",
                       ErrorInfo{"", "googleapis.com", {}})),
            "DEADLINE_EXCEEDED");
  EXPECT_EQ(
      ComputeErrorType(Status(
          StatusCode::kUnavailable,
          "PerformWork() - CURL error [6]=Couldn't resolve host name",
          ErrorInfo{"", "googleapis.com", {{"http_status_code", "429"}}})),
      "RESOURCE_EXHAUSTED");

  struct TestCase {
    StatusCode code;
    char const* msg;
    char const* expected;
  } const cases[] = {
      {StatusCode::kDeadlineExceeded,
       "Deadline exceeded waiting for data in ReadObject", "TIMEOUT"},
      {StatusCode::kDeadlineExceeded, "timeout [30s] while waiting for Write()",
       "TIMEOUT"},
      {StatusCode::kDeadlineExceeded,
       "PerformWork() - CURL error [28]=Operation timed out", "TIMEOUT"},
      {StatusCode::kDeadlineExceeded, "Deadline Exceeded", "DEADLINE_EXCEEDED"},
      {StatusCode::kDeadlineExceeded, "server timeout [10s]",
       "DEADLINE_EXCEEDED"},
      {StatusCode::kDeadlineExceeded, "rpc [10s] while waiting for server",
       "DEADLINE_EXCEEDED"},
      {StatusCode::kUnavailable,
       "PerformWork() - CURL error [6]=Couldn't resolve host name",
       "DNS_FAILURE"},
      {StatusCode::kUnavailable,
       "PerformWork() - CURL error [35]=SSL connect error", "TLS_FAILURE"},
      {StatusCode::kUnavailable,
       "PerformWork() - CURL error [98]=Client cert error", "TLS_FAILURE"},
      {StatusCode::kUnknown,
       "PerformWork() - CURL error [60]=Peer certificate cannot be verified",
       "TLS_FAILURE"},
      {StatusCode::kUnavailable,
       "PerformWork() - CURL error [7]=Failed to connect to host",
       "CONNECTION_ERROR"},
      {StatusCode::kUnavailable,
       "PerformWork() - CURL error [56]=Failure when receiving data",
       "CONNECTION_ERROR"},
      {StatusCode::kUnavailable,
       "PerformWork() - CURL error [96]=QUIC connect error",
       "CONNECTION_ERROR"},
      {StatusCode::kUnknown, "PerformWork() - CURL error [6]=Unknown option",
       "UNKNOWN"},
      {StatusCode::kInvalidArgument,
       "PerformWork() - CURL error [6]=Couldn't resolve host name",
       "INVALID_ARGUMENT"},
      {StatusCode::kInvalidArgument,
       "PerformWork() - CURL error [60]=Peer certificate cannot be verified",
       "INVALID_ARGUMENT"},
      {StatusCode::kUnavailable, "PerformWork() - CURL error [2]=Failed init",
       "UNAVAILABLE"},
      {StatusCode::kUnavailable, "PerformWork() - CURL error [abc]=malformed",
       "UNAVAILABLE"},
      {StatusCode::kUnavailable, "PerformWork() - CURL error [6",
       "UNAVAILABLE"},
  };
  for (auto const& c : cases) {
    EXPECT_EQ(ComputeErrorType(Status(c.code, c.msg)), c.expected)
        << "code=" << StatusCodeToString(c.code) << ", msg=" << c.msg;
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

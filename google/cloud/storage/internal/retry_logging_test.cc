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

#include "google/cloud/storage/internal/retry_logging.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "absl/strings/match.h"
#include <gmock/gmock.h>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;

// The lines a `ScopedLog` captured that are retry records.
std::vector<std::string> RetryRecords(testing_util::ScopedLog& log) {
  std::vector<std::string> records;
  for (auto const& line : log.ExtractLines()) {
    if (absl::StrContains(line, "[gcs-retry]")) records.push_back(line);
  }
  return records;
}

TEST(RetryLogging, ResourceFormatting) {
  EXPECT_THAT(RetryLogResource("test-bucket", "test-object"),
              Eq("test-bucket/test-object"));
}

TEST(RetryLogging, ResourceEmptyWhenUnset) {
  EXPECT_THAT(RetryLogResource("", ""), IsEmpty());
}

/// @test A missing name must not leave a dangling separator.
TEST(RetryLogging, ResourceOmitsTheSeparatorWhenOneNameIsEmpty) {
  EXPECT_THAT(RetryLogResource("test-bucket", ""), Eq("test-bucket"));
  EXPECT_THAT(RetryLogResource("", "test-object"), Eq("test-object"));
}

TEST(RetryLogging, LogsOperationStatusAttemptAndResource) {
  testing_util::ScopedLog log;

  LogTransientRetry(
      "ReadObject/resume", "test-bucket/test-object",
      Status(StatusCode::kResourceExhausted, "Rate limit exceeded"),
      /*attempt=*/3);

  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(
                  HasSubstr("[gcs-retry]"), HasSubstr("ReadObject/resume"),
                  HasSubstr("attempt 3"), HasSubstr("test-bucket/test-object"),
                  HasSubstr("Rate limit exceeded"))));
}

TEST(RetryLogging, OmitsResourceWhenEmpty) {
  testing_util::ScopedLog log;

  LogTransientRetry("GetBucketMetadata", /*resource=*/"",
                    Status(StatusCode::kUnavailable, "try again"),
                    /*attempt=*/1);

  EXPECT_THAT(log.ExtractLines(), Contains(AllOf(HasSubstr("[gcs-retry]"),
                                                 HasSubstr("GetBucketMetadata"),
                                                 Not(HasSubstr("resource=")))));
}

/// @test A server message spanning several lines still produces one record.
///
/// GCS error bodies routinely end with a newline. Without flattening, the
/// record splits and `grep '[gcs-retry]'` returns a truncated line; worse, a
/// service could embed a fake record in its error text.
TEST(RetryLogging, StatusIsFlattenedToASingleLine) {
  testing_util::ScopedLog log;

  LogTransientRetry("ReadObject/open", "test-bucket/test-object",
                    Status(StatusCode::kUnavailable, "line one\nline two\n"),
                    /*attempt=*/1);

  EXPECT_THAT(log.ExtractLines(),
              Contains(AllOf(HasSubstr("[gcs-retry]"), HasSubstr("line one"),
                             HasSubstr("line two"), Not(HasSubstr("\n")))));
}

TEST(RetryLogging, UploadResourceRedactsTheUploadId) {
  auto const url = std::string(
      "https://storage.googleapis.com/upload/storage/v1/b/test-bucket/o"
      "?uploadType=resumable&upload_id=0123456789abcdefSECRET");

  std::string const actual = RetryLogUploadResource(url);

  // The bucket stays, so the log still says which upload broke, and a short
  // prefix stays so concurrent uploads can be told apart.
  EXPECT_THAT(actual, HasSubstr("test-bucket"));
  EXPECT_THAT(actual, HasSubstr("upload_id=01234567...[redacted]"));
  EXPECT_THAT(actual, Not(HasSubstr("SECRET")));
  EXPECT_THAT(actual, Not(HasSubstr("0123456789abcdef")));
}

TEST(RetryLogging, UploadResourceKeepsTrailingParameters) {
  auto const url =
      std::string("https://example.com/o?upload_id=0123456789abcdef&alt=json");

  EXPECT_THAT(RetryLogUploadResource(url),
              Eq("https://example.com/o?upload_id=01234567...[redacted]"
                 "&alt=json"));
}

/// @test The gRPC transport carries the session token in a resource name.
///
/// `UploadChunkRequest::upload_session_url()` holds
/// `projects/<project>/buckets/<bucket>/<token>` on gRPC. The token is as
/// sensitive as a REST `upload_id`, so it has to be redacted too.
TEST(RetryLogging, UploadResourceRedactsTheGrpcSessionToken) {
  auto const id =
      std::string("projects/_/buckets/test-bucket/0123456789abcdefSECRET");

  std::string const actual = RetryLogUploadResource(id);

  EXPECT_THAT(actual, Eq("projects/_/buckets/test-bucket/"
                         "01234567...[redacted]"));
  EXPECT_THAT(actual, Not(HasSubstr("SECRET")));
}

/// @test A gRPC session token may itself contain '/'.
TEST(RetryLogging, UploadResourceRedactsAGrpcTokenContainingSlashes) {
  auto const id =
      std::string("projects/my-project/buckets/test-bucket/abcdefgh/ij/SECRET");

  std::string const actual = RetryLogUploadResource(id);

  EXPECT_THAT(actual, Eq("projects/my-project/buckets/test-bucket/"
                         "abcdefgh...[redacted]"));
  EXPECT_THAT(actual, Not(HasSubstr("SECRET")));
}

/// @test A bucket with no trailing token has nothing to hide.
TEST(RetryLogging, UploadResourceKeepsABareBucketResource) {
  auto const id = std::string("projects/_/buckets/test-bucket");

  EXPECT_THAT(RetryLogUploadResource(id), Eq(id));
}

/// @test An opaque token with no recognisable structure is still redacted.
///
/// This is the shape gRPC actually returns -- the emulator hands back a bare
/// 64 character hex string, with none of the `projects/.../buckets/...`
/// framing. An earlier revision returned it verbatim and published the whole
/// bearer capability.
TEST(RetryLogging, UploadResourceRedactsOpaqueTokens) {
  auto const id = std::string(
      "d8bec938b31cdd41225a65f98da14d12ada93fa6ef3f5ccfbc8840d29b828391");

  std::string const actual = RetryLogUploadResource(id);

  EXPECT_THAT(actual, Eq("d8bec938...[redacted]"));
  EXPECT_THAT(actual, Not(HasSubstr("b31cdd41")));
}

/// @test A short unrecognised value cannot leak by being shorter than the
/// retained prefix.
TEST(RetryLogging, UploadResourceRedactsShortOpaqueTokens) {
  EXPECT_THAT(RetryLogUploadResource("abc"), Eq("abc...[redacted]"));
}

TEST(RetryLogging, UploadResourceIsEmptyWhenUnset) {
  EXPECT_THAT(RetryLogUploadResource(""), IsEmpty());
}

TEST(LoggingRetryPolicy, LogsEachRetriedAttempt) {
  testing_util::ScopedLog log;
  storage::LimitedErrorCountRetryPolicy impl(/*maximum_failures=*/3);
  LoggingRetryPolicy tested(impl, "ReadObject/open", "test-bucket/test-object");

  EXPECT_TRUE(tested.OnFailure(Status(StatusCode::kUnavailable, "try again")));
  EXPECT_TRUE(tested.OnFailure(Status(StatusCode::kUnavailable, "try again")));

  std::vector<std::string> const records = RetryRecords(log);
  EXPECT_THAT(records, SizeIs(2));
  EXPECT_THAT(records, Each(HasSubstr("test-bucket/test-object")));
  EXPECT_THAT(records, Contains(HasSubstr("attempt 1")));
  EXPECT_THAT(records, Contains(HasSubstr("attempt 2")));
  EXPECT_THAT(tested.attempt_count(), Eq(2));
}

/// @test A permanent error is not a retry, so it produces no record.
TEST(LoggingRetryPolicy, PermanentErrorIsSilent) {
  testing_util::ScopedLog log;
  storage::LimitedErrorCountRetryPolicy impl(/*maximum_failures=*/3);
  LoggingRetryPolicy tested(impl, "ReadObject/open", "test-bucket/test-object");

  EXPECT_FALSE(
      tested.OnFailure(Status(StatusCode::kNotFound, "no such thing")));

  EXPECT_THAT(RetryRecords(log), IsEmpty());
  EXPECT_THAT(tested.attempt_count(), Eq(0));
}

/// @test The attempt that exhausts the policy is not reported as a retry.
///
/// This is the difference from classifying the status independently: the
/// last failure is handed to the caller, not retried, so announcing a retry
/// would be a lie.
TEST(LoggingRetryPolicy, ExhaustingThePolicyIsSilentOnTheFinalAttempt) {
  testing_util::ScopedLog log;
  storage::LimitedErrorCountRetryPolicy impl(/*maximum_failures=*/2);
  LoggingRetryPolicy tested(impl, "ReadObject/open", "test-bucket/test-object");

  auto const transient = Status(StatusCode::kUnavailable, "try again");
  EXPECT_TRUE(tested.OnFailure(transient));
  EXPECT_TRUE(tested.OnFailure(transient));
  EXPECT_FALSE(tested.OnFailure(transient));

  std::vector<std::string> const records = RetryRecords(log);
  EXPECT_THAT(records, SizeIs(2));
  EXPECT_THAT(records, Not(Contains(HasSubstr("attempt 3"))));
  EXPECT_THAT(tested.attempt_count(), Eq(2));
}

TEST(LoggingRetryPolicy, ForwardsToTheWrappedPolicy) {
  storage::LimitedErrorCountRetryPolicy impl(/*maximum_failures=*/1);
  LoggingRetryPolicy tested(impl, "ReadObject/open", "test-bucket/test-object");

  auto const permanent = Status(StatusCode::kNotFound, "404");
  auto const transient = Status(StatusCode::kUnavailable, "503");

  EXPECT_TRUE(tested.IsPermanentFailure(permanent));
  EXPECT_FALSE(tested.IsPermanentFailure(transient));

  // One failure is tolerated; the second exhausts the policy.
  EXPECT_FALSE(tested.IsExhausted());
  EXPECT_TRUE(tested.OnFailure(transient));
  EXPECT_FALSE(tested.IsExhausted());
  EXPECT_FALSE(tested.OnFailure(transient));
  EXPECT_TRUE(tested.IsExhausted());
  EXPECT_THAT(tested.IsExhausted(), Eq(impl.IsExhausted()));
}

/// @test `kAborted` follows whatever the wrapped policy decides.
///
/// The synchronous policies treat it as permanent, so nothing is logged here.
/// The async paths use a policy that retries it, and because the decision
/// comes from the policy there is no separate classifier to keep in step.
TEST(LoggingRetryPolicy, FollowsTheWrappedPolicyForAborted) {
  testing_util::ScopedLog log;
  storage::LimitedErrorCountRetryPolicy impl(/*maximum_failures=*/3);
  LoggingRetryPolicy tested(impl, "ReadObject/open", "test-bucket/test-object");

  EXPECT_FALSE(tested.OnFailure(Status(StatusCode::kAborted, "aborted")));

  EXPECT_THAT(RetryRecords(log), IsEmpty());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

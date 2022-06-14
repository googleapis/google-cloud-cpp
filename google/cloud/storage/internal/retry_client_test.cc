// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/retry_client.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Property;
using ::testing::Return;

Options BasicTestPolicies() {
  using us = std::chrono::microseconds;
  return Options{}
      .set<Oauth2CredentialsOption>(oauth2::CreateAnonymousCredentials())
      .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(3).clone())
      .set<BackoffPolicyOption>(
          // Make the tests faster.
          ExponentialBackoffPolicy(us(1), us(2), 2).clone())
      .set<IdempotencyPolicyOption>(AlwaysRetryIdempotencyPolicy{}.clone());
}

/// @test Verify that non-idempotent operations return on the first failure.
TEST(RetryClientTest, NonIdempotentErrorHandling) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce(Return(StatusOr<EmptyResponse>(TransientError())));

  // Use a delete operation because this is idempotent only if the it has
  // the IfGenerationMatch() and/or Generation() option set.
  StatusOr<EmptyResponse> result =
      client->DeleteObject(DeleteObjectRequest("test-bucket", "test-object"));
  EXPECT_THAT(result, StatusIs(TransientError().code()));
}

/// @test Verify that the retry loop returns on the first permanent failure.
TEST(RetryClientTest, PermanentErrorHandling) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(BasicTestPolicies());

  // Use a read-only operation because these are always idempotent.
  EXPECT_CALL(*mock, GetObjectMetadata)
      .WillOnce(Return(StatusOr<ObjectMetadata>(TransientError())))
      .WillOnce(Return(StatusOr<ObjectMetadata>(PermanentError())));

  StatusOr<ObjectMetadata> result = client->GetObjectMetadata(
      GetObjectMetadataRequest("test-bucket", "test-object"));
  EXPECT_THAT(result, StatusIs(PermanentError().code()));
}

/// @test Verify that the retry loop returns on the first permanent failure.
TEST(RetryClientTest, TooManyTransientsHandling) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(BasicTestPolicies());

  // Use a read-only operation because these are always idempotent.
  EXPECT_CALL(*mock, GetObjectMetadata)
      .WillRepeatedly(Return(StatusOr<ObjectMetadata>(TransientError())));

  StatusOr<ObjectMetadata> result = client->GetObjectMetadata(
      GetObjectMetadataRequest("test-bucket", "test-object"));
  EXPECT_THAT(result, StatusIs(TransientError().code()));
}

/// @test Verify that the retry loop works with exhausted retry policy.
TEST(RetryClientTest, ExpiredRetryPolicy) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<RetryPolicyOption>(
          LimitedTimeRetryPolicy{std::chrono::milliseconds(0)}.clone()));

  StatusOr<ObjectMetadata> result = client->GetObjectMetadata(
      GetObjectMetadataRequest("test-bucket", "test-object"));
  ASSERT_FALSE(result);
  EXPECT_THAT(
      result,
      StatusIs(StatusCode::kDeadlineExceeded,
               HasSubstr("Retry policy exhausted before first attempt")));
}

/// @test Verify that `CreateResumableUpload()` handles transients.
TEST(RetryClientTest, CreateResumableUploadHandlesTransient) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          AlwaysRetryIdempotencyPolicy().clone()));

  EXPECT_CALL(*mock, CreateResumableUpload)
      .WillOnce(
          Return(StatusOr<CreateResumableUploadResponse>(TransientError())))
      .WillOnce(
          Return(StatusOr<CreateResumableUploadResponse>(TransientError())))
      .WillOnce(Return(make_status_or(
          CreateResumableUploadResponse{"test-only-upload-id"})));

  auto response = client->CreateResumableUpload(
      ResumableUploadRequest("test-bucket", "test-object"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->upload_id, "test-only-upload-id");
}

/// @test Verify that `QueryResumableUpload()` handles transients.
TEST(RetryClientTest, QueryResumableUploadHandlesTransient) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(
          Return(StatusOr<QueryResumableUploadResponse>(TransientError())))
      .WillOnce(
          Return(StatusOr<QueryResumableUploadResponse>(TransientError())))
      .WillOnce(Return(make_status_or(QueryResumableUploadResponse{
          /*.committed_size=*/1234, /*.payload=*/absl::nullopt})));

  auto response = client->QueryResumableUpload(
      QueryResumableUploadRequest("test-only-upload-id"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->committed_size.value_or(0), 1234);
  EXPECT_FALSE(response->payload.has_value());
}

/// @test Verify that transient failures are handled as expected.
TEST(RetryClientTest, UploadChunkHandleTransient) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  // Verify that a transient on a UploadChunk() results in calls to
  // QueryResumableUpload() and that transients in these calls are retried too.
  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(TransientError()))
      .WillOnce(Return(QueryResumableUploadResponse{0, absl::nullopt}));

  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(Return(QueryResumableUploadResponse{quantum, absl::nullopt}));

  // A simpler scenario where only the UploadChunk() calls fail.
  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(QueryResumableUploadResponse{quantum, absl::nullopt}));
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(
          Return(QueryResumableUploadResponse{2 * quantum, absl::nullopt}));

  // Repeat the failure with kAborted.  This error code is only retryable for
  // resumable uploads.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(Return(
          Status(StatusCode::kAborted, "Concurrent requests received.")));
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(
          Return(QueryResumableUploadResponse{2 * quantum, absl::nullopt}));
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(
          Return(QueryResumableUploadResponse{3 * quantum, absl::nullopt}));

  // Even simpler scenario where only the UploadChunk() calls succeeds.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(
          Return(QueryResumableUploadResponse{4 * quantum, absl::nullopt}));

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-session-id", 0, {{payload}}));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(quantum, response->committed_size.value_or(0));

  response = client->UploadChunk(
      UploadChunkRequest("test-only-session-id", quantum, {{payload}}));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(2 * quantum, response->committed_size.value_or(0));

  response = client->UploadChunk(
      UploadChunkRequest("test-only-session-id", 2 * quantum, {{payload}}));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(3 * quantum, response->committed_size.value_or(0));

  response = client->UploadChunk(
      UploadChunkRequest("test-only-session-id", 3 * quantum, {{payload}}));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(4 * quantum, response->committed_size.value_or(0));
}

/// @test Verify that we can restore a session and continue writing.
TEST(RetryClientTest, UploadChunkRestoreSession) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const p0(quantum, '0');
  std::string const p1(quantum, '1');

  auto const restored_committed_size = std::uint64_t{4 * quantum};
  auto committed_size = restored_committed_size;

  ::testing::InSequence sequence;

  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const&) {
    committed_size += quantum;
    return QueryResumableUploadResponse{committed_size, absl::nullopt};
  });
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const&) {
    committed_size += quantum;
    return QueryResumableUploadResponse{committed_size, absl::nullopt};
  });

  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", restored_committed_size, {{p0}}));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(restored_committed_size + quantum,
            response->committed_size.value_or(0));

  response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", restored_committed_size + quantum, {{p1}}));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(restored_committed_size + 2 * quantum,
            response->committed_size.value_or(0));
}

/// @test Verify that transient failures with partial writes are handled
TEST(RetryClientTest, UploadChunkHandleTransientPartialFailures) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const payload = std::string(quantum, 'X') + std::string(quantum, 'Y') +
                       std::string(quantum, 'Z');

  ::testing::InSequence sequence;
  // An initial call to UploadChunk() fails with a retryable error.
  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(TransientError()));
  // When calling QueryResumableUpload() we discover that they have been
  // partially successful.
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(QueryResumableUploadResponse{quantum, absl::nullopt}));
  // We expect that the next call skips these initial bytes, and simulate
  // another transient failure
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    EXPECT_EQ(r.offset(), quantum);
    EXPECT_THAT(r.payload(), ElementsAre(payload.substr(quantum)));
    return TransientError();
  });
  // We expect another call to QueryResumableUpload(), and simulate another
  // partial failure.
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(
          Return(QueryResumableUploadResponse{2 * quantum, absl::nullopt}));
  // This should trigger another UploadChunk() request with the remaining data.
  EXPECT_CALL(*mock, UploadChunk).WillOnce([&](UploadChunkRequest const& r) {
    EXPECT_EQ(r.offset(), 2 * quantum);
    EXPECT_THAT(r.payload(), ElementsAre(payload.substr(2 * quantum)));
    return QueryResumableUploadResponse{3 * quantum, absl::nullopt};
  });

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", 0, {{payload}}));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(3 * quantum, response->committed_size.value_or(0));
}

/// @test Verify that a permanent error on UploadChunk results in a failure.
TEST(RetryClientTest, UploadChunkPermanentError) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(PermanentError()));

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", 0, {{payload}}));
  EXPECT_THAT(response, StatusIs(PermanentError().code(),
                                 HasSubstr(PermanentError().message())));
}

/// @test Verify that a permanent error on QueryResumableUpload results in a
/// failure.
TEST(RetryClientTest, UploadChunkPermanentErrorOnQuery) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');
  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock, QueryResumableUpload).WillOnce(Return(PermanentError()));

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", 0, {{payload}}));
  EXPECT_THAT(response, StatusIs(PermanentError().code(),
                                 HasSubstr(PermanentError().message())));
}

/// @test Verify that unexpected results return an error.
TEST(RetryClientTest, UploadChunkHandleRollback) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  // Simulate a response where the service rolls back the previous value of
  // `committed_size`
  auto const hwm = 4 * quantum;
  auto const rollback = 3 * quantum;
  EXPECT_LT(rollback, hwm);
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(Return(QueryResumableUploadResponse{hwm, absl::nullopt}))
      .WillOnce(Return(QueryResumableUploadResponse{rollback, absl::nullopt}));

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", rollback, {{payload}}));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->committed_size.value_or(0), hwm);

  response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", hwm, {{payload}}));
  EXPECT_THAT(
      response,
      StatusIs(
          StatusCode::kInternal,
          HasSubstr("This is most likely a bug in the GCS client library")));
}

/// @test Verify that unexpected results return an error.
TEST(RetryClientTest, UploadChunkHandleOvercommit) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  // Simulate a response where the service rolls back the previous value of
  // `committed_size`
  auto const excess = 4 * quantum;
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(Return(QueryResumableUploadResponse{excess, absl::nullopt}));

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", 0, {{payload}}));
  EXPECT_THAT(
      response,
      StatusIs(StatusCode::kInternal,
               HasSubstr("This could be caused by multiple applications trying "
                         "to use the same resumable upload")));
}

/// @test Verify that retry exhaustion following a short write fails.
TEST(RetryClientTest, UploadChunkExhausted) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(std::string(quantum * 2, 'X'));

  EXPECT_CALL(*mock, UploadChunk)
      .Times(4)
      .WillRepeatedly(Return(TransientError()));
  EXPECT_CALL(*mock, QueryResumableUpload)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(QueryResumableUploadResponse{0, absl::nullopt}));

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", 0, {{payload}}));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST(RetryClientTest, UploadChunkUploadChunkPolicyExhaustedOnStart) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies()
          .set<RetryPolicyOption>(
              LimitedTimeRetryPolicy(std::chrono::seconds(0)).clone())
          .set<IdempotencyPolicyOption>(StrictIdempotencyPolicy().clone()));

  auto const payload = std::string(UploadChunkRequest::kChunkSizeQuantum, 'X');
  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", 0, {{payload}}));
  EXPECT_THAT(
      response,
      StatusIs(StatusCode::kDeadlineExceeded,
               HasSubstr("Retry policy exhausted before first attempt")));
}

/// @test Verify that responses without a range header are handled.
TEST(RetryClientTest, UploadChunkMissingRangeHeaderInUpload) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const payload = std::string(quantum, '0');

  ::testing::InSequence sequence;
  // Simulate an upload that "succeeds", but returns a missing value for the
  // committed size.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(
          Return(QueryResumableUploadResponse{/*committed_size=*/absl::nullopt,
                                              /*.payload=*/absl::nullopt}));
  // This should trigger a QueryResumableUpload(), simulate a good response.
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(QueryResumableUploadResponse{quantum, absl::nullopt}));

  // The test will create a second request that finalizes the upload. Respond
  // without a committed size also, this should be interpreted as success and
  // not require an additional query.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(Return(QueryResumableUploadResponse{
          /*.committed_size=*/absl::nullopt, /*.payload=*/ObjectMetadata()}));

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", 0, {{payload}}));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(quantum, response->committed_size.value_or(0));

  response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", quantum, {{payload}}, HashValues{}));
  ASSERT_STATUS_OK(response);
  EXPECT_FALSE(response->committed_size.has_value());
  EXPECT_TRUE(response->payload.has_value());
}

/// @test Verify that responses without a range header are handled.
TEST(RetryClientTest, UploadChunkMissingRangeHeaderInQueryResumableUpload) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const payload = std::string(quantum, '0');

  ::testing::InSequence sequence;
  // Assume the first upload works, but it is missing any information about
  // what bytes got uploaded.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(
          Return(QueryResumableUploadResponse{/*committed_size=*/absl::nullopt,
                                              /*.payload=*/absl::nullopt}));
  // This should trigger a `QueryResumableUpload()`, which should also have its
  // Range header missing indicating no bytes were uploaded.
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(
          Return(QueryResumableUploadResponse{absl::nullopt, absl::nullopt}));

  // This should trigger a second upload, which we will let succeed.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(
          Return(QueryResumableUploadResponse{/*committed_size=*/quantum,
                                              /*.payload=*/absl::nullopt}));

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", 0, {{payload}}));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(quantum, response->committed_size.value_or(0));
}

/// @test Verify that full but unfinalized uploads are handled correctly.
TEST(RetryClientTest, UploadFinalChunkQueryMissingPayloadTriggersRetry) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const payload = std::string(quantum, '0');

  ::testing::InSequence sequence;
  // Simulate an upload chunk that has some kind of transient error.
  EXPECT_CALL(*mock,
              UploadChunk(Property(&UploadChunkRequest::last_chunk, true)))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")));
  // This should trigger a `QueryResumableUpload()`, simulate the case where
  // all the data is reported as "committed", but the payload is not reported
  // back.
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(QueryResumableUploadResponse{quantum, absl::nullopt}));
  // This should force a new UploadChunk() to finalize the object, verify this
  // is an "empty" message, and return a successful result.
  EXPECT_CALL(*mock,
              UploadChunk(Property(&UploadChunkRequest::payload_size, 0)))
      .WillOnce(
          Return(QueryResumableUploadResponse{quantum, ObjectMetadata()}));

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", 0, {{payload}}, HashValues{}));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(quantum, response->committed_size.value_or(0));
  EXPECT_TRUE(response->payload.has_value());
}

/// @test Verify that not returning a final payload eventually becomes an error.
TEST(RetryClientTest, UploadFinalChunkQueryTooManyMissingPayloads) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const payload = std::string(quantum, '0');

  // Simulate an upload chunk that has some kind of transient error.
  EXPECT_CALL(*mock, UploadChunk)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(Status(StatusCode::kUnavailable, "try-again")));
  // This should trigger a `QueryResumableUpload()`, simulate the case where the
  // service never returns a payload.
  EXPECT_CALL(*mock, QueryResumableUpload)
      .Times(AtLeast(2))
      .WillRepeatedly(
          Return(QueryResumableUploadResponse{quantum, absl::nullopt}));

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", 0, {{payload}}, HashValues{}));
  ASSERT_THAT(response, Not(IsOk()));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

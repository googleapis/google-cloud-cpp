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

#include "google/cloud/storage/internal/connection_impl.h"
#include "google/cloud/storage/internal/tracing_connection.h"
#include "google/cloud/storage/options.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_generic_stub.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <utility>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::storage::testing::MockGenericStub;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Property;
using ::testing::Return;

Options BasicTestPolicies() {
  using ms = std::chrono::milliseconds;
  return Options{}
      .set<RetryPolicyOption>(LimitedErrorCountRetryPolicy(3).clone())
      .set<BackoffPolicyOption>(
          // Make the tests faster.
          ExponentialBackoffPolicy(ms(1), ms(2), 2).clone())
      .set<IdempotencyPolicyOption>(AlwaysRetryIdempotencyPolicy{}.clone());
}

/// @test Verify that non-idempotent operations return on the first failure.
TEST(RetryClientTest, NonIdempotentErrorHandling) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce(Return(StatusOr<EmptyResponse>(TransientError())));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  // Use a delete operation because this is idempotent only if it has
  // the IfGenerationMatch() and/or Generation() option set.
  StatusOr<EmptyResponse> result =
      client->DeleteObject(DeleteObjectRequest("test-bucket", "test-object"));
  EXPECT_THAT(result, StatusIs(TransientError().code()));
}

/// @test Verify that the retry loop returns on the first permanent failure.
TEST(RetryClientTest, PermanentErrorHandling) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  // Use a read-only operation because these are always idempotent.
  EXPECT_CALL(*mock, GetObjectMetadata)
      .WillOnce(Return(StatusOr<ObjectMetadata>(TransientError())))
      .WillOnce(Return(StatusOr<ObjectMetadata>(PermanentError())));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(BasicTestPolicies());

  StatusOr<ObjectMetadata> result = client->GetObjectMetadata(
      GetObjectMetadataRequest("test-bucket", "test-object"));
  EXPECT_THAT(result, StatusIs(PermanentError().code()));
}

/// @test Verify that the retry loop returns on the first permanent failure.
TEST(RetryClientTest, TooManyTransientsHandling) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  // Use a read-only operation because these are always idempotent.
  EXPECT_CALL(*mock, GetObjectMetadata)
      .WillRepeatedly(Return(StatusOr<ObjectMetadata>(TransientError())));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(BasicTestPolicies());

  StatusOr<ObjectMetadata> result = client->GetObjectMetadata(
      GetObjectMetadataRequest("test-bucket", "test-object"));
  EXPECT_THAT(result, StatusIs(TransientError().code()));
}

/// @test Verify that the retry loop works with exhausted retry policy.
TEST(RetryClientTest, ExpiredRetryPolicy) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<RetryPolicyOption>(
          LimitedTimeRetryPolicy{std::chrono::milliseconds(0)}.clone()));

  StatusOr<ObjectMetadata> result = client->GetObjectMetadata(
      GetObjectMetadataRequest("test-bucket", "test-object"));
  ASSERT_FALSE(result);
  EXPECT_THAT(result, StatusIs(StatusCode::kDeadlineExceeded,
                               HasSubstr("Retry policy exhausted before")));
}

/// @test Verify that `CreateResumableUpload()` handles transients.
TEST(RetryClientTest, CreateResumableUploadHandlesTransient) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, CreateResumableUpload)
      .WillOnce(
          Return(StatusOr<CreateResumableUploadResponse>(TransientError())))
      .WillOnce(
          Return(StatusOr<CreateResumableUploadResponse>(TransientError())))
      .WillOnce(Return(make_status_or(
          CreateResumableUploadResponse{"test-only-upload-id"})));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          AlwaysRetryIdempotencyPolicy().clone()));

  auto response = client->CreateResumableUpload(
      ResumableUploadRequest("test-bucket", "test-object"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->upload_id, "test-only-upload-id");
}

/// @test Verify that `QueryResumableUpload()` handles transients.
TEST(RetryClientTest, QueryResumableUploadHandlesTransient) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(
          Return(StatusOr<QueryResumableUploadResponse>(TransientError())))
      .WillOnce(
          Return(StatusOr<QueryResumableUploadResponse>(TransientError())))
      .WillOnce(Return(make_status_or(QueryResumableUploadResponse{
          /*.committed_size=*/1234, /*.payload=*/absl::nullopt})));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto response = client->QueryResumableUpload(
      QueryResumableUploadRequest("test-only-upload-id"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->committed_size.value_or(0), 1234);
  EXPECT_FALSE(response->payload.has_value());
}

/// @test Verify that transient failures are handled as expected.
TEST(RetryClientTest, UploadChunkHandleTransient) {
  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;

  auto mock = std::make_unique<MockGenericStub>();
  // Verify that a transient on a UploadChunk() results in calls to
  // QueryResumableUpload() and that transients in these calls are retried too.
  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
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

  // Even simpler scenario where only the UploadChunk() calls succeeds.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(
          Return(QueryResumableUploadResponse{3 * quantum, absl::nullopt}));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  std::string const payload(quantum, '0');

  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-session-id", 0, {{payload}}, CreateNullHashFunction()));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(quantum, response->committed_size.value_or(0));

  response = client->UploadChunk(UploadChunkRequest(
      "test-only-session-id", quantum, {{payload}}, CreateNullHashFunction()));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(2 * quantum, response->committed_size.value_or(0));

  response = client->UploadChunk(UploadChunkRequest("test-only-session-id",
                                                    2 * quantum, {{payload}},
                                                    CreateNullHashFunction()));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(3 * quantum, response->committed_size.value_or(0));
}

// TODO(#9293) - fix this test to use ErrorInfo
Status TransientAbortError() {
  return Status(StatusCode::kAborted, "Concurrent requests received.");
}

/// @test Verify that transient failures are handled as expected.
TEST(RetryClientTest, UploadChunkAbortedMaybeIsTransient) {
  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  // Verify that the workaround for "transients" (as defined in #9563) results
  // in calls to QueryResumableUpload().
  EXPECT_CALL(*mock, UploadChunk)
      .Times(4)
      .WillRepeatedly(Return(TransientAbortError()));
  EXPECT_CALL(*mock, QueryResumableUpload)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(QueryResumableUploadResponse{0, absl::nullopt}));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-session-id", 0, {{payload}}, CreateNullHashFunction()));
  EXPECT_THAT(response, StatusIs(StatusCode::kAborted,
                                 HasSubstr("Concurrent requests received.")));
}

/// @test Verify that we can restore a session and continue writing.
TEST(RetryClientTest, UploadChunkRestoreSession) {
  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const restored_committed_size = std::uint64_t{4 * quantum};
  auto committed_size = restored_committed_size;

  auto mock = std::make_unique<MockGenericStub>();
  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](auto&, auto const&, UploadChunkRequest const&) {
        committed_size += quantum;
        return QueryResumableUploadResponse{committed_size, absl::nullopt};
      });
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](auto&, auto const&, UploadChunkRequest const&) {
        committed_size += quantum;
        return QueryResumableUploadResponse{committed_size, absl::nullopt};
      });

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  std::string const p0(quantum, '0');
  std::string const p1(quantum, '1');

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", restored_committed_size, {{p0}},
                         CreateNullHashFunction()));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(restored_committed_size + quantum,
            response->committed_size.value_or(0));

  response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", restored_committed_size + quantum, {{p1}},
      CreateNullHashFunction()));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(restored_committed_size + 2 * quantum,
            response->committed_size.value_or(0));
}

/// @test Verify that transient failures with partial writes are handled
TEST(RetryClientTest, UploadChunkHandleTransientPartialFailures) {
  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const payload = std::string(quantum, 'X') + std::string(quantum, 'Y') +
                       std::string(quantum, 'Z');

  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  ::testing::InSequence sequence;
  // An initial call to UploadChunk() fails with a retryable error.
  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(TransientError()));
  // When calling QueryResumableUpload() we discover that they have been
  // partially successful.
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(QueryResumableUploadResponse{quantum, absl::nullopt}));
  // We expect that the next call skips these initial bytes, and simulate
  // another transient failure
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](auto&, auto const&, UploadChunkRequest const& r) {
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
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](auto&, auto const&, UploadChunkRequest const& r) {
        EXPECT_EQ(r.offset(), 2 * quantum);
        EXPECT_THAT(r.payload(), ElementsAre(payload.substr(2 * quantum)));
        return QueryResumableUploadResponse{3 * quantum, absl::nullopt};
      });

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", 0, {{payload}}, CreateNullHashFunction()));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(3 * quantum, response->committed_size.value_or(0));
}

/// @test Verify that a permanent error on UploadChunk results in a failure.
TEST(RetryClientTest, UploadChunkPermanentError) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(PermanentError()));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", 0, {{payload}}, CreateNullHashFunction()));
  EXPECT_THAT(response, StatusIs(PermanentError().code(),
                                 HasSubstr(PermanentError().message())));
}

/// @test Verify that a permanent error on QueryResumableUpload results in a
/// failure.
TEST(RetryClientTest, UploadChunkPermanentErrorOnQuery) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock, QueryResumableUpload).WillOnce(Return(PermanentError()));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", 0, {{payload}}, CreateNullHashFunction()));
  EXPECT_THAT(response, StatusIs(PermanentError().code(),
                                 HasSubstr(PermanentError().message())));
}

/// @test Verify that unexpected results return an error.
TEST(RetryClientTest, UploadChunkHandleRollback) {
  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  // Simulate a response where the service rolls back the previous value of
  // `committed_size`
  auto const hwm = 4 * quantum;
  auto const rollback = 3 * quantum;
  EXPECT_LT(rollback, hwm);
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(Return(QueryResumableUploadResponse{hwm, absl::nullopt}))
      .WillOnce(Return(QueryResumableUploadResponse{rollback, absl::nullopt}));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", rollback, {{payload}}, CreateNullHashFunction()));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->committed_size.value_or(0), hwm);

  response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", hwm, {{payload}}, CreateNullHashFunction()));
  EXPECT_THAT(
      response,
      StatusIs(
          StatusCode::kInternal,
          HasSubstr("This is most likely a bug in the GCS client library")));
}

/// @test Verify that unexpected results return an error.
TEST(RetryClientTest, UploadChunkHandleOvercommit) {
  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(quantum, '0');

  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  // Simulate a response where the service rolls back the previous value of
  // `committed_size`
  auto const excess = 4 * quantum;
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(Return(QueryResumableUploadResponse{excess, absl::nullopt}));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", 0, {{payload}}, CreateNullHashFunction()));
  EXPECT_THAT(
      response,
      StatusIs(
          StatusCode::kInternal,
          HasSubstr("If you believe this is a bug in the client library")));
}

/// @test Verify that retry exhaustion following a short write fails.
TEST(RetryClientTest, UploadChunkExhausted) {
  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(std::string(quantum * 2, 'X'));

  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, UploadChunk)
      .Times(4)
      .WillRepeatedly(Return(TransientError()));
  EXPECT_CALL(*mock, QueryResumableUpload)
      .Times(AtLeast(2))
      .WillRepeatedly(Return(QueryResumableUploadResponse{0, absl::nullopt}));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", 0, {{payload}}, CreateNullHashFunction()));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST(RetryClientTest, UploadChunkUploadChunkPolicyExhaustedOnStart) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies()
          .set<RetryPolicyOption>(
              LimitedTimeRetryPolicy(std::chrono::seconds(0)).clone())
          .set<IdempotencyPolicyOption>(StrictIdempotencyPolicy().clone()));

  auto const payload = std::string(UploadChunkRequest::kChunkSizeQuantum, 'X');
  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", 0, {{payload}}, CreateNullHashFunction()));
  EXPECT_THAT(response, StatusIs(StatusCode::kDeadlineExceeded,
                                 HasSubstr("Retry policy exhausted before")));
}

/// @test Verify that responses without a range header are handled.
TEST(RetryClientTest, UploadChunkMissingRangeHeaderInUpload) {
  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const payload = std::string(quantum, '0');

  auto mock = std::make_unique<MockGenericStub>();
  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  // Simulate an upload that "succeeds", but returns a missing value for the
  // committed size.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(
          Return(QueryResumableUploadResponse{/*.committed_size=*/absl::nullopt,
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

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", 0, {{payload}}, CreateNullHashFunction()));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(quantum, response->committed_size.value_or(0));

  response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", quantum, {{payload}},
                         CreateNullHashFunction(), HashValues{}));
  ASSERT_STATUS_OK(response);
  EXPECT_FALSE(response->committed_size.has_value());
  EXPECT_TRUE(response->payload.has_value());
}

/// @test Verify that responses without a range header are handled.
TEST(RetryClientTest, UploadChunkMissingRangeHeaderInQueryResumableUpload) {
  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const payload = std::string(quantum, '0');

  auto mock = std::make_unique<MockGenericStub>();
  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  // Assume the first upload works, but it is missing any information about
  // what bytes got uploaded.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(
          Return(QueryResumableUploadResponse{/*.committed_size=*/absl::nullopt,
                                              /*.payload=*/absl::nullopt}));
  // This should trigger a `QueryResumableUpload()`, which should also have its
  // Range header missing indicating no bytes were uploaded.
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(
          Return(QueryResumableUploadResponse{absl::nullopt, absl::nullopt}));

  // This should trigger a second upload, which we will let succeed.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(
          Return(QueryResumableUploadResponse{/*.committed_size=*/quantum,
                                              /*.payload=*/absl::nullopt}));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", 0, {{payload}}, CreateNullHashFunction()));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(quantum, response->committed_size.value_or(0));
}

/// @test Verify that full but unfinalized uploads are handled correctly.
TEST(RetryClientTest, UploadFinalChunkQueryMissingPayloadTriggersRetry) {
  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const payload = std::string(quantum, '0');

  auto mock = std::make_unique<MockGenericStub>();
  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  // Simulate an upload chunk that has some kind of transient error.
  EXPECT_CALL(
      *mock, UploadChunk(_, _, Property(&UploadChunkRequest::last_chunk, true)))
      .WillOnce(Return(Status(StatusCode::kUnavailable, "try-again")));
  // This should trigger a `QueryResumableUpload()`, simulate the case where
  // all the data is reported as "committed", but the payload is not reported
  // back.
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(QueryResumableUploadResponse{quantum, absl::nullopt}));
  // This should force a new UploadChunk() to finalize the object, verify this
  // is an "empty" message, and return a successful result.
  EXPECT_CALL(*mock,
              UploadChunk(_, _, Property(&UploadChunkRequest::payload_size, 0)))
      .WillOnce(
          Return(QueryResumableUploadResponse{quantum, ObjectMetadata()}));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", 0, {{payload}},
                         CreateNullHashFunction(), HashValues{}));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(quantum, response->committed_size.value_or(0));
  EXPECT_TRUE(response->payload.has_value());
}

/// @test Verify that not returning a final payload eventually becomes an error.
TEST(RetryClientTest, UploadFinalChunkQueryTooManyMissingPayloads) {
  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  auto const payload = std::string(quantum, '0');

  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
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

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", 0, {{payload}},
                         CreateNullHashFunction(), HashValues{}));
  ASSERT_THAT(response, Not(IsOk()));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::EnableTracing;
using ::google::cloud::testing_util::InstallSpanCatcher;
using ::google::cloud::testing_util::SpanNamed;
using ::testing::ElementsAre;
using ::testing::Return;

TEST(RetryClientTest, BackoffSpansSimple) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, GetObjectMetadata)
      .WillRepeatedly(Return(TransientError()));

  auto client = storage_internal::MakeTracingClient(
      StorageConnectionImpl::Create(std::move(mock)));
  google::cloud::internal::OptionsSpan const span(
      EnableTracing(BasicTestPolicies()));
  auto response = client->GetObjectMetadata(GetObjectMetadataRequest());
  EXPECT_THAT(response, StatusIs(TransientError().code()));
  EXPECT_THAT(span_catcher->GetSpans(),
              ElementsAre(SpanNamed("Backoff"), SpanNamed("Backoff"),
                          SpanNamed("Backoff"),
                          SpanNamed("storage::Client::GetObjectMetadata")));
}

TEST(RetryClientTest, BackoffSpansUploadChunk) {
  auto span_catcher = InstallSpanCatcher();
  auto mock = std::make_unique<MockGenericStub>();
  ::testing::InSequence sequence;
  EXPECT_CALL(*mock, options);  // Required in RetryClient::Create()
  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(QueryResumableUploadResponse{0, absl::nullopt}));
  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(QueryResumableUploadResponse{0, absl::nullopt}));
  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(QueryResumableUploadResponse{0, absl::nullopt}));
  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(TransientError()));

  auto client = storage_internal::MakeTracingClient(
      StorageConnectionImpl::Create(std::move(mock)));
  google::cloud::internal::OptionsSpan const span(
      EnableTracing(BasicTestPolicies()));

  auto const quantum = UploadChunkRequest::kChunkSizeQuantum;
  std::string const payload(std::string(quantum * 2, 'X'));
  auto response = client->UploadChunk(UploadChunkRequest(
      "test-only-upload-id", 0, {{payload}}, CreateNullHashFunction()));
  EXPECT_THAT(response, StatusIs(TransientError().code()));
  EXPECT_THAT(
      span_catcher->GetSpans(),
      ElementsAre(SpanNamed("Backoff"), SpanNamed("Backoff"),
                  SpanNamed("Backoff"),
                  SpanNamed("storage::Client::WriteObject/UploadChunk")));
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

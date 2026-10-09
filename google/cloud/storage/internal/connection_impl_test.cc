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
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/testing/mock_generic_stub.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/mock_opentelemetry_metrics.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <opentelemetry/metrics/provider.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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
using ::google::cloud::testing_util::MockCounter;
using ::google::cloud::testing_util::MockMeter;
using ::google::cloud::testing_util::MockMeterProvider;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::Eq;
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
          /*.committed_size=*/1234, /*.payload=*/std::nullopt})));

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
      .WillOnce(Return(QueryResumableUploadResponse{0, std::nullopt}));

  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(Return(QueryResumableUploadResponse{quantum, std::nullopt}));

  // A simpler scenario where only the UploadChunk() calls fail.
  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(QueryResumableUploadResponse{quantum, std::nullopt}));
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(
          Return(QueryResumableUploadResponse{2 * quantum, std::nullopt}));

  // Even simpler scenario where only the UploadChunk() calls succeeds.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(
          Return(QueryResumableUploadResponse{3 * quantum, std::nullopt}));

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
      .WillRepeatedly(Return(QueryResumableUploadResponse{0, std::nullopt}));

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
        return QueryResumableUploadResponse{committed_size, std::nullopt};
      });
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](auto&, auto const&, UploadChunkRequest const&) {
        committed_size += quantum;
        return QueryResumableUploadResponse{committed_size, std::nullopt};
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
      .WillOnce(Return(QueryResumableUploadResponse{quantum, std::nullopt}));
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
          Return(QueryResumableUploadResponse{2 * quantum, std::nullopt}));
  // This should trigger another UploadChunk() request with the remaining data.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce([&](auto&, auto const&, UploadChunkRequest const& r) {
        EXPECT_EQ(r.offset(), 2 * quantum);
        EXPECT_THAT(r.payload(), ElementsAre(payload.substr(2 * quantum)));
        return QueryResumableUploadResponse{3 * quantum, std::nullopt};
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
      .WillOnce(Return(QueryResumableUploadResponse{hwm, std::nullopt}))
      .WillOnce(Return(QueryResumableUploadResponse{rollback, std::nullopt}));

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
      .WillOnce(Return(QueryResumableUploadResponse{excess, std::nullopt}));

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
      .WillRepeatedly(Return(QueryResumableUploadResponse{0, std::nullopt}));

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
          Return(QueryResumableUploadResponse{/*.committed_size=*/std::nullopt,
                                              /*.payload=*/std::nullopt}));
  // This should trigger a QueryResumableUpload(), simulate a good response.
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(QueryResumableUploadResponse{quantum, std::nullopt}));

  // The test will create a second request that finalizes the upload. Respond
  // without a committed size also, this should be interpreted as success and
  // not require an additional query.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(Return(QueryResumableUploadResponse{
          /*.committed_size=*/std::nullopt, /*.payload=*/ObjectMetadata()}));

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
          Return(QueryResumableUploadResponse{/*.committed_size=*/std::nullopt,
                                              /*.payload=*/std::nullopt}));
  // This should trigger a `QueryResumableUpload()`, which should also have its
  // Range header missing indicating no bytes were uploaded.
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(
          Return(QueryResumableUploadResponse{std::nullopt, std::nullopt}));

  // This should trigger a second upload, which we will let succeed.
  EXPECT_CALL(*mock, UploadChunk)
      .WillOnce(
          Return(QueryResumableUploadResponse{/*.committed_size=*/quantum,
                                              /*.payload=*/std::nullopt}));

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
      .WillOnce(Return(QueryResumableUploadResponse{quantum, std::nullopt}));
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
          Return(QueryResumableUploadResponse{quantum, std::nullopt}));

  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(
      BasicTestPolicies().set<IdempotencyPolicyOption>(
          StrictIdempotencyPolicy().clone()));

  auto response = client->UploadChunk(
      UploadChunkRequest("test-only-upload-id", 0, {{payload}},
                         CreateNullHashFunction(), HashValues{}));
  ASSERT_THAT(response, Not(IsOk()));
}

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
      .WillOnce(Return(QueryResumableUploadResponse{0, std::nullopt}));
  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(QueryResumableUploadResponse{0, std::nullopt}));
  EXPECT_CALL(*mock, UploadChunk).WillOnce(Return(TransientError()));
  EXPECT_CALL(*mock, QueryResumableUpload)
      .WillOnce(Return(QueryResumableUploadResponse{0, std::nullopt}));
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

// `ReadObject()` positions the child stream it creates, using the same request
// rewrite a resumed or hedged read applies. Verify the request that reaches
// the stub for each way a caller can express a starting position.
StatusOr<std::unique_ptr<ObjectReadSource>> ReadObjectWithRequest(
    ReadObjectRangeRequest const& request,
    std::function<void(ReadObjectRangeRequest const&)> check) {
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options).Times(AtLeast(0));
  EXPECT_CALL(*mock, ReadObject)
      .WillOnce([check = std::move(check)](auto&, auto const&,
                                           ReadObjectRangeRequest const& req) {
        check(req);
        return std::unique_ptr<ObjectReadSource>(
            std::make_unique<testing::MockObjectReadSource>());
      });
  auto client = StorageConnectionImpl::Create(std::move(mock));
  google::cloud::internal::OptionsSpan const span(BasicTestPolicies());
  return client->ReadObject(request);
}

TEST(RetryClientTest, ReadObjectPositionsPlainRequest) {
  ReadObjectRangeRequest request("test-bucket", "test-object");
  EXPECT_THAT(
      ReadObjectWithRequest(request,
                            [](ReadObjectRangeRequest const& req) {
                              // Offset 0 with no range: the rewrite must not
                              // introduce a `ReadFromOffset(0)`, which would
                              // change the request.
                              EXPECT_FALSE(req.HasOption<ReadFromOffset>());
                              EXPECT_FALSE(req.HasOption<ReadLast>());
                              EXPECT_FALSE(req.HasOption<Generation>());
                            }),
      IsOk());
}

TEST(RetryClientTest, ReadObjectPositionsReadFromOffset) {
  ReadObjectRangeRequest request("test-bucket", "test-object");
  request.set_option(ReadFromOffset(1024));
  EXPECT_THAT(ReadObjectWithRequest(
                  request,
                  [](ReadObjectRangeRequest const& req) {
                    EXPECT_EQ(1024,
                              req.GetOption<ReadFromOffset>().value_or(0));
                  }),
              IsOk());
}

TEST(RetryClientTest, ReadObjectPositionsReadRange) {
  ReadObjectRangeRequest request("test-bucket", "test-object");
  request.set_option(ReadRange(100, 200));
  EXPECT_THAT(ReadObjectWithRequest(
                  request,
                  [](ReadObjectRangeRequest const& req) {
                    // The range is preserved, and the offset is pinned to its
                    // start so a resumed read picks up where this one left off.
                    EXPECT_EQ(100, req.GetOption<ReadFromOffset>().value_or(0));
                    ASSERT_TRUE(req.HasOption<ReadRange>());
                    EXPECT_EQ(200, req.GetOption<ReadRange>().value().end);
                  }),
              IsOk());
}

TEST(RetryClientTest, ReadObjectPositionsReadLast) {
  ReadObjectRangeRequest request("test-bucket", "test-object");
  request.set_option(ReadLast(512));
  EXPECT_THAT(ReadObjectWithRequest(
                  request,
                  [](ReadObjectRangeRequest const& req) {
                    // `ReadLast` counts from the end, so it is rewritten in
                    // place and must not become a `ReadFromOffset`.
                    EXPECT_EQ(512, req.GetOption<ReadLast>().value_or(0));
                    EXPECT_FALSE(req.HasOption<ReadFromOffset>());
                  }),
              IsOk());
}

TEST(RetryClientTest, ReadObjectPinsGeneration) {
  ReadObjectRangeRequest request("test-bucket", "test-object");
  request.set_option(Generation(12345));
  request.set_option(ReadFromOffset(64));
  EXPECT_THAT(ReadObjectWithRequest(
                  request,
                  [](ReadObjectRangeRequest const& req) {
                    EXPECT_EQ(12345, req.GetOption<Generation>().value_or(0));
                    EXPECT_EQ(64, req.GetOption<ReadFromOffset>().value_or(0));
                  }),
              IsOk());
}

// Installs a global meter provider for the lifetime of this object, and
// restores the previous one when it goes out of scope.
class ScopedMeterProvider {
 public:
  explicit ScopedMeterProvider(
      opentelemetry::nostd::shared_ptr<
          opentelemetry::metrics::MeterProvider> const& provider)
      : previous_(opentelemetry::metrics::Provider::GetMeterProvider()) {
    opentelemetry::metrics::Provider::SetMeterProvider(provider);
  }
  ~ScopedMeterProvider() {
    opentelemetry::metrics::Provider::SetMeterProvider(previous_);
  }

 private:
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>
      previous_;
};

ReadSourceResult MakeReadResult(std::string const& payload, char* buf) {
  std::copy(payload.begin(), payload.end(), buf);
  return ReadSourceResult{payload.size(),
                          HttpResponse{HttpStatusCode::kOk, {}, {}}};
}

TEST(RetryClientTest, HedgedReadRecordsMetricsOnGlobalMeterProvider) {
  auto dispatched = std::make_unique<MockCounter<std::uint64_t>>();
  auto won = std::make_unique<MockCounter<std::uint64_t>>();
  EXPECT_CALL(*dispatched, Add(std::uint64_t{1})).Times(1);
  EXPECT_CALL(*won, Add(std::uint64_t{1})).Times(1);

  opentelemetry::nostd::shared_ptr<MockMeter> meter =
      std::make_shared<MockMeter>();
  EXPECT_CALL(*meter, CreateUInt64Counter(
                          Eq("storage.read_hedging.hedges_dispatched"), _, _))
      .WillOnce(Return(ByMove(opentelemetry::nostd::unique_ptr<
                              opentelemetry::metrics::Counter<std::uint64_t>>(
          dispatched.release()))));
  EXPECT_CALL(*meter,
              CreateUInt64Counter(Eq("storage.read_hedging.hedge_won"), _, _))
      .WillOnce(Return(ByMove(
          opentelemetry::nostd::unique_ptr<
              opentelemetry::metrics::Counter<std::uint64_t>>(won.release()))));
  auto provider = std::make_shared<MockMeterProvider>();
  EXPECT_CALL(*provider, GetMeter).WillOnce(Return(meter));
  opentelemetry::nostd::shared_ptr<opentelemetry::metrics::MeterProvider>
      global_provider{
          std::shared_ptr<opentelemetry::metrics::MeterProvider>(provider)};
  ScopedMeterProvider const scoped_provider{global_provider};

  // A single read thread, so the first read tells us which thread opens every
  // primary attempt. Any other thread is running a hedge.
  auto primary_thread = std::make_shared<std::thread::id>();
  auto primary_opens = std::make_shared<std::atomic<int>>(0);
  auto unblock_primary = std::make_shared<std::promise<void>>();
  std::shared_future<void> const primary_unblocked =
      unblock_primary->get_future().share();
  auto mock = std::make_unique<MockGenericStub>();
  EXPECT_CALL(*mock, options).Times(AtLeast(0));
  EXPECT_CALL(*mock, ReadObject)
      .WillRepeatedly([primary_thread, primary_opens, primary_unblocked](
                          auto&, auto const&, ReadObjectRangeRequest const&) {
        auto source = std::make_unique<testing::MockObjectReadSource>();
        if (*primary_opens == 0) *primary_thread = std::this_thread::get_id();
        if (std::this_thread::get_id() != *primary_thread) {
          EXPECT_CALL(*source, Read).WillOnce([](char* buf, std::size_t) {
            return MakeReadResult("hedge", buf);
          });
          return StatusOr<std::unique_ptr<ObjectReadSource>>(std::move(source));
        }
        switch ((*primary_opens)++) {
          case 0:
            EXPECT_CALL(*source, Read).WillOnce([](char* buf, std::size_t) {
              return MakeReadResult("warm-up", buf);
            });
            break;
          case 1:
            EXPECT_CALL(*source, Read)
                .WillOnce([primary_unblocked](char* buf, std::size_t) {
                  primary_unblocked.wait();
                  return MakeReadResult("slow", buf);
                });
            EXPECT_CALL(*source, Close).WillOnce([] {
              return make_status_or(HttpResponse{HttpStatusCode::kOk, {}, {}});
            });
            break;
          default:
            EXPECT_CALL(*source, Read).WillOnce([](char* buf, std::size_t) {
              return MakeReadResult("drain", buf);
            });
            break;
        }
        return StatusOr<std::unique_ptr<ObjectReadSource>>(std::move(source));
      });

  auto client = StorageConnectionImpl::Create(
      std::move(mock),
      BasicTestPolicies()
          .set<storage_experimental::EnableReadHedgingOption>(true)
          .set<storage_experimental::MaxReadHedgesOption>(1)
          .set<storage_experimental::MaximumHedgeBufferOption>(1024)
          .set<storage_experimental::ReadThreadPoolSizeOption>(1));
  std::vector<char> buffer(100);

  // Primary answers at once, nothing is hedged.
  {
    google::cloud::internal::OptionsSpan const span(
        client->options().set<storage_experimental::ReadHedgeDelayOption>(
            std::chrono::seconds(30)));
    StatusOr<std::unique_ptr<ObjectReadSource>> source =
        client->ReadObject(ReadObjectRangeRequest("test-bucket", "warm-up"));
    ASSERT_THAT(source, IsOk());
    ASSERT_THAT((*source)->Read(buffer.data(), buffer.size()), IsOk());
  }

  // Primary stalls, the hedge wins.
  StatusOr<std::unique_ptr<ObjectReadSource>> source;
  {
    google::cloud::internal::OptionsSpan const span(
        client->options().set<storage_experimental::ReadHedgeDelayOption>(
            std::chrono::milliseconds(1)));
    source = client->ReadObject(
        ReadObjectRangeRequest("test-bucket", "test-object"));
    ASSERT_THAT(source, IsOk());
    StatusOr<ReadSourceResult> const result =
        (*source)->Read(buffer.data(), buffer.size());
    ASSERT_THAT(result, IsOk());
    EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
                Eq("hedge"));
  }

  // The losing primary is still closing its `RetryObjectReadSource` (which
  // holds a reference to `client`) on the single read thread. A synchronous
  // read on that same thread can only run after the loser's task has finished,
  // so `client` is destroyed on this thread rather than on a detached worker.
  unblock_primary->set_value();
  {
    google::cloud::internal::OptionsSpan const span(
        client->options().set<storage_experimental::ReadHedgeDelayOption>(
            std::chrono::seconds(30)));
    StatusOr<std::unique_ptr<ObjectReadSource>> drain =
        client->ReadObject(ReadObjectRangeRequest("test-bucket", "drain"));
    ASSERT_THAT(drain, IsOk());
    StatusOr<ReadSourceResult> const result =
        (*drain)->Read(buffer.data(), buffer.size());
    ASSERT_THAT(result, IsOk());
    EXPECT_THAT(std::string(buffer.data(), result->bytes_received),
                Eq("drain"));
  }
  EXPECT_EQ(3, primary_opens->load());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

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
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
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
  auto client =
      RetryClient::Create(std::shared_ptr<internal::RawClient>(mock),
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
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock),
                                    BasicTestPolicies());

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
  auto client = RetryClient::Create(std::shared_ptr<internal::RawClient>(mock),
                                    BasicTestPolicies());

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
  auto client = RetryClient::Create(
      std::shared_ptr<internal::RawClient>(mock),
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
  auto client =
      RetryClient::Create(std::shared_ptr<internal::RawClient>(mock),
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
  auto client =
      RetryClient::Create(std::shared_ptr<internal::RawClient>(mock),
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

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

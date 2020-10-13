// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
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
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::chrono_literals::operator"" _us;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;

class RetryClientTest : public ::testing::Test {
 protected:
  void SetUp() override { mock_ = std::make_shared<testing::MockClient>(); }
  void TearDown() override { mock_.reset(); }

  std::shared_ptr<testing::MockClient> mock_;
};

/// @test Verify that non-idempotent operations return on the first failure.
TEST_F(RetryClientTest, NonIdempotentErrorHandling) {
  RetryClient client(std::shared_ptr<internal::RawClient>(mock_),
                     LimitedErrorCountRetryPolicy(3), StrictIdempotencyPolicy(),
                     // Make the tests faster.
                     ExponentialBackoffPolicy(1_us, 2_us, 2));

  EXPECT_CALL(*mock_, DeleteObject(_))
      .WillOnce(Return(StatusOr<EmptyResponse>(TransientError())));

  // Use a delete operation because this is idempotent only if the it has
  // the IfGenerationMatch() and/or Generation() option set.
  StatusOr<EmptyResponse> result =
      client.DeleteObject(DeleteObjectRequest("test-bucket", "test-object"));
  EXPECT_THAT(result, StatusIs(TransientError().code()));
}

/// @test Verify that the retry loop returns on the first permanent failure.
TEST_F(RetryClientTest, PermanentErrorHandling) {
  RetryClient client(std::shared_ptr<internal::RawClient>(mock_),
                     LimitedErrorCountRetryPolicy(3),
                     // Make the tests faster.
                     ExponentialBackoffPolicy(1_us, 2_us, 2));

  // Use a read-only operation because these are always idempotent.
  EXPECT_CALL(*mock_, GetObjectMetadata(_))
      .WillOnce(Return(StatusOr<ObjectMetadata>(TransientError())))
      .WillOnce(Return(StatusOr<ObjectMetadata>(PermanentError())));

  StatusOr<ObjectMetadata> result = client.GetObjectMetadata(
      GetObjectMetadataRequest("test-bucket", "test-object"));
  EXPECT_THAT(result, StatusIs(PermanentError().code()));
}

/// @test Verify that the retry loop returns on the first permanent failure.
TEST_F(RetryClientTest, TooManyTransientsHandling) {
  RetryClient client(std::shared_ptr<internal::RawClient>(mock_),
                     LimitedErrorCountRetryPolicy(3),
                     // Make the tests faster.
                     ExponentialBackoffPolicy(1_us, 2_us, 2));

  // Use a read-only operation because these are always idempotent.
  EXPECT_CALL(*mock_, GetObjectMetadata(_))
      .WillRepeatedly(Return(StatusOr<ObjectMetadata>(TransientError())));

  StatusOr<ObjectMetadata> result = client.GetObjectMetadata(
      GetObjectMetadataRequest("test-bucket", "test-object"));
  EXPECT_THAT(result, StatusIs(TransientError().code()));
}

/// @test Verify that the retry loop works with exhausted retry policy.
TEST_F(RetryClientTest, ExpiredRetryPolicy) {
  RetryClient client(std::shared_ptr<internal::RawClient>(mock_),
                     LimitedTimeRetryPolicy(std::chrono::milliseconds(0)),
                     ExponentialBackoffPolicy(1_us, 2_us, 2));

  StatusOr<ObjectMetadata> result = client.GetObjectMetadata(
      GetObjectMetadataRequest("test-bucket", "test-object"));
  ASSERT_FALSE(result);
  EXPECT_THAT(
      result,
      StatusIs(StatusCode::kDeadlineExceeded,
               HasSubstr("Retry policy exhausted before first attempt")));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

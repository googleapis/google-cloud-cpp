// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/internal/retry_info_helper.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::MockBackoffPolicy;
using ::testing::Optional;
using ::testing::Return;

class MockRetryPolicy : public RetryPolicy {
 public:
  MOCK_METHOD(bool, OnFailure, (Status const&), (override));
  MOCK_METHOD(bool, IsExhausted, (), (const, override));
  MOCK_METHOD(bool, IsPermanentFailure, (Status const&), (const, override));
};

TEST(BackoffOrBreak, HeedsRetryInfo) {
  auto const retry_delay = std::chrono::minutes(5);
  auto status = internal::ResourceExhaustedError("try again");
  internal::SetRetryInfo(status, internal::RetryInfo{retry_delay});

  auto mock_r = std::make_unique<MockRetryPolicy>();
  EXPECT_CALL(*mock_r, OnFailure(status)).WillOnce(Return(false));
  EXPECT_CALL(*mock_r, IsExhausted).WillOnce(Return(false));
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  auto actual = BackoffOrBreak(true, status, *mock_r, *mock_b);
  EXPECT_THAT(actual, Optional(retry_delay));
}

TEST(BackoffOrBreak, NoRetriesIfPolicyExhausted) {
  auto const retry_delay = std::chrono::minutes(5);
  auto status = internal::ResourceExhaustedError("try again");
  internal::SetRetryInfo(status, internal::RetryInfo{retry_delay});

  auto mock_r = std::make_unique<MockRetryPolicy>();
  EXPECT_CALL(*mock_r, OnFailure(status)).WillOnce(Return(false));
  EXPECT_CALL(*mock_r, IsExhausted).WillOnce(Return(true));
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  auto actual = BackoffOrBreak(true, status, *mock_r, *mock_b);
  EXPECT_EQ(actual, absl::nullopt);
}

TEST(BackoffOrBreak, PermanentFailureWhenIgnoringRetryInfo) {
  auto const retry_delay = std::chrono::minutes(5);
  auto status = internal::ResourceExhaustedError("try again");
  internal::SetRetryInfo(status, internal::RetryInfo{retry_delay});

  auto mock_r = std::make_unique<MockRetryPolicy>();
  EXPECT_CALL(*mock_r, OnFailure).WillOnce(Return(false));
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion).Times(0);

  auto actual = BackoffOrBreak(false, status, *mock_r, *mock_b);
  EXPECT_EQ(actual, absl::nullopt);
}

TEST(BackoffOrBreak, TransientFailureWhenIgnoringRetryInfo) {
  auto const retry_delay = std::chrono::minutes(5);
  auto status = internal::UnavailableError("try again");
  internal::SetRetryInfo(status, internal::RetryInfo{retry_delay});

  auto mock_r = std::make_unique<MockRetryPolicy>();
  EXPECT_CALL(*mock_r, OnFailure).WillOnce(Return(true));
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, OnCompletion)
      .WillOnce(Return(std::chrono::milliseconds(10)));

  auto actual = BackoffOrBreak(false, status, *mock_r, *mock_b);
  EXPECT_THAT(actual, Optional(std::chrono::milliseconds(10)));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable_internal
}  // namespace cloud
}  // namespace google

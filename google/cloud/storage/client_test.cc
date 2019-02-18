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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/curl_client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::_;
using ::testing::Return;

class ObservableRetryPolicy : public LimitedErrorCountRetryPolicy {
 public:
  using LimitedErrorCountRetryPolicy::LimitedErrorCountRetryPolicy;

  std::unique_ptr<RetryPolicy> clone() const override {
    return std::unique_ptr<RetryPolicy>(new ObservableRetryPolicy(*this));
  }

  bool IsExhausted() const override {
    ++is_exhausted_call_count;
    return LimitedErrorCountRetryPolicy::IsExhausted();
  }

  static int is_exhausted_call_count;
};
int ObservableRetryPolicy::is_exhausted_call_count;

class ObservableBackoffPolicy : public ExponentialBackoffPolicy {
 public:
  using ExponentialBackoffPolicy::ExponentialBackoffPolicy;

  std::unique_ptr<BackoffPolicy> clone() const override {
    return std::unique_ptr<BackoffPolicy>(new ObservableBackoffPolicy(*this));
  }

  std::chrono::milliseconds OnCompletion() override {
    ++on_completion_call_count;
    return ExponentialBackoffPolicy::OnCompletion();
  }

  static int on_completion_call_count;
};

class ClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock = std::make_shared<testing::MockClient>();
    ObservableRetryPolicy::is_exhausted_call_count = 0;
    ObservableBackoffPolicy::on_completion_call_count = 0;
  }
  void TearDown() override {
    ObservableRetryPolicy::is_exhausted_call_count = 0;
    ObservableBackoffPolicy::on_completion_call_count = 0;
    mock.reset();
  }

  std::shared_ptr<testing::MockClient> mock;
};

int ObservableBackoffPolicy::on_completion_call_count;

TEST_F(ClientTest, OverrideRetryPolicy) {
  Client client{std::shared_ptr<internal::RawClient>(mock),
                ObservableRetryPolicy(3)};

  // Reset the counters at the beginning of the test.

  // Call an API (any API) on the client, we do not care about the status, just
  // that our policy is called.
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce(Return(make_status_or(BucketMetadata{})));
  (void)client.GetBucketMetadata("foo-bar-baz");
  EXPECT_LE(1, ObservableRetryPolicy::is_exhausted_call_count);
  EXPECT_EQ(0, ObservableBackoffPolicy::on_completion_call_count);
}

TEST_F(ClientTest, OverrideBackoffPolicy) {
  using ms = std::chrono::milliseconds;
  Client client{std::shared_ptr<internal::RawClient>(mock),
                ObservableBackoffPolicy(ms(20), ms(100), 2.0)};

  // Call an API (any API) on the client, we do not care about the status, just
  // that our policy is called.
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce(Return(make_status_or(BucketMetadata{})));
  (void)client.GetBucketMetadata("foo-bar-baz");
  EXPECT_EQ(0, ObservableRetryPolicy::is_exhausted_call_count);
  EXPECT_LE(1, ObservableBackoffPolicy::on_completion_call_count);
}

TEST_F(ClientTest, OverrideBothPolicies) {
  using ms = std::chrono::milliseconds;
  Client client{std::shared_ptr<internal::RawClient>(mock),
                ObservableBackoffPolicy(ms(20), ms(100), 2.0),
                ObservableRetryPolicy(3)};

  // Call an API (any API) on the client, we do not care about the status, just
  // that our policy is called.
  EXPECT_CALL(*mock, GetBucketMetadata(_))
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce(Return(make_status_or(BucketMetadata{})));
  (void)client.GetBucketMetadata("foo-bar-baz");
  EXPECT_LE(1, ObservableRetryPolicy::is_exhausted_call_count);
  EXPECT_LE(1, ObservableBackoffPolicy::on_completion_call_count);
}

/// @test Verify the constructor creates the right set of RawClient decorations.
TEST_F(ClientTest, DefaultDecorators) {
  // Create a client, use the anonymous credentials because on the CI
  // environment there may not be other credentials configured.
  Client tested(oauth2::CreateAnonymousCredentials());

  EXPECT_TRUE(tested.raw_client() != nullptr);
  auto retry = dynamic_cast<internal::RetryClient*>(tested.raw_client().get());
  ASSERT_TRUE(retry != nullptr);

  auto logging = dynamic_cast<internal::LoggingClient*>(retry->client().get());
  ASSERT_TRUE(logging != nullptr);

  auto curl = dynamic_cast<internal::CurlClient*>(logging->client().get());
  ASSERT_TRUE(curl != nullptr);
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

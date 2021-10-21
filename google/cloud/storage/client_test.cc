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
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::internal::ClientImplDetails;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::Return;

class ObservableRetryPolicy : public LimitedErrorCountRetryPolicy {
 public:
  using LimitedErrorCountRetryPolicy::LimitedErrorCountRetryPolicy;

  std::unique_ptr<LimitedErrorCountRetryPolicy::BaseType> clone()
      const override {
    return std::unique_ptr<LimitedErrorCountRetryPolicy::BaseType>(
        new ObservableRetryPolicy(*this));
  }

  bool IsExhausted() const override {
    ++is_exhausted_call_count_;
    return LimitedErrorCountRetryPolicy::IsExhausted();
  }

  static int is_exhausted_call_count_;
};
int ObservableRetryPolicy::is_exhausted_call_count_;

class ObservableBackoffPolicy : public ExponentialBackoffPolicy {
 public:
  using ExponentialBackoffPolicy::ExponentialBackoffPolicy;

  std::unique_ptr<BackoffPolicy> clone() const override {
    return std::unique_ptr<BackoffPolicy>(new ObservableBackoffPolicy(*this));
  }

  std::chrono::milliseconds OnCompletion() override {
    ++on_completion_call_count_;
    return ExponentialBackoffPolicy::OnCompletion();
  }

  static int on_completion_call_count_;
};
int ObservableBackoffPolicy::on_completion_call_count_;

class ClientTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_ = std::make_shared<testing::MockClient>();
    ObservableRetryPolicy::is_exhausted_call_count_ = 0;
    ObservableBackoffPolicy::on_completion_call_count_ = 0;
  }
  void TearDown() override {
    ObservableRetryPolicy::is_exhausted_call_count_ = 0;
    ObservableBackoffPolicy::on_completion_call_count_ = 0;
    mock_.reset();
  }

  std::shared_ptr<testing::MockClient> mock_;
};

TEST_F(ClientTest, OverrideRetryPolicy) {
  auto client = internal::ClientImplDetails::CreateClient(
      std::shared_ptr<internal::RawClient>(mock_), ObservableRetryPolicy(3));

  // Reset the counters at the beginning of the test.

  // Call an API (any API) on the client, we do not care about the status, just
  // that our policy is called.
  EXPECT_CALL(*mock_, GetBucketMetadata)
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce(Return(make_status_or(BucketMetadata{})));
  (void)client.GetBucketMetadata("foo-bar-baz");
  EXPECT_LE(1, ObservableRetryPolicy::is_exhausted_call_count_);
  EXPECT_EQ(0, ObservableBackoffPolicy::on_completion_call_count_);
}

TEST_F(ClientTest, OverrideBackoffPolicy) {
  using ms = std::chrono::milliseconds;
  auto client = internal::ClientImplDetails::CreateClient(
      std::shared_ptr<internal::RawClient>(mock_),
      ObservableBackoffPolicy(ms(20), ms(100), 2.0));

  // Call an API (any API) on the client, we do not care about the status, just
  // that our policy is called.
  EXPECT_CALL(*mock_, GetBucketMetadata)
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce(Return(make_status_or(BucketMetadata{})));
  (void)client.GetBucketMetadata("foo-bar-baz");
  EXPECT_EQ(0, ObservableRetryPolicy::is_exhausted_call_count_);
  EXPECT_LE(1, ObservableBackoffPolicy::on_completion_call_count_);
}

TEST_F(ClientTest, OverrideBothPolicies) {
  using ms = std::chrono::milliseconds;
  auto client = internal::ClientImplDetails::CreateClient(
      std::shared_ptr<internal::RawClient>(mock_),
      ObservableBackoffPolicy(ms(20), ms(100), 2.0), ObservableRetryPolicy(3));

  // Call an API (any API) on the client, we do not care about the status, just
  // that our policy is called.
  EXPECT_CALL(*mock_, GetBucketMetadata)
      .WillOnce(Return(StatusOr<BucketMetadata>(TransientError())))
      .WillOnce(Return(make_status_or(BucketMetadata{})));
  (void)client.GetBucketMetadata("foo-bar-baz");
  EXPECT_LE(1, ObservableRetryPolicy::is_exhausted_call_count_);
  EXPECT_LE(1, ObservableBackoffPolicy::on_completion_call_count_);
}

/// @test Verify the constructor creates the right set of RawClient decorations.
TEST_F(ClientTest, DefaultDecorators) {
  ScopedEnvironment disable_grpc("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  // Create a client, use the anonymous credentials because on the CI
  // environment there may not be other credentials configured.
  auto tested =
      Client(Options{}
                 .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
                 .set<TracingComponentsOption>({}));

  EXPECT_TRUE(ClientImplDetails::GetRawClient(tested) != nullptr);
  auto* retry = dynamic_cast<internal::RetryClient*>(
      ClientImplDetails::GetRawClient(tested).get());
  ASSERT_TRUE(retry != nullptr);

  auto* curl = dynamic_cast<internal::CurlClient*>(retry->client().get());
  ASSERT_TRUE(curl != nullptr);
}

/// @test Verify the constructor creates the right set of RawClient decorations.
TEST_F(ClientTest, LoggingDecorators) {
  // Create a client, use the anonymous credentials because on the CI
  // environment there may not be other credentials configured.
  auto tested =
      Client(Options{}
                 .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
                 .set<TracingComponentsOption>({"raw-client"}));

  EXPECT_TRUE(ClientImplDetails::GetRawClient(tested) != nullptr);
  auto* retry = dynamic_cast<internal::RetryClient*>(
      ClientImplDetails::GetRawClient(tested).get());
  ASSERT_TRUE(retry != nullptr);

  auto* logging = dynamic_cast<internal::LoggingClient*>(retry->client().get());
  ASSERT_TRUE(logging != nullptr);

  auto* curl = dynamic_cast<internal::CurlClient*>(logging->client().get());
  ASSERT_TRUE(curl != nullptr);
}

#include "google/cloud/internal/disable_deprecation_warnings.inc"

TEST_F(ClientTest, DeprecatedButNotDecommissioned) {
  auto m1 = std::make_shared<testing::MockClient>();

  auto c1 = storage::Client(m1, Client::NoDecorations{});
  EXPECT_EQ(c1.raw_client().get(), m1.get());

  auto m2 = std::make_shared<testing::MockClient>();
  auto c2 = storage::Client(m2, LimitedErrorCountRetryPolicy(3));
  EXPECT_NE(c2.raw_client().get(), m2.get());
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

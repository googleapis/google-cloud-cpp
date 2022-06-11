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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/curl_client.h"
#include "google/cloud/storage/internal/rest_client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::internal::ClientImplDetails;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::MockBackoffPolicy;
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

TEST_F(ClientTest, Equality) {
  auto a =
      storage::Client(Options{}.set<google::cloud::UnifiedCredentialsOption>(
          google::cloud::MakeInsecureCredentials()));
  auto b =
      storage::Client(Options{}.set<google::cloud::UnifiedCredentialsOption>(
          google::cloud::MakeInsecureCredentials()));
  EXPECT_TRUE(a != b);
  EXPECT_TRUE(a == a);
  EXPECT_TRUE(b == b);
  auto c = a;  // NOLINT(performance-unnecessary-copy-initialization)
  EXPECT_TRUE(a == c);
  b = std::move(a);
  EXPECT_TRUE(b == c);
}

TEST_F(ClientTest, OverrideRetryPolicy) {
  auto client = testing::ClientFromMock(mock_, ObservableRetryPolicy(3));

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
  auto client = testing::ClientFromMock(
      mock_, ObservableBackoffPolicy(ms(20), ms(100), 2.0));

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
  auto client = testing::ClientFromMock(
      mock_, ObservableBackoffPolicy(ms(20), ms(100), 2.0),
      ObservableRetryPolicy(3));

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
TEST_F(ClientTest, DefaultDecoratorsCurlClient) {
  ScopedEnvironment disable_grpc("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  ScopedEnvironment disable_rest("GOOGLE_CLOUD_CPP_STORAGE_HAVE_REST_CLIENT",
                                 absl::nullopt);

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
TEST_F(ClientTest, LoggingDecoratorsCurlClient) {
  ScopedEnvironment disable_rest("GOOGLE_CLOUD_CPP_STORAGE_HAVE_REST_CLIENT",
                                 absl::nullopt);
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

/// @test Verify the constructor creates the right set of RawClient decorations.
TEST_F(ClientTest, DefaultDecoratorsRestClient) {
  ScopedEnvironment disable_grpc("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  ScopedEnvironment enable_rest("GOOGLE_CLOUD_CPP_STORAGE_HAVE_REST_CLIENT",
                                "yes");

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

  auto* rest = dynamic_cast<internal::RestClient*>(retry->client().get());
  ASSERT_TRUE(rest != nullptr);
}

/// @test Verify the constructor creates the right set of RawClient decorations.
TEST_F(ClientTest, LoggingDecoratorsRestClient) {
  ScopedEnvironment enable_rest("GOOGLE_CLOUD_CPP_STORAGE_HAVE_REST_CLIENT",
                                "yes");
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

  auto* rest = dynamic_cast<internal::RestClient*>(logging->client().get());
  ASSERT_TRUE(rest != nullptr);
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

TEST_F(ClientTest, DeprecatedRetryPolicies) {
  auto constexpr kNumRetries = 2;
  auto mock_b = absl::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([=] {
    auto clone_1 = absl::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone_1, clone).WillOnce([=] {
      auto clone_2 = absl::make_unique<MockBackoffPolicy>();
      EXPECT_CALL(*clone_2, OnCompletion)
          .Times(kNumRetries)
          .WillRepeatedly(Return(std::chrono::milliseconds(0)));
      return clone_2;
    });
    return clone_1;
  });

  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ListBuckets)
      .Times(kNumRetries + 1)
      .WillRepeatedly(Return(TransientError()));

  auto client = storage::Client(mock, LimitedErrorCountRetryPolicy(kNumRetries),
                                std::move(*mock_b));
  (void)client.ListBuckets();
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

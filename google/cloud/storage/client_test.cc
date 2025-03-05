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
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/testing_util/mock_backoff_policy.h"
#include "google/cloud/testing_util/opentelemetry_matchers.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>
#include <memory>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::storage::internal::ClientImplDetails;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::MockBackoffPolicy;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::ElementsAre;
using ::testing::NotNull;
using ::testing::Return;

class ObservableRetryPolicy : public LimitedErrorCountRetryPolicy {
 public:
  using LimitedErrorCountRetryPolicy::LimitedErrorCountRetryPolicy;

  std::unique_ptr<google::cloud::RetryPolicy> clone() const override {
    return std::unique_ptr<google::cloud::RetryPolicy>(
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

/// @test Verify the constructor creates the right set of StorageConnection
/// decorations.
TEST_F(ClientTest, DefaultDecoratorsRestClient) {
  ScopedEnvironment disable_grpc("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);

  // Create a client, use the anonymous credentials because on the CI
  // environment there may not be other credentials configured.
  auto tested =
      Client(Options{}
                 .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
                 .set<LoggingComponentsOption>({}));

  auto const impl = ClientImplDetails::GetConnection(tested);
  ASSERT_THAT(impl, NotNull());
  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("RestStub", "StorageConnectionImpl"));
}

/// @test Verify the constructor creates the right set of StorageConnection
/// decorations.
TEST_F(ClientTest, LoggingDecoratorsRestClient) {
  ScopedEnvironment logging("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  ScopedEnvironment legacy("GOOGLE_CLOUD_CPP_STORAGE_USE_LEGACY_HTTP",
                           absl::nullopt);

  // Create a client, use the anonymous credentials because on the CI
  // environment there may not be other credentials configured.
  auto tested =
      Client(Options{}
                 .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
                 .set<LoggingComponentsOption>({"raw-client"}));

  auto const impl = ClientImplDetails::GetConnection(tested);
  ASSERT_THAT(impl, NotNull());
  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("RestStub", "LoggingStub", "StorageConnectionImpl"));
}

#ifdef GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY
using ::google::cloud::testing_util::DisableTracing;
using ::google::cloud::testing_util::EnableTracing;

TEST_F(ClientTest, OTelEnableTracing) {
  ScopedEnvironment logging("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  ScopedEnvironment legacy("GOOGLE_CLOUD_CPP_STORAGE_USE_LEGACY_HTTP",
                           absl::nullopt);

  // Create a client. Use the anonymous credentials because on the CI
  // environment there may not be other credentials configured.
  auto options = Options{}
                     .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
                     .set<LoggingComponentsOption>({"raw-client"});

  auto tested = Client(EnableTracing(std::move(options)));
  auto const impl = ClientImplDetails::GetConnection(tested);
  ASSERT_THAT(impl, NotNull());

  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("RestStub", "LoggingStub", "StorageConnectionImpl",
                          "TracingConnection"));
}

TEST_F(ClientTest, OTelDisableTracing) {
  ScopedEnvironment logging("CLOUD_STORAGE_ENABLE_TRACING", absl::nullopt);
  ScopedEnvironment legacy("GOOGLE_CLOUD_CPP_STORAGE_USE_LEGACY_HTTP",
                           absl::nullopt);

  // Create a client. Use the anonymous credentials because on the CI
  // environment there may not be other credentials configured.
  auto options = Options{}
                     .set<UnifiedCredentialsOption>(MakeInsecureCredentials())
                     .set<LoggingComponentsOption>({"raw-client"});

  auto tested = Client(DisableTracing(std::move(options)));
  auto const impl = ClientImplDetails::GetConnection(tested);
  ASSERT_THAT(impl, NotNull());

  EXPECT_THAT(impl->InspectStackStructure(),
              ElementsAre("RestStub", "LoggingStub", "StorageConnectionImpl"));
}

#endif  // GOOGLE_CLOUD_CPP_HAVE_OPENTELEMETRY

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
  auto mock_b = std::make_unique<MockBackoffPolicy>();
  EXPECT_CALL(*mock_b, clone).WillOnce([=] {
    auto clone_1 = std::make_unique<MockBackoffPolicy>();
    EXPECT_CALL(*clone_1, clone).WillOnce([=] {
      auto clone_2 = std::make_unique<MockBackoffPolicy>();
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
  (void)client.ListBuckets(OverrideDefaultProject("fake-project"));
}

TEST_F(ClientTest, DeprecatedClientFromMock) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = testing::ClientFromMock(mock);

  internal::ListObjectsResponse response;
  response.items.push_back(
      ObjectMetadata{}.set_bucket("bucket").set_name("object/1"));
  response.items.push_back(
      ObjectMetadata{}.set_bucket("bucket").set_name("object/2"));
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce(Return(TransientError()))
      .WillOnce(Return(response));

  auto stream = client.ListObjects("bucket", Prefix("object/"));
  std::vector<StatusOr<ObjectMetadata>> objects{stream.begin(), stream.end()};
  EXPECT_THAT(objects, ElementsAre(IsOkAndHolds(response.items[0]),
                                   IsOkAndHolds(response.items[1])));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

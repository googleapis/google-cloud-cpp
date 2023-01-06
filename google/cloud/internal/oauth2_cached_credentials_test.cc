// Copyright 2022 Google LLC
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

#include "google/cloud/internal/oauth2_cached_credentials.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/internal/oauth2_credential_constants.h"
#include "google/cloud/testing_util/chrono_output.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::UnavailableError;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAreArray;
using ::testing::Return;

class MockCredentials : public Credentials {
 public:
  MOCK_METHOD(StatusOr<internal::AccessToken>, GetToken,
              (std::chrono::system_clock::time_point), (override));
  MOCK_METHOD(StatusOr<std::vector<std::uint8_t>>, SignBlob,
              (absl::optional<std::string> const&, std::string const&),
              (const, override));
  MOCK_METHOD(std::string, AccountEmail, (), (const, override));
  MOCK_METHOD(std::string, KeyId, (), (const, override));
};

TEST(CachedCredentials, GetTokenUncached) {
  auto mock = std::make_shared<MockCredentials>();
  auto const now = std::chrono::system_clock::now();
  auto const tp = now + std::chrono::seconds(123);
  auto const expected =
      internal::AccessToken{"test-token", now + std::chrono::hours(1)};
  EXPECT_CALL(*mock, GetToken(tp)).WillOnce(Return(expected));
  CachedCredentials tested(mock);
  auto actual = tested.GetToken(tp);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, expected);
}

TEST(CachedCredentials, GetTokenReuseWhileNotExpired) {
  auto mock = std::make_shared<MockCredentials>();
  auto const now = std::chrono::system_clock::now();
  auto const expected =
      internal::AccessToken{"test-token", now + std::chrono::hours(1)};
  EXPECT_CALL(*mock, GetToken).WillOnce(Return(expected));
  CachedCredentials tested(mock);
  auto const stop =
      expected.expiration - GoogleOAuthAccessTokenExpirationSlack();
  for (auto tp = now; tp < stop; tp += std::chrono::seconds(5)) {
    auto actual = tested.GetToken(tp);
    ASSERT_STATUS_OK(actual)
        << "now=" << now << ", expected=" << expected << ", tp=" << tp;
    EXPECT_EQ(*actual, expected)
        << "now=" << now << ", expected=" << expected << ", tp=" << tp;
  }
}

TEST(CachedCredentials, GetTokenExpiredRefresh) {
  auto mock = std::make_shared<MockCredentials>();
  auto const now = std::chrono::system_clock::now();
  auto const tp1 = now;
  auto const e1 =
      internal::AccessToken{"test-token", now + std::chrono::hours(1)};
  auto const tp2 = now + std::chrono::hours(1) + std::chrono::minutes(1);
  auto const e2 =
      internal::AccessToken{"test-token", now + std::chrono::hours(2)};
  EXPECT_CALL(*mock, GetToken(tp1)).WillOnce(Return(e1));
  EXPECT_CALL(*mock, GetToken(tp2)).WillOnce(Return(e2));
  CachedCredentials tested(mock);
  auto a1 = tested.GetToken(tp1);
  ASSERT_STATUS_OK(a1);
  EXPECT_EQ(*a1, e1);

  auto a2 = tested.GetToken(tp2);
  ASSERT_STATUS_OK(a2);
  EXPECT_EQ(*a2, e2);
}

TEST(CachedCredentials, GetTokenExpiringReuseOnError) {
  auto mock = std::make_shared<MockCredentials>();
  auto const now = std::chrono::system_clock::now();
  auto const tp1 = now;
  auto const e1 =
      internal::AccessToken{"test-token", now + std::chrono::hours(1)};
  auto const tp2 = e1.expiration - GoogleOAuthAccessTokenExpirationSlack() +
                   std::chrono::seconds(1);
  EXPECT_CALL(*mock, GetToken(tp1)).WillOnce(Return(e1));
  EXPECT_CALL(*mock, GetToken(tp2))
      .WillOnce(Return(UnavailableError("try-again")));
  CachedCredentials tested(mock);
  auto a1 = tested.GetToken(tp1);
  ASSERT_STATUS_OK(a1);
  EXPECT_EQ(*a1, e1);

  auto a2 = tested.GetToken(tp2);
  ASSERT_STATUS_OK(a2);
  EXPECT_EQ(*a2, e1);
}

TEST(CachedCredentials, GetTokenExpiredWithError) {
  auto mock = std::make_shared<MockCredentials>();
  auto const now = std::chrono::system_clock::now();
  auto const e1 =
      internal::AccessToken{"test-token", now + std::chrono::hours(1)};
  auto const tp1 = now;
  auto const tp2 = e1.expiration + std::chrono::seconds(1);
  EXPECT_CALL(*mock, GetToken(tp1)).WillOnce(Return(e1));
  EXPECT_CALL(*mock, GetToken(tp2))
      .WillOnce(Return(UnavailableError("try-again")));
  CachedCredentials tested(mock);
  auto a1 = tested.GetToken(tp1);
  ASSERT_STATUS_OK(a1);
  EXPECT_EQ(*a1, e1);

  auto a2 = tested.GetToken(tp2);
  EXPECT_THAT(a2, StatusIs(StatusCode::kUnavailable));
}

TEST(CachedCredentials, SignBlob) {
  auto mock = std::make_shared<MockCredentials>();
  auto const expected = std::vector<std::uint8_t>{1, 2, 3};
  EXPECT_CALL(*mock, SignBlob(absl::make_optional(std::string("test-account")),
                              "test-blob"))
      .WillOnce(Return(expected));
  CachedCredentials tested(mock);
  auto actual = tested.SignBlob("test-account", "test-blob");
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, ElementsAreArray(expected));
}

TEST(CachedCredentials, AccountEmail) {
  auto mock = std::make_shared<MockCredentials>();
  EXPECT_CALL(*mock, AccountEmail).WillOnce(Return("test-account-email"));
  CachedCredentials tested(mock);
  EXPECT_EQ(tested.AccountEmail(), "test-account-email");
}

TEST(CachedCredentials, KeyId) {
  auto mock = std::make_shared<MockCredentials>();
  EXPECT_CALL(*mock, KeyId).WillOnce(Return("test-key-id"));
  CachedCredentials tested(mock);
  EXPECT_EQ(tested.KeyId(), "test-key-id");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

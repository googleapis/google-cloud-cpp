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

#include "google/cloud/internal/oauth2_logging_credentials.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/contains_once.h"
#include "google/cloud/testing_util/scoped_log.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::UnavailableError;
using ::google::cloud::testing_util::ContainsOnce;
using ::google::cloud::testing_util::ScopedLog;
using ::google::cloud::testing_util::StatusIs;
using ::testing::AllOf;
using ::testing::HasSubstr;
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

TEST(LoggingCredentials, GetTokenSuccess) {
  auto mock = std::make_shared<MockCredentials>();
  auto const now = std::chrono::system_clock::now();
  auto const expiration = now + std::chrono::hours(1);
  auto const expected = internal::AccessToken{"test-token", expiration};
  auto const tp = now + std::chrono::seconds(123);
  EXPECT_CALL(*mock, GetToken(tp)).WillOnce(Return(expected));
  ScopedLog log;
  LoggingCredentials tested("testing", TracingOptions(), mock);
  auto actual = tested.GetToken(tp);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, expected);
  EXPECT_THAT(log.ExtractLines(),
              ContainsOnce(AllOf(HasSubstr("GetToken(testing)"),
                                 HasSubstr("will expire in 57m57s"))));
}

TEST(LoggingCredentials, GetTokenExpired) {
  auto mock = std::make_shared<MockCredentials>();
  auto const now = std::chrono::system_clock::now();
  auto const expiration = now + std::chrono::hours(1);
  auto const expected = internal::AccessToken{"test-token", expiration};
  auto const tp = now + std::chrono::hours(1) + std::chrono::minutes(1);
  EXPECT_CALL(*mock, GetToken(tp)).WillOnce(Return(expected));
  ScopedLog log;
  LoggingCredentials tested("testing", TracingOptions(), mock);
  auto actual = tested.GetToken(tp);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, expected);
  EXPECT_THAT(log.ExtractLines(),
              ContainsOnce(AllOf(HasSubstr("GetToken(testing)"),
                                 HasSubstr("token expired 1m ago"))));
}

TEST(LoggingCredentials, GetTokenError) {
  auto mock = std::make_shared<MockCredentials>();
  auto const now = std::chrono::system_clock::now();
  auto const tp = now + std::chrono::seconds(123);
  EXPECT_CALL(*mock, GetToken(tp))
      .WillOnce(Return(UnavailableError("try-again")));
  ScopedLog log;
  LoggingCredentials tested("testing", TracingOptions(), mock);
  auto actual = tested.GetToken(tp);
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
  EXPECT_THAT(log.ExtractLines(),
              ContainsOnce(AllOf(HasSubstr("GetToken(testing) failed"),
                                 HasSubstr("try-again"))));
}

TEST(LoggingCredentials, SignBlob) {
  auto mock = std::make_shared<MockCredentials>();
  auto const expected = std::vector<std::uint8_t>{1, 2, 3};
  EXPECT_CALL(*mock, SignBlob(absl::make_optional(std::string("test-account")),
                              "test-blob"))
      .WillOnce(Return(expected));
  ScopedLog log;
  LoggingCredentials tested("testing", TracingOptions(), mock);
  auto actual = tested.SignBlob("test-account", "test-blob");
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(
      log.ExtractLines(),
      ContainsOnce(AllOf(HasSubstr("SignBlob(testing)"),
                         HasSubstr("signing_service_account=test-account"))));
}

TEST(LoggingCredentials, SignBlobOptional) {
  auto mock = std::make_shared<MockCredentials>();
  auto const expected = std::vector<std::uint8_t>{1, 2, 3};
  EXPECT_CALL(*mock, SignBlob(absl::optional<std::string>{}, "test-blob"))
      .WillOnce(Return(expected));
  ScopedLog log;
  LoggingCredentials tested("testing", TracingOptions(), mock);
  auto actual = tested.SignBlob(absl::nullopt, "test-blob");
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(
      log.ExtractLines(),
      ContainsOnce(AllOf(HasSubstr("SignBlob(testing)"),
                         HasSubstr("signing_service_account=<not set>"))));
}

TEST(LoggingCredentials, AccountEmail) {
  auto mock = std::make_shared<MockCredentials>();
  EXPECT_CALL(*mock, AccountEmail).WillOnce(Return("test-account-email"));
  ScopedLog log;
  LoggingCredentials tested("testing", TracingOptions(), mock);
  auto const actual = tested.AccountEmail();
  EXPECT_EQ(actual, "test-account-email");
  EXPECT_THAT(log.ExtractLines(),
              ContainsOnce(HasSubstr("AccountEmail(testing)")));
}

TEST(LoggingCredentials, KeyId) {
  auto mock = std::make_shared<MockCredentials>();
  EXPECT_CALL(*mock, KeyId).WillOnce(Return("test-key-id"));
  ScopedLog log;
  LoggingCredentials tested("testing", TracingOptions(), mock);
  auto const actual = tested.KeyId();
  EXPECT_EQ(actual, "test-key-id");
  EXPECT_THAT(log.ExtractLines(), ContainsOnce(HasSubstr("KeyId(testing)")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

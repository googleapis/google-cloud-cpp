// Copyright 2021 Google LLC
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

#include "google/cloud/internal/oauth2_impersonate_service_account_credentials.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::AccessTokenLifetimeOption;
using ::google::cloud::internal::AccessToken;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::std::chrono::minutes;
using ::std::chrono::seconds;
using ::testing::EndsWith;
using ::testing::Eq;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StartsWith;

class MockMinimalIamCredentialsRest : public MinimalIamCredentialsRest {
 public:
  MOCK_METHOD(StatusOr<google::cloud::internal::AccessToken>,
              GenerateAccessToken, (GenerateAccessTokenRequest const&),
              (override));
};

TEST(ImpersonateServiceAccountCredentialsTest, Basic) {
  auto const now = std::chrono::system_clock::now();

  auto mock = std::make_shared<MockMinimalIamCredentialsRest>();
  EXPECT_CALL(*mock, GenerateAccessToken)
      .WillOnce(
          Return(make_status_or(AccessToken{"token1", now + minutes(15)})))
      .WillOnce(
          Return(make_status_or(AccessToken{"token2", now + minutes(30)})))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "")))
      .WillOnce(Return(Status(StatusCode::kPermissionDenied, "")));

  auto config = google::cloud::internal::ImpersonateServiceAccountConfig(
      google::cloud::MakeGoogleDefaultCredentials(),
      "test-only-invalid@test.invalid",
      Options{}.set<AccessTokenLifetimeOption>(std::chrono::minutes(15)));

  MockFunction<ImpersonateServiceAccountCredentials::CurrentTimeFn>
      mock_current_time_fn;

  EXPECT_CALL(mock_current_time_fn, Call())
      .WillOnce([&] { return now + seconds(1790); })
      .WillOnce([&] { return now + minutes(45); });

  ImpersonateServiceAccountCredentials under_test(
      config, mock, mock_current_time_fn.AsStdFunction());

  for (auto const i : {1, 5, 9}) {
    SCOPED_TRACE("Testing with i = " + std::to_string(i));
    auto header = under_test.AuthorizationHeader(now + minutes(i));
    ASSERT_THAT(header, IsOk());
    EXPECT_THAT(header->first, Eq("Authorization"));
    EXPECT_THAT(header->second, StartsWith("Bearer"));
    EXPECT_THAT(header->second, EndsWith("token1"));
  }

  auto header = under_test.AuthorizationHeader(now + minutes(20));
  ASSERT_THAT(header, IsOk());
  EXPECT_THAT(header->first, Eq("Authorization"));
  EXPECT_THAT(header->second, StartsWith("Bearer"));
  EXPECT_THAT(header->second, EndsWith("token2"));

  header = under_test.AuthorizationHeader(now + seconds(1790));
  ASSERT_THAT(header, IsOk());
  EXPECT_THAT(header->first, Eq("Authorization"));
  EXPECT_THAT(header->second, StartsWith("Bearer"));
  EXPECT_THAT(header->second, EndsWith("token2"));

  header = under_test.AuthorizationHeader(now + minutes(45));
  ASSERT_THAT(header, StatusIs(StatusCode::kPermissionDenied));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

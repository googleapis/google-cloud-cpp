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

#include "google/cloud/internal/oauth2_refreshing_credentials_wrapper.h"
#include "google/cloud/internal/oauth2_credential_constants.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::std::chrono::minutes;
using ::std::chrono::seconds;
using ::testing::Eq;
using ::testing::MockFunction;
using ::testing::Not;

TEST(RefreshingCredentialsWrapper, IsValid) {
  auto now = std::chrono::system_clock::now();
  MockFunction<RefreshingCredentialsWrapper::CurrentTimeFn>
      mock_current_time_fn;
  EXPECT_CALL(mock_current_time_fn, Call()).WillOnce([&] { return now; });

  RefreshingCredentialsWrapper w(mock_current_time_fn.AsStdFunction());
  std::pair<std::string, std::string> const auth_token =
      std::make_pair("Authorization", "foo");
  auto refresh_fn = [&] {
    internal::AccessToken token;
    token.token = auth_token.second;
    token.expiration = now + minutes(60);
    return make_status_or(token);
  };
  auto token = w.AuthorizationHeader(refresh_fn);
  EXPECT_TRUE(w.IsValid());
}

TEST(RefreshingCredentialsWrapper, IsNotValid) {
  std::pair<std::string, std::string> const auth_token =
      std::make_pair("Authorization", "foo");
  RefreshingCredentialsWrapper w;
  EXPECT_FALSE(w.IsValid());
}

TEST(RefreshingCredentialsWrapper, RefreshTokenSuccess) {
  auto now = std::chrono::system_clock::now();
  MockFunction<RefreshingCredentialsWrapper::CurrentTimeFn>
      mock_current_time_fn;
  EXPECT_CALL(mock_current_time_fn, Call()).WillOnce([&] { return now; });

  RefreshingCredentialsWrapper w(mock_current_time_fn.AsStdFunction());
  std::pair<std::string, std::string> const auth_token =
      std::make_pair("Authorization", "foo");

  // Test that we only call the refresh_fn on the first call to
  // AuthorizationHeader.
  testing::MockFunction<absl::FunctionRef<StatusOr<internal::AccessToken>()>>
      mock_refresh_fn;
  EXPECT_CALL(mock_refresh_fn, Call()).WillOnce([&] {
    internal::AccessToken token;
    token.token = auth_token.second;
    token.expiration = now + minutes(60);
    return make_status_or(token);
  });
  auto token = w.AuthorizationHeader(mock_refresh_fn.AsStdFunction());
  ASSERT_THAT(token, IsOk());
  EXPECT_THAT(*token, Eq(auth_token));

  token = w.AuthorizationHeader(mock_refresh_fn.AsStdFunction());
  ASSERT_THAT(token, IsOk());
  EXPECT_THAT(*token, Eq(auth_token));
}

TEST(RefreshingCredentialsWrapper, RefreshTokenFailure) {
  auto refresh_fn = [&]() -> StatusOr<internal::AccessToken> {
    return Status(StatusCode::kInvalidArgument, {}, {});
  };
  RefreshingCredentialsWrapper w;
  auto token = w.AuthorizationHeader(refresh_fn);
  EXPECT_THAT(token, Not(IsOk()));
  EXPECT_THAT(token.status().code(), Eq(StatusCode::kInvalidArgument));
}

TEST(RefreshingCredentialsWrapper, RefreshTokenFailureValidToken) {
  auto now = std::chrono::system_clock::now();
  auto expire_time = now + minutes(60);

  MockFunction<RefreshingCredentialsWrapper::CurrentTimeFn>
      mock_current_time_fn;
  EXPECT_CALL(mock_current_time_fn, Call())
      .WillOnce([&] {
        return expire_time + GoogleOAuthAccessTokenExpirationSlack() +
               seconds(10);
      })
      .WillOnce([&] { return now; });

  std::pair<std::string, std::string> const auth_token =
      std::make_pair("Authorization", "foo");
  RefreshingCredentialsWrapper w(mock_current_time_fn.AsStdFunction());
  auto refresh_fn = [&] {
    internal::AccessToken token;
    token.token = auth_token.second;
    token.expiration = expire_time;
    return make_status_or(token);
  };
  auto token = w.AuthorizationHeader(refresh_fn);

  auto failing_refresh_fn = []() -> StatusOr<internal::AccessToken> {
    return Status(StatusCode::kInvalidArgument, {}, {});
  };
  token = w.AuthorizationHeader(failing_refresh_fn);
  ASSERT_THAT(token, IsOk());
  EXPECT_THAT(*token, Eq(auth_token));
}

TEST(RefreshingCredentialsWrapper, RefreshTokenFailureInvalidToken) {
  auto now = std::chrono::system_clock::now();
  MockFunction<RefreshingCredentialsWrapper::CurrentTimeFn>
      mock_current_time_fn;
  auto expire_time = now + minutes(60);
  EXPECT_CALL(mock_current_time_fn, Call()).Times(2).WillRepeatedly([&] {
    return expire_time + GoogleOAuthAccessTokenExpirationSlack() + seconds(10);
  });

  std::pair<std::string, std::string> const auth_token =
      std::make_pair("Authorization", "foo");
  RefreshingCredentialsWrapper w(mock_current_time_fn.AsStdFunction());
  auto refresh_fn = [&] {
    internal::AccessToken token;
    token.token = auth_token.second;
    token.expiration = expire_time;
    return make_status_or(token);
  };
  auto token = w.AuthorizationHeader(refresh_fn);

  auto failing_refresh_fn = [&]() -> StatusOr<internal::AccessToken> {
    return Status(StatusCode::kInvalidArgument, {}, {});
  };
  token = w.AuthorizationHeader(failing_refresh_fn);
  EXPECT_THAT(token, Not(IsOk()));
  EXPECT_THAT(token.status().code(), Eq(StatusCode::kInvalidArgument));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

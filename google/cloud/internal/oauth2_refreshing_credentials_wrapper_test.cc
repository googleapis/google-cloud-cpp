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
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::Eq;
using ::testing::Not;

TEST(RefreshingCredentialsWrapper, IsExpired) {
  std::pair<std::string, std::string> const auth_token =
      std::make_pair("Authorization", "foo");
  RefreshingCredentialsWrapper w;
  auto refresh_fn = [&]() {
    RefreshingCredentialsWrapper::TemporaryToken token;
    token.token = auth_token;
    token.expiration_time =
        std::chrono::system_clock::now() - std::chrono::seconds(1000);
    return token;
  };
  auto token =
      w.AuthorizationHeader(std::chrono::system_clock::now(), refresh_fn);
  EXPECT_TRUE(w.IsExpired(std::chrono::system_clock::now()));
}

TEST(RefreshingCredentialsWrapper, IsNotExpired) {
  std::pair<std::string, std::string> const auth_token =
      std::make_pair("Authorization", "foo");
  RefreshingCredentialsWrapper w;
  auto refresh_fn = [&]() {
    RefreshingCredentialsWrapper::TemporaryToken token;
    token.token = auth_token;
    token.expiration_time =
        std::chrono::system_clock::now() + std::chrono::minutes(60);
    return token;
  };
  auto token =
      w.AuthorizationHeader(std::chrono::system_clock::now(), refresh_fn);

  EXPECT_FALSE(w.IsExpired(std::chrono::system_clock::now()));
}

TEST(RefreshingCredentialsWrapper, IsValid) {
  std::pair<std::string, std::string> const auth_token =
      std::make_pair("Authorization", "foo");
  RefreshingCredentialsWrapper w;
  auto refresh_fn = [&]() {
    RefreshingCredentialsWrapper::TemporaryToken token;
    token.token = auth_token;
    token.expiration_time =
        std::chrono::system_clock::now() + std::chrono::minutes(60);
    return token;
  };
  auto token =
      w.AuthorizationHeader(std::chrono::system_clock::now(), refresh_fn);
  EXPECT_TRUE(w.IsValid(std::chrono::system_clock::now()));
}

TEST(RefreshingCredentialsWrapper, IsNotValid) {
  std::pair<std::string, std::string> const auth_token =
      std::make_pair("Authorization", "foo");
  RefreshingCredentialsWrapper w;
  EXPECT_FALSE(w.IsValid(std::chrono::system_clock::now()));
}

TEST(RefreshingCredentialsWrapper, RefreshTokenSuccess) {
  std::pair<std::string, std::string> const auth_token =
      std::make_pair("Authorization", "foo");
  RefreshingCredentialsWrapper w;
  auto refresh_fn = [&]() {
    RefreshingCredentialsWrapper::TemporaryToken token;
    token.token = auth_token;
    token.expiration_time =
        std::chrono::system_clock::now() + std::chrono::seconds(60);
    return token;
  };
  auto token =
      w.AuthorizationHeader(std::chrono::system_clock::now(), refresh_fn);
  ASSERT_THAT(token, IsOk());
  EXPECT_THAT(*token, Eq(auth_token));
}

TEST(RefreshingCredentialsWrapper, RefreshTokenFailure) {
  RefreshingCredentialsWrapper w;
  auto refresh_fn = [&]() {
    return Status(StatusCode::kInvalidArgument, {}, {});
  };
  auto token =
      w.AuthorizationHeader(std::chrono::system_clock::now(), refresh_fn);
  EXPECT_THAT(token, Not(IsOk()));
  EXPECT_THAT(token.status().code(), Eq(StatusCode::kInvalidArgument));
}

TEST(RefreshingCredentialsWrapper, RefreshTokenFailureValidToken) {
  std::pair<std::string, std::string> const auth_token =
      std::make_pair("Authorization", "foo");
  RefreshingCredentialsWrapper w;
  auto refresh_fn = [&]() {
    RefreshingCredentialsWrapper::TemporaryToken token;
    token.token = auth_token;
    token.expiration_time =
        std::chrono::system_clock::now() + std::chrono::seconds(60);
    return token;
  };
  auto token =
      w.AuthorizationHeader(std::chrono::system_clock::now(), refresh_fn);

  auto failing_refresh_fn = [&]() {
    return Status(StatusCode::kInvalidArgument, {}, {});
  };
  token = w.AuthorizationHeader(std::chrono::system_clock::now(),
                                failing_refresh_fn);
  ASSERT_THAT(token, IsOk());
  EXPECT_THAT(*token, Eq(auth_token));
}

TEST(RefreshingCredentialsWrapper, RefreshTokenFailureInvalidToken) {
  std::pair<std::string, std::string> const auth_token =
      std::make_pair("Authorization", "foo");
  RefreshingCredentialsWrapper w;
  auto refresh_fn = [&]() {
    RefreshingCredentialsWrapper::TemporaryToken token;
    token.token = auth_token;
    token.expiration_time =
        std::chrono::system_clock::now() - std::chrono::seconds(3600);
    return token;
  };
  auto token =
      w.AuthorizationHeader(std::chrono::system_clock::now(), refresh_fn);

  auto failing_refresh_fn = [&]() {
    return Status(StatusCode::kInvalidArgument, {}, {});
  };
  token = w.AuthorizationHeader(std::chrono::system_clock::now(),
                                failing_refresh_fn);
  EXPECT_THAT(token, Not(IsOk()));
  EXPECT_THAT(token.status().code(), Eq(StatusCode::kInvalidArgument));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

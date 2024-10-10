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

#include "google/cloud/internal/oauth2_credentials.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::UnavailableError;
using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsOkAndHolds;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Return;

class MockCredentials : public Credentials {
 public:
  MOCK_METHOD(StatusOr<AccessToken>, GetToken,
              (std::chrono::system_clock::time_point), (override));
};

TEST(Credentials, AuthorizationHeaderSuccess) {
  MockCredentials mock;
  auto const now = std::chrono::system_clock::now();
  auto const expiration = now + std::chrono::seconds(3600);
  EXPECT_CALL(mock, GetToken(now))
      .WillOnce(Return(AccessToken{"test-token", expiration}));
  auto actual = mock.AuthenticationHeader(now);
  EXPECT_THAT(actual, IsOkAndHolds(Pair("Authorization", "Bearer test-token")));
}

TEST(Credentials, AuthenticationHeaderJoinedSuccess) {
  MockCredentials mock;
  auto const now = std::chrono::system_clock::now();
  auto const expiration = now + std::chrono::seconds(3600);
  EXPECT_CALL(mock, GetToken(now))
      .WillOnce(Return(AccessToken{"test-token", expiration}));
  auto actual = AuthenticationHeaderJoined(mock, now);
  EXPECT_THAT(actual, IsOkAndHolds("Authorization: Bearer test-token"));
}

TEST(Credentials, AuthenticationHeaderJoinedEmpty) {
  MockCredentials mock;
  auto const now = std::chrono::system_clock::now();
  auto const expiration = now + std::chrono::seconds(3600);
  EXPECT_CALL(mock, GetToken(now))
      .WillOnce(Return(AccessToken{"", expiration}));
  auto actual = AuthenticationHeaderJoined(mock, now);
  EXPECT_THAT(actual, IsOkAndHolds(IsEmpty()));
}

TEST(Credentials, AuthenticationHeaderError) {
  MockCredentials mock;
  EXPECT_CALL(mock, GetToken).WillOnce(Return(UnavailableError("try-again")));
  auto actual = mock.AuthenticationHeader(std::chrono::system_clock::now());
  EXPECT_EQ(actual.status(), UnavailableError("try-again"));
}

TEST(Credentials, AuthenticationHeaderJoinedError) {
  MockCredentials mock;
  EXPECT_CALL(mock, GetToken).WillOnce(Return(UnavailableError("try-again")));
  auto actual = AuthenticationHeaderJoined(mock);
  EXPECT_EQ(actual.status(), UnavailableError("try-again"));
}

TEST(Credentials, ProjectId) {
  MockCredentials mock;
  EXPECT_THAT(mock.project_id(), Not(IsOk()));
  EXPECT_THAT(mock.project_id({}), Not(IsOk()));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

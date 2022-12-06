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
using ::testing::IsEmpty;
using ::testing::Return;

class MockCredentials : public Credentials {
 public:
  MOCK_METHOD((StatusOr<std::pair<std::string, std::string>>),
              AuthorizationHeader, (), (override));
};

TEST(Credentials, AuthorizationHeaderSuccess) {
  MockCredentials mock;
  auto const expected = std::make_pair(std::string("Authorization"),
                                       std::string("Bearer test-token"));
  EXPECT_CALL(mock, AuthorizationHeader).WillOnce(Return(expected));
  auto actual = AuthorizationHeader(mock);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, expected);
}

TEST(Credentials, AuthorizationHeaderJoinedSuccess) {
  MockCredentials mock;
  auto const expected = std::make_pair(std::string("Authorization"),
                                       std::string("Bearer test-token"));
  EXPECT_CALL(mock, AuthorizationHeader).WillOnce(Return(expected));
  auto actual = AuthorizationHeaderJoined(mock);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(*actual, "Authorization: Bearer test-token");
}

TEST(Credentials, AuthorizationHeaderJoinedEmpty) {
  MockCredentials mock;
  auto const expected =
      std::make_pair(std::string("Authorization"), std::string{});
  EXPECT_CALL(mock, AuthorizationHeader).WillOnce(Return(expected));
  auto actual = AuthorizationHeaderJoined(mock);
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, IsEmpty());
}

TEST(Credentials, AuthorizationHeaderError) {
  MockCredentials mock;
  EXPECT_CALL(mock, AuthorizationHeader)
      .WillOnce(Return(UnavailableError("try-again")));
  auto actual = AuthorizationHeader(mock);
  EXPECT_EQ(actual.status(), UnavailableError("try-again"));
}

TEST(Credentials, AuthorizationHeaderJoinedError) {
  MockCredentials mock;
  EXPECT_CALL(mock, AuthorizationHeader)
      .WillOnce(Return(UnavailableError("try-again")));
  auto actual = AuthorizationHeaderJoined(mock);
  EXPECT_EQ(actual.status(), UnavailableError("try-again"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

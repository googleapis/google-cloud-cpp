// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/access_token_credentials.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using google::cloud::internal::AccessToken;
using ::testing::Return;

TEST(AccessTokenCredentials, Simple) {
  ::testing::MockFunction<AccessToken()> mock_source;
  auto const expiration =
      std::chrono::system_clock::now() - std::chrono::minutes(10);
  EXPECT_CALL(mock_source, Call)
      .WillOnce(Return(AccessToken{"token1", expiration}))
      .WillOnce(Return(AccessToken{"token2", expiration}))
      .WillOnce(Return(AccessToken{"token3", expiration}));

  AccessTokenCredentials tested(mock_source.AsStdFunction());
  EXPECT_EQ("Authorization: Bearer token1",
            tested.AuthorizationHeader().value());
  EXPECT_EQ("Authorization: Bearer token2",
            tested.AuthorizationHeader().value());
  EXPECT_EQ("Authorization: Bearer token3",
            tested.AuthorizationHeader().value());
}

TEST(AccessTokenCredentials, NotExpired) {
  ::testing::MockFunction<AccessToken()> mock_source;
  auto const expiration =
      std::chrono::system_clock::now() + std::chrono::minutes(10);
  EXPECT_CALL(mock_source, Call)
      .WillOnce(Return(AccessToken{"token1", expiration}));

  AccessTokenCredentials tested(mock_source.AsStdFunction());
  EXPECT_EQ("Authorization: Bearer token1",
            tested.AuthorizationHeader().value());
  EXPECT_EQ("Authorization: Bearer token1",
            tested.AuthorizationHeader().value());
  EXPECT_EQ("Authorization: Bearer token1",
            tested.AuthorizationHeader().value());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google
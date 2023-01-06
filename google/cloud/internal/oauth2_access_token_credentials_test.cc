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

#include "google/cloud/internal/oauth2_access_token_credentials.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::AccessToken;

TEST(AccessTokenCredentials, Simple) {
  auto const now = std::chrono::system_clock::now();
  auto const expiration = now - std::chrono::minutes(10);
  auto const expected = AccessToken{"token1", expiration};
  AccessTokenCredentials tested(expected);
  EXPECT_EQ(expected, tested.GetToken(now).value());
  EXPECT_EQ(expected, tested.GetToken(now).value());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

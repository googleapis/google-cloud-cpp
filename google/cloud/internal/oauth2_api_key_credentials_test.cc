// Copyright 2024 Google LLC
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

#include "google/cloud/internal/oauth2_api_key_credentials.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>

namespace google {
namespace cloud {
namespace oauth2_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOkAndHolds;
using ::testing::IsEmpty;
using ::testing::Pair;

TEST(ApiKeyCredentials, EmptyToken) {
  ApiKeyCredentials creds("api-key");
  auto const now = std::chrono::system_clock::now();
  auto const token = creds.GetToken(now);
  ASSERT_STATUS_OK(token);
  EXPECT_THAT(token->token, IsEmpty());
}

TEST(ApiKeyCredentials, SetsXGoogApiKeyHeader) {
  ApiKeyCredentials creds("api-key");
  auto const now = std::chrono::system_clock::now();
  EXPECT_THAT(creds.AuthenticationHeader(now),
              IsOkAndHolds(Pair("x-goog-api-key", "api-key")));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace oauth2_internal
}  // namespace cloud
}  // namespace google

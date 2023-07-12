// Copyright 2023 Google LLC
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

#include "google/cloud/oauth2/access_token_generator.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace oauth2 {
namespace {

using ::google::cloud::testing_util::IsOkAndHolds;

TEST(AccessTokenGeneratorTest, Basic) {
  auto const expiration =
      std::chrono::system_clock::now() + std::chrono::minutes(15);
  auto credentials = MakeAccessTokenCredentials("test-token", expiration);
  auto generator = MakeAccessTokenGenerator(*credentials);
  auto token = generator->GetToken();
  EXPECT_THAT(token, IsOkAndHolds(AccessToken{"test-token", expiration}));
}

}  // namespace
}  // namespace oauth2
}  // namespace cloud
}  // namespace google

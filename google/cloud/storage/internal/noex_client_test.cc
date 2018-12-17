// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/noex_client.h"
#include "google/cloud/storage/internal/curl_client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
/// @test Verify the constructor creates the right set of RawClient decorations.
TEST(NoexClientTest, DefaultDecorators) {
  // Create a client, use the anonymous credentials because on the CI
  // environment there may not be other credentials configured.
  noex::Client tested(oauth2::CreateAnonymousCredentials());

  EXPECT_TRUE(tested.raw_client() != nullptr);
  auto retry = dynamic_cast<internal::RetryClient*>(tested.raw_client().get());
  ASSERT_TRUE(retry != nullptr);

  auto logging = dynamic_cast<internal::LoggingClient*>(retry->client().get());
  ASSERT_TRUE(logging != nullptr);

  auto curl = dynamic_cast<internal::CurlClient*>(logging->client().get());
  ASSERT_TRUE(curl != nullptr);
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/storage/oauth2/anonymous_credentials.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace oauth2 {
namespace {

class AnonymousCredentialsTest : public ::testing::Test {};

/// @test Verify `AnonymousCredentials` works as expected.
TEST_F(AnonymousCredentialsTest, AuthorizationHeaderReturnsEmptyString) {
  AnonymousCredentials credentials;
  auto header = credentials.AuthorizationHeader();
  ASSERT_STATUS_OK(header);
  EXPECT_EQ("", header.value());
}

}  // namespace
}  // namespace oauth2
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

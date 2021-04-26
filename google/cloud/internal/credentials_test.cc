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

#include "google/cloud/internal/credentials.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

struct Visitor : public CredentialsVisitor {
  std::string name;
  AccessToken access_token;

  void visit(GoogleDefaultCredentialsConfig&) override {
    name = "GoogleDefaultCredentialsConfig";
  }
  void visit(AccessTokenConfig& cfg) override {
    name = "AccessTokenConfig";
    access_token = cfg.access_token();
  }
};

TEST(Credentials, GoogleDefaultCredentials) {
  Visitor visitor;

  auto credentials = MakeGoogleDefaultCredentials();
  CredentialsVisitor::dispatch(*credentials, visitor);
  EXPECT_EQ("GoogleDefaultCredentialsConfig", visitor.name);
}

TEST(Credentials, AccessTokenCredentials) {
  Visitor visitor;

  auto const expiration = std::chrono::system_clock::now();
  auto credentials = MakeAccessTokenCredentials("test-token", expiration);
  CredentialsVisitor::dispatch(*credentials, visitor);
  ASSERT_EQ("AccessTokenConfig", visitor.name);
  EXPECT_EQ("test-token", visitor.access_token.token);
  EXPECT_EQ(expiration, visitor.access_token.expiration);
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

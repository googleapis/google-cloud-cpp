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

using ::testing::ElementsAre;
using ::testing::IsEmpty;

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

TEST(Credentials, ImpersonateServiceAccountCredentialsDefault) {
  auto credentials = MakeImpersonateServiceAccountCredentials(
      MakeGoogleDefaultCredentials(), "invalid-test-only@invalid.address");
  EXPECT_EQ("invalid-test-only@invalid.address",
            credentials->target_service_account());
  EXPECT_EQ(std::chrono::hours(1), credentials->lifetime());
  EXPECT_THAT(credentials->scopes(),
              ElementsAre("https://www.googleapis.com/auth/cloud-platform"));
  EXPECT_THAT(credentials->delegates(), IsEmpty());
}

TEST(Credentials, ImpersonateServiceAccountCredentialsDefaultWithOptions) {
  auto credentials = MakeImpersonateServiceAccountCredentials(
      MakeGoogleDefaultCredentials(), "invalid-test-only@invalid.address",
      Options{}
          .set<LifetimeOption>(std::chrono::minutes(15))
          .set<ScopesOption>({"scope1", "scope2"})
          .set<DelegatesOption>({"delegate1", "delegate2"}));
  EXPECT_EQ("invalid-test-only@invalid.address",
            credentials->target_service_account());
  EXPECT_EQ(std::chrono::minutes(15), credentials->lifetime());
  EXPECT_THAT(credentials->scopes(), ElementsAre("scope1", "scope2"));
  EXPECT_THAT(credentials->delegates(), ElementsAre("delegate1", "delegate2"));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

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
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
namespace {

using ::testing::Return;

struct Visitor : public CredentialsVisitor {
  std::string name;
  AccessTokenSource source;

  void visit(GoogleDefaultCredentialsConfig&) override {
    name = "GoogleDefaultCredentialsConfig";
  }
  void visit(DynamicAccessTokenConfig& cfg) override {
    name = "DynamicAccessTokenConfig";
    source = cfg.source();
  }
};

TEST(Credentials, GoogleDefaultCredentials) {
  Visitor visitor;

  auto credentials = GoogleDefaultCredentials();
  CredentialsVisitor::dispatch(*credentials, visitor);
  EXPECT_EQ("GoogleDefaultCredentialsConfig", visitor.name);
}

TEST(Credentials, AccessTokenCredentials) {
  Visitor visitor;

  auto const expiration = std::chrono::system_clock::now();
  auto credentials = AccessTokenCredentials("test-token", expiration);
  CredentialsVisitor::dispatch(*credentials, visitor);
  ASSERT_EQ("DynamicAccessTokenConfig", visitor.name);
  auto const access_token = visitor.source();
  EXPECT_EQ("test-token", access_token.token);
  EXPECT_EQ(expiration, access_token.expiration);
}

TEST(Credentials, DynamicAccessTokenCredentials) {
  ::testing::MockFunction<AccessToken()> mock;
  auto const e1 = std::chrono::system_clock::now() + std::chrono::hours(1);
  auto const e2 = std::chrono::system_clock::now() + std::chrono::hours(2);
  EXPECT_CALL(mock, Call)
      .WillOnce(Return(AccessToken{"t1", e1}))
      .WillOnce(Return(AccessToken{"t2", e2}));

  Visitor visitor;
  auto credentials = DynamicAccessTokenCredentials(mock.AsStdFunction());
  CredentialsVisitor::dispatch(*credentials, visitor);
  EXPECT_EQ("DynamicAccessTokenConfig", visitor.name);
  auto t1 = visitor.source();
  EXPECT_EQ("t1", t1.token);
  EXPECT_EQ(e1, t1.expiration);
  auto t2 = visitor.source();
  EXPECT_EQ("t2", t2.token);
  EXPECT_EQ(e2, t2.expiration);
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

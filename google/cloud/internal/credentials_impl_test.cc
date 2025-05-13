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

#include "google/cloud/internal/credentials_impl.h"
#include "google/cloud/internal/populate_common_options.h"
#include "google/cloud/testing_util/credentials.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::TestCredentialsVisitor;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsNull;

TEST(Credentials, ErrorCredentials) {
  TestCredentialsVisitor visitor;

  auto credentials = MakeErrorCredentials({});
  CredentialsVisitor::dispatch(*credentials, visitor);
  EXPECT_EQ("ErrorCredentialsConfig", visitor.name);
}

TEST(Credentials, InsecureCredentials) {
  TestCredentialsVisitor visitor;

  auto credentials = MakeInsecureCredentials();
  CredentialsVisitor::dispatch(*credentials, visitor);
  EXPECT_EQ("InsecureCredentialsConfig", visitor.name);
}

TEST(Credentials, GoogleDefaultCredentials) {
  TestCredentialsVisitor visitor;

  auto credentials = MakeGoogleDefaultCredentials();
  CredentialsVisitor::dispatch(*credentials, visitor);
  EXPECT_EQ("GoogleDefaultCredentialsConfig", visitor.name);
}

TEST(Credentials, AccessTokenCredentials) {
  TestCredentialsVisitor visitor;

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
  TestCredentialsVisitor visitor;
  CredentialsVisitor::dispatch(*credentials, visitor);
  ASSERT_THAT(visitor.impersonate, Not(IsNull()));
  EXPECT_EQ("invalid-test-only@invalid.address",
            visitor.impersonate->target_service_account());
  EXPECT_EQ(std::chrono::hours(1), visitor.impersonate->lifetime());
  EXPECT_THAT(visitor.impersonate->scopes(),
              ElementsAre("https://www.googleapis.com/auth/cloud-platform"));
  EXPECT_THAT(visitor.impersonate->delegates(), IsEmpty());
}

TEST(Credentials, ImpersonateServiceAccountCredentialsDefaultWithOptions) {
  auto credentials = MakeImpersonateServiceAccountCredentials(
      MakeGoogleDefaultCredentials(), "invalid-test-only@invalid.address",
      Options{}
          .set<AccessTokenLifetimeOption>(std::chrono::minutes(15))
          .set<ScopesOption>({"scope1", "scope2"})
          .set<DelegatesOption>({"delegate1", "delegate2"}));
  TestCredentialsVisitor visitor;
  CredentialsVisitor::dispatch(*credentials, visitor);
  ASSERT_THAT(visitor.impersonate, Not(IsNull()));
  EXPECT_EQ("invalid-test-only@invalid.address",
            visitor.impersonate->target_service_account());
  EXPECT_EQ(std::chrono::minutes(15), visitor.impersonate->lifetime());
  EXPECT_THAT(visitor.impersonate->scopes(), ElementsAre("scope1", "scope2"));
  EXPECT_THAT(visitor.impersonate->delegates(),
              ElementsAre("delegate1", "delegate2"));
}

TEST(Credentials, ServiceAccount) {
  auto credentials = MakeServiceAccountCredentials("test-only-invalid");
  TestCredentialsVisitor visitor;
  CredentialsVisitor::dispatch(*credentials, visitor);
  ASSERT_EQ("ServiceAccountConfig", visitor.name);
  EXPECT_EQ("test-only-invalid", visitor.json_object);
}

TEST(Credentials, ExternalAccount) {
  auto credentials = MakeExternalAccountCredentials(
      "test-only-invalid", Options{}.set<ScopesOption>({"scope1", "scope2"}));
  TestCredentialsVisitor visitor;
  CredentialsVisitor::dispatch(*credentials, visitor);
  ASSERT_EQ("ExternalAccountConfig", visitor.name);
  EXPECT_EQ("test-only-invalid", visitor.json_object);
  EXPECT_THAT(visitor.options.get<ScopesOption>(),
              ElementsAre("scope1", "scope2"));
}

TEST(Credentials, ApiKeyCredentials) {
  TestCredentialsVisitor visitor;

  auto credentials = MakeApiKeyCredentials("api-key");
  CredentialsVisitor::dispatch(*credentials, visitor);
  EXPECT_EQ("ApiKeyConfig", visitor.name);
  EXPECT_EQ("api-key", visitor.api_key);
}

TEST(PopulateAuthOptions, EmptyOptions) {
  auto result_options = PopulateAuthOptions(Options{});

  EXPECT_THAT(result_options.get<ScopesOption>(),
              ElementsAre("https://www.googleapis.com/auth/cloud-platform"));
  EXPECT_THAT(result_options.get<AccessTokenLifetimeOption>(),
              Eq(std::chrono::hours(1)));
  EXPECT_THAT(result_options.get<LoggingComponentsOption>(),
              Eq(DefaultTracingComponents()));
}

TEST(PopulateAuthOptions, ExistingNonIntersectingOptions) {
  auto options = Options{}.set<EndpointOption>("my-endpoint");
  auto result_options = PopulateAuthOptions(options);

  EXPECT_THAT(result_options.get<EndpointOption>(), Eq("my-endpoint"));
  EXPECT_THAT(result_options.get<ScopesOption>(),
              ElementsAre("https://www.googleapis.com/auth/cloud-platform"));
  EXPECT_THAT(result_options.get<AccessTokenLifetimeOption>(),
              Eq(std::chrono::hours(1)));
  EXPECT_THAT(result_options.get<LoggingComponentsOption>(),
              Eq(DefaultTracingComponents()));
}

TEST(PopulateAuthOptions, ExistingIntersectingOptions) {
  auto options = Options{}
                     .set<ScopesOption>({"my-scope"})
                     .set<LoggingComponentsOption>({"my-logging-component"});
  auto result_options = PopulateAuthOptions(options);
  EXPECT_THAT(result_options.get<ScopesOption>(), ElementsAre("my-scope"));
  EXPECT_THAT(result_options.get<AccessTokenLifetimeOption>(),
              Eq(std::chrono::hours(1)));
  EXPECT_THAT(result_options.get<LoggingComponentsOption>(),
              ElementsAre("my-logging-component"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

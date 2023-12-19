// Copyright 2022 Google LLC
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

#include "google/cloud/internal/populate_common_options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/user_agent_prefix.h"
#include "google/cloud/opentelemetry_options.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/universe_domain_options.h"
#include "absl/types/optional.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::ScopedEnvironment;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;

TEST(PopulateCommonOptions, Simple) {
  // Unset all the relevant environment variables.
  ScopedEnvironment user("GOOGLE_CLOUD_CPP_USER_PROJECT", absl::nullopt);
  ScopedEnvironment tracing("GOOGLE_CLOUD_CPP_ENABLE_TRACING", absl::nullopt);
  auto actual =
      PopulateCommonOptions(Options{}, {}, {}, {}, "default.googleapis.com");
  EXPECT_TRUE(actual.has<EndpointOption>());
  EXPECT_THAT(actual.get<EndpointOption>(), Eq("default.googleapis.com."));
  EXPECT_TRUE(actual.has<AuthorityOption>());
  EXPECT_THAT(actual.get<AuthorityOption>(), Eq("default.googleapis.com"));
  EXPECT_FALSE(actual.has<UserProjectOption>());
  EXPECT_TRUE(actual.has<TracingComponentsOption>());
  EXPECT_THAT(actual.get<TracingComponentsOption>(), IsEmpty());
  EXPECT_TRUE(actual.has<UserAgentProductsOption>());
  EXPECT_THAT(actual.get<UserAgentProductsOption>(),
              Contains(UserAgentPrefix()));
}

TEST(PopulateCommonOptions, EmptyEndpointOption) {
  auto actual = PopulateCommonOptions(Options{}.set<EndpointOption>(""), {}, {},
                                      {}, "default.googleapis.com");
  EXPECT_TRUE(actual.has<EndpointOption>());
  EXPECT_THAT(actual.get<EndpointOption>(), Eq(""));
}

TEST(PopulateCommonOptions, EmptyEndpointEnvVar) {
  ScopedEnvironment endpoint("GOOGLE_CLOUD_CPP_SERVICE_ENDPOINT", "");
  auto actual =
      PopulateCommonOptions(Options{}, "GOOGLE_CLOUD_CPP_SERVICE_ENDPOINT", {},
                            {}, "default.googleapis.com");
  EXPECT_TRUE(actual.has<EndpointOption>());
  EXPECT_THAT(actual.get<EndpointOption>(), Eq("default.googleapis.com."));
}

TEST(PopulateCommonOptions, EmptyEmulatorEnvVar) {
  ScopedEnvironment endpoint("GOOGLE_CLOUD_CPP_EMULATOR_ENDPOINT", "");
  auto actual =
      PopulateCommonOptions(Options{}, {}, "GOOGLE_CLOUD_CPP_EMULATOR_ENDPOINT",
                            {}, "default.googleapis.com");
  EXPECT_TRUE(actual.has<EndpointOption>());
  EXPECT_THAT(actual.get<EndpointOption>(), Eq("default.googleapis.com."));
}

// TODO(#13191): Simplify into multiple tests.
TEST(PopulateCommonOptions, EndpointAuthority) {
  Options optionses[] = {
      Options{},
      Options{}.set<EndpointOption>("endpoint_option"),
      Options{}.set<AuthorityOption>("authority_option"),
      Options{}
          .set<EndpointOption>("endpoint_option")
          .set<AuthorityOption>("authority_option"),
  };
  absl::optional<std::string> endpoints[] = {absl::nullopt, "", "endpoint"};
  absl::optional<std::string> emulators[] = {absl::nullopt, "", "emulator"};
  absl::optional<std::string> authorities[] = {absl::nullopt, "", "authority"};
  for (auto const& options : optionses) {
    for (auto const& endpoint_env : endpoints) {
      for (auto const& emulator_env : emulators) {
        for (auto const& authority_env : authorities) {
          ScopedEnvironment endpoint("SERVICE_ENDPOINT", endpoint_env);
          ScopedEnvironment emulator("SERVICE_EMULATOR", emulator_env);
          ScopedEnvironment authority("SERVICE_AUTHORITY", authority_env);

          auto actual = PopulateCommonOptions(
              options, "SERVICE_ENDPOINT", "SERVICE_EMULATOR",
              "SERVICE_AUTHORITY", "default.googleapis.com");

          ASSERT_TRUE(actual.has<EndpointOption>());
          auto const& actual_endpoint = actual.get<EndpointOption>();
          if (emulator_env.has_value() && !emulator_env->empty()) {
            EXPECT_THAT(actual_endpoint, Eq(*emulator_env));
          } else if (endpoint_env.has_value() && !endpoint_env->empty()) {
            EXPECT_THAT(actual_endpoint, Eq(*endpoint_env));
          } else if (options.has<EndpointOption>()) {
            EXPECT_THAT(actual_endpoint, Eq(options.get<EndpointOption>()));
          } else {
            EXPECT_THAT(actual_endpoint, Eq("default.googleapis.com."));
          }

          ASSERT_TRUE(actual.has<AuthorityOption>());
          auto const& actual_authority = actual.get<AuthorityOption>();
          if (authority_env.has_value() && !authority_env->empty()) {
            EXPECT_THAT(actual_authority, Eq(*authority_env));
          } else if (options.has<AuthorityOption>()) {
            EXPECT_THAT(actual_authority, Eq(options.get<AuthorityOption>()));
          } else {
            EXPECT_THAT(actual_authority, Eq("default.googleapis.com"));
          }
        }
      }
    }
  }
}

TEST(PopulateCommonOptions, UniverseDomain) {
  auto actual =
      PopulateCommonOptions(Options{}.set<UniverseDomainOption>("my-ud.net"),
                            {}, {}, {}, "default.googleapis.com");
  EXPECT_EQ(actual.get<EndpointOption>(), "default.my-ud.net");
}

TEST(PopulateCommonOptions, EndpointOptionOverridesUniverseDomain) {
  auto actual = PopulateCommonOptions(
      Options{}
          .set<UniverseDomainOption>("ignored-ud.net")
          .set<EndpointOption>("custom-endpoint.googleapis.com"),
      {}, {}, {}, "default.googleapis.com");
  EXPECT_EQ(actual.get<EndpointOption>(), "custom-endpoint.googleapis.com");
}

TEST(PopulateCommonOptions, EnvVarsOverridesUniverseDomain) {
  ScopedEnvironment endpoint("SERVICE_ENDPOINT", "endpoint-env.googleapis.com");
  ScopedEnvironment emulator("SERVICE_EMULATOR", "emulator-env.googleapis.com");

  auto actual = PopulateCommonOptions(
      Options{}.set<UniverseDomainOption>("ignored-ud.net"), "SERVICE_ENDPOINT",
      {}, {}, "default.googleapis.com");
  EXPECT_EQ(actual.get<EndpointOption>(), "endpoint-env.googleapis.com");

  actual = PopulateCommonOptions(
      Options{}.set<UniverseDomainOption>("ignored-ud.net"), {},
      "SERVICE_EMULATOR", {}, "default.googleapis.com");
  EXPECT_EQ(actual.get<EndpointOption>(), "emulator-env.googleapis.com");
}

TEST(PopulateCommonOptions, UserProject) {
  Options optionses[] = {
      Options{},
      Options{}.set<UserProjectOption>("project_option"),
  };
  absl::optional<std::string> projects[] = {absl::nullopt, "", "project"};
  for (auto const& options : optionses) {
    for (auto const& project_env : projects) {
      ScopedEnvironment projects("GOOGLE_CLOUD_CPP_USER_PROJECT", project_env);
      auto actual =
          PopulateCommonOptions(options, {}, {}, {}, "default.googleapis.com");
      if (project_env.has_value() && !project_env->empty()) {
        EXPECT_THAT(actual.get<UserProjectOption>(), Eq(*project_env));
      } else if (options.has<UserProjectOption>()) {
        EXPECT_THAT(actual.get<UserProjectOption>(),
                    Eq(options.get<UserProjectOption>()));
      } else {
        EXPECT_FALSE(actual.has<UserProjectOption>());
      }
    }
  }
}

TEST(PopulateCommonOptions, OpenTelemetryTracing) {
  struct TestCase {
    absl::optional<std::string> env;
    bool value;
  };
  std::vector<TestCase> tests = {
      {absl::nullopt, false},
      {"", false},
      {"ON", true},
  };
  auto const input = Options{}.set<OpenTelemetryTracingOption>(false);
  for (auto const& test : tests) {
    ScopedEnvironment env("GOOGLE_CLOUD_CPP_OPENTELEMETRY_TRACING", test.env);
    auto options = PopulateCommonOptions(input, {}, {}, {}, {});
    EXPECT_EQ(options.get<OpenTelemetryTracingOption>(), test.value);
  }
}

TEST(DefaultTracingComponents, NoEnvironment) {
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_ENABLE_TRACING", absl::nullopt);
  auto const actual = DefaultTracingComponents();
  EXPECT_THAT(actual, ElementsAre());
}

TEST(DefaultTracingComponents, WithValue) {
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_ENABLE_TRACING", "a,b,c");
  auto const actual = DefaultTracingComponents();
  EXPECT_THAT(actual, ElementsAre("a", "b", "c"));
}

TEST(MakeAuthOptions, WithoutTracing) {
  auto options = Options{}.set<EndpointOption>("endpoint_option");
  auto auth_options = MakeAuthOptions(options);
  EXPECT_FALSE(auth_options.has<EndpointOption>());
  EXPECT_FALSE(auth_options.get<OpenTelemetryTracingOption>());
}

TEST(MakeAuthOptions, WithTracing) {
  auto options = Options{}
                     .set<EndpointOption>("endpoint_option")
                     .set<OpenTelemetryTracingOption>(true);
  auto auth_options = MakeAuthOptions(options);
  EXPECT_FALSE(auth_options.has<EndpointOption>());
  EXPECT_TRUE(auth_options.get<OpenTelemetryTracingOption>());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

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
#include "google/cloud/testing_util/scoped_environment.h"
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
  auto actual = PopulateCommonOptions(Options{}, {}, {}, {}, "default");
  EXPECT_TRUE(actual.has<EndpointOption>());
  EXPECT_THAT(actual.get<EndpointOption>(), Eq("default"));
  EXPECT_TRUE(actual.has<AuthorityOption>());
  EXPECT_THAT(actual.get<AuthorityOption>(), Eq("default"));
  EXPECT_FALSE(actual.has<UserProjectOption>());
  EXPECT_TRUE(actual.has<TracingComponentsOption>());
  EXPECT_THAT(actual.get<TracingComponentsOption>(), IsEmpty());
  EXPECT_TRUE(actual.has<UserAgentProductsOption>());
  EXPECT_THAT(actual.get<UserAgentProductsOption>(),
              Contains(UserAgentPrefix()));
}

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

          auto actual = PopulateCommonOptions(options, "SERVICE_ENDPOINT",
                                              "SERVICE_EMULATOR",
                                              "SERVICE_AUTHORITY", "default");

          ASSERT_TRUE(actual.has<EndpointOption>());
          auto const& actual_endpoint = actual.get<EndpointOption>();
          if (emulator_env.has_value() && !emulator_env->empty()) {
            EXPECT_THAT(actual_endpoint, Eq(*emulator_env));
          } else if (endpoint_env.has_value() && !endpoint_env->empty()) {
            EXPECT_THAT(actual_endpoint, Eq(*endpoint_env));
          } else if (options.has<EndpointOption>()) {
            EXPECT_THAT(actual_endpoint, Eq(options.get<EndpointOption>()));
          } else {
            EXPECT_THAT(actual_endpoint, Eq("default"));
          }

          ASSERT_TRUE(actual.has<AuthorityOption>());
          auto const& actual_authority = actual.get<AuthorityOption>();
          if (authority_env.has_value() && !authority_env->empty()) {
            EXPECT_THAT(actual_authority, Eq(*authority_env));
          } else if (options.has<AuthorityOption>()) {
            EXPECT_THAT(actual_authority, Eq(options.get<AuthorityOption>()));
          } else {
            EXPECT_THAT(actual_authority, Eq("default"));
          }
        }
      }
    }
  }
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
      auto actual = PopulateCommonOptions(options, {}, {}, {}, "default");
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

TEST(PopulateCommonOptions, DefaultTracingComponentsNoEnvironment) {
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_ENABLE_TRACING", absl::nullopt);
  auto const actual = DefaultTracingComponents();
  EXPECT_THAT(actual, ElementsAre());
}

TEST(PopulateCommonOptions, DefaultTracingComponentsWithValue) {
  ScopedEnvironment env("GOOGLE_CLOUD_CPP_ENABLE_TRACING", "a,b,c");
  auto const actual = DefaultTracingComponents();
  EXPECT_THAT(actual, ElementsAre("a", "b", "c"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

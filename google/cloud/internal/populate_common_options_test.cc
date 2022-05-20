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
using ::testing::IsEmpty;

TEST(PopulateCommonOptions, Simple) {
  // Unset all the relevant environment variables
  ScopedEnvironment user("GOOGLE_CLOUD_CPP_USER_PROJECT", absl::nullopt);
  ScopedEnvironment tracing("GOOGLE_CLOUD_CPP_ENABLE_TRACING", absl::nullopt);
  auto actual =
      PopulateCommonOptions(Options{}, {}, {}, "default.googleapis.com");
  EXPECT_TRUE(actual.has<EndpointOption>());
  EXPECT_TRUE(actual.has<AuthorityOption>());
  EXPECT_TRUE(actual.has<UserAgentProductsOption>());
  EXPECT_THAT(actual.get<UserAgentProductsOption>(),
              Contains(UserAgentPrefix()));
  EXPECT_TRUE(actual.has<TracingComponentsOption>());
  EXPECT_THAT(actual.get<TracingComponentsOption>(), IsEmpty());
  EXPECT_FALSE(actual.has<UserProjectOption>());
}

TEST(PopulateCommonOptions, Endpoint) {
  auto const empty = Options{};
  auto const with_ep = Options{}.set<EndpointOption>("with-ep");
  struct TestCase {
    std::string name;
    Options initial;
    absl::optional<std::string> default_env;
    absl::optional<std::string> emulator_env;
    std::string expected;
  } test_cases[] = {
      {"empty-0", empty, absl::nullopt, absl::nullopt, "default"},
      {"empty-1", empty, absl::nullopt, "", "default"},
      {"empty-2", empty, absl::nullopt, "emulator", "emulator"},
      {"empty-3", empty, "", absl::nullopt, "default"},
      {"empty-4", empty, "", "", "default"},
      {"empty-5", empty, "", "emulator", "emulator"},
      {"empty-6", empty, "env", absl::nullopt, "env"},
      {"empty-7", empty, "env", "", "env"},
      {"empty-8", empty, "env", "emulator", "emulator"},
      {"with-ep-0", with_ep, absl::nullopt, absl::nullopt, "with-ep"},
      {"with-ep-1", with_ep, absl::nullopt, "", "with-ep"},
      {"with-ep-2", with_ep, absl::nullopt, "emulator", "emulator"},
      {"with-ep-3", with_ep, "", absl::nullopt, "with-ep"},
      {"with-ep-4", with_ep, "", "", "with-ep"},
      {"with-ep-5", with_ep, "", "emulator", "emulator"},
      {"with-ep-6", with_ep, "env", absl::nullopt, "env"},
      {"with-ep-7", with_ep, "env", "", "env"},
      {"with-ep-8", with_ep, "env", "emulator", "emulator"},
  };

  for (auto const& test : test_cases) {
    SCOPED_TRACE("Running test case " + test.name);
    ScopedEnvironment env("DEFAULT", test.default_env);
    ScopedEnvironment emulator("EMULATOR", test.emulator_env);
    auto actual =
        PopulateCommonOptions(test.initial, "DEFAULT", "EMULATOR", "default");
    EXPECT_EQ(actual.get<EndpointOption>(), test.expected);
    EXPECT_EQ(actual.get<AuthorityOption>(), "default");
  }
}

TEST(PopulateCommonOptions, UserProject) {
  auto const empty = Options{};
  auto const with_value = Options{}.set<UserProjectOption>("with-value");
  struct TestCase {
    std::string name;
    Options initial;
    absl::optional<std::string> env;
    std::string expected;
  } test_cases[] = {
      {"without-value", empty, "user-project", "user-project"},
      {"with-value-0", with_value, absl::nullopt, "with-value"},
      {"with-value-1", with_value, "user-project", "user-project"},
  };

  for (auto const& test : test_cases) {
    SCOPED_TRACE("Running test case " + test.name);
    ScopedEnvironment env("GOOGLE_CLOUD_CPP_USER_PROJECT", test.env);
    auto actual = PopulateCommonOptions(test.initial, {}, {}, "default");
    EXPECT_EQ(actual.get<UserProjectOption>(), test.expected);
  }

  // Also test the case where nothing is set.
  ScopedEnvironment unset("GOOGLE_CLOUD_CPP_USER_PROJECT", absl::nullopt);
  auto actual = PopulateCommonOptions(Options{}, {}, {}, "default");
  EXPECT_FALSE(actual.has<UserProjectOption>());
}

TEST(PopulateCommonOptions, OverrideAuthorityOption) {
  // Unset all the relevant environment variables
  ScopedEnvironment user("GOOGLE_CLOUD_CPP_USER_PROJECT", absl::nullopt);
  ScopedEnvironment tracing("GOOGLE_CLOUD_CPP_ENABLE_TRACING", absl::nullopt);
  auto actual = PopulateCommonOptions(
      Options{}.set<AuthorityOption>("custom-authority.googleapis.com"), {}, {},
      "default.googleapis.com");
  EXPECT_EQ(actual.get<EndpointOption>(), "default.googleapis.com");
  EXPECT_EQ(actual.get<AuthorityOption>(), "custom-authority.googleapis.com");
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

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

#include "google/cloud/internal/service_endpoint.h"
#include "google/cloud/common_options.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/universe_domain_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;

auto constexpr kDefaultEndpoint = "default_endpoint.googleapis.com";

TEST(DetermineServiceEndpoint, EnvVarSet) {
  auto constexpr kEnvVarEndpoint = "foo.testing.net";
  Options options;
  auto result = DetermineServiceEndpoint(kEnvVarEndpoint,
                                         ExtractOption<EndpointOption>(options),
                                         kDefaultEndpoint, options);
  EXPECT_THAT(result, IsOkAndHolds(kEnvVarEndpoint));
}

TEST(DetermineServiceEndpoint, EnvVarEmpty) {
  auto constexpr kEnvVarEndpoint = "";
  Options options;
  auto result = DetermineServiceEndpoint(kEnvVarEndpoint,
                                         ExtractOption<EndpointOption>(options),
                                         kDefaultEndpoint, options);
  EXPECT_THAT(result, IsOkAndHolds(absl::StrCat(kDefaultEndpoint, ".")));
}

TEST(DetermineServiceEndpoint, EndpointOptionSet) {
  auto constexpr kOptionEndpoint = "option.testing.net";
  auto options = Options{}.set<EndpointOption>(kOptionEndpoint);
  auto result = DetermineServiceEndpoint(absl::nullopt,
                                         ExtractOption<EndpointOption>(options),
                                         kDefaultEndpoint, options);
  EXPECT_THAT(result, IsOkAndHolds(kOptionEndpoint));
}

TEST(DetermineServiceEndpoint, EndpointOptionEmpty) {
  auto constexpr kOptionEndpoint = "";
  auto options = Options{}.set<EndpointOption>(kOptionEndpoint);
  auto result = DetermineServiceEndpoint(absl::nullopt,
                                         ExtractOption<EndpointOption>(options),
                                         kDefaultEndpoint, options);
  EXPECT_THAT(result, IsOkAndHolds(kOptionEndpoint));
}

TEST(DetermineServiceEndpoint, UniverseDomainSetWithNonEmptyValue) {
  auto constexpr kUniverseDomain = "universe.domain";
  auto options = Options{}.set<UniverseDomainOption>(kUniverseDomain);
  auto result = DetermineServiceEndpoint(absl::nullopt,
                                         ExtractOption<EndpointOption>(options),
                                         kDefaultEndpoint, options);
  EXPECT_THAT(result, IsOkAndHolds("default_endpoint.universe.domain"));
}

TEST(DetermineServiceEndpoint, UniverseDomainSetWithTrailingPeriod) {
  auto constexpr kUniverseDomain = "universe.domain.";
  auto options = Options{}.set<UniverseDomainOption>(kUniverseDomain);
  auto result = DetermineServiceEndpoint(absl::nullopt,
                                         ExtractOption<EndpointOption>(options),
                                         kDefaultEndpoint, options);
  EXPECT_THAT(result, IsOkAndHolds("default_endpoint.universe.domain."));
}

TEST(DetermineServiceEndpoint, UniverseDomainSetWithEmptyValue) {
  auto options = Options{}.set<UniverseDomainOption>("");
  auto result = DetermineServiceEndpoint(absl::nullopt,
                                         ExtractOption<EndpointOption>(options),
                                         kDefaultEndpoint, options);
  EXPECT_THAT(result,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("UniverseDomainOption cannot be empty")));
}

TEST(DetermineServiceEndpoint, DefaultHost) {
  Options options;
  auto result = DetermineServiceEndpoint(absl::nullopt,
                                         ExtractOption<EndpointOption>(options),
                                         kDefaultEndpoint, options);
  EXPECT_THAT(result, IsOkAndHolds(absl::StrCat(kDefaultEndpoint, ".")));
}

TEST(UniverseDomainEndpoint, WithoutUniverseDomainOption) {
  auto ep = UniverseDomainEndpoint("foo.googleapis.com.", Options{});
  EXPECT_EQ(ep, "foo.googleapis.com.");
}

TEST(UniverseDomainEndpoint, WithUniverseDomainOption) {
  auto ep = UniverseDomainEndpoint(
      "foo.googleapis.com.", Options{}.set<UniverseDomainOption>("my-ud.net"));
  EXPECT_EQ(ep, "foo.my-ud.net");
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

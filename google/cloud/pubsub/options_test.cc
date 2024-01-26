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

#include "google/cloud/pubsub/options.h"
#include "google/cloud/common_options.h"
#include "google/cloud/universe_domain_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(IAMPolicyOptions, Default) {
  auto const actual = IAMPolicyOptions();
  EXPECT_EQ(actual.get<EndpointOption>(), "pubsub.googleapis.com");
  EXPECT_EQ(actual.get<AuthorityOption>(), "pubsub.googleapis.com");
}

TEST(IAMPolicyOptions, OverrideAll) {
  auto const actual =
      IAMPolicyOptions(Options{}
                           .set<EndpointOption>("test-only-endpoint")
                           .set<AuthorityOption>("test-only-authority")
                           .set<UserProjectOption>("test-only-user-project"));
  EXPECT_EQ(actual.get<EndpointOption>(), "test-only-endpoint");
  EXPECT_EQ(actual.get<AuthorityOption>(), "test-only-authority");
  EXPECT_EQ(actual.get<UserProjectOption>(), "test-only-user-project");
}

TEST(IAMPolicyOptions, OverrideEndpoint) {
  auto const actual =
      IAMPolicyOptions(Options{}
                           .set<EndpointOption>("test-only-endpoint")
                           .set<UserProjectOption>("test-only-user-project"));
  EXPECT_EQ(actual.get<EndpointOption>(), "test-only-endpoint");
  EXPECT_EQ(actual.get<AuthorityOption>(), "pubsub.googleapis.com");
  EXPECT_EQ(actual.get<UserProjectOption>(), "test-only-user-project");
}

TEST(IAMPolicyOptions, OverrideAuthority) {
  auto const actual =
      IAMPolicyOptions(Options{}
                           .set<AuthorityOption>("test-only-authority")
                           .set<UserProjectOption>("test-only-user-project"));
  EXPECT_EQ(actual.get<EndpointOption>(), "pubsub.googleapis.com");
  EXPECT_EQ(actual.get<AuthorityOption>(), "test-only-authority");
  EXPECT_EQ(actual.get<UserProjectOption>(), "test-only-user-project");
}

TEST(IAMPolicyOptions, IncorporatesUniverseDomain) {
  auto const actual = IAMPolicyOptions(
      Options{}.set<internal::UniverseDomainOption>("my-ud.net"));
  EXPECT_EQ(actual.get<EndpointOption>(), "pubsub.my-ud.net");
  EXPECT_EQ(actual.get<AuthorityOption>(), "pubsub.my-ud.net");
}

TEST(IAMPolicyOptions, EndpointOverridesUniverseDomain) {
  auto const actual =
      IAMPolicyOptions(Options{}
                           .set<internal::UniverseDomainOption>("my-ud.net")
                           .set<EndpointOption>("test-only-endpoint")
                           .set<AuthorityOption>("test-only-authority"));
  EXPECT_EQ(actual.get<EndpointOption>(), "test-only-endpoint");
  EXPECT_EQ(actual.get<AuthorityOption>(), "test-only-authority");
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

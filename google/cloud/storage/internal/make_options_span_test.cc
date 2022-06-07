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

#include "google/cloud/storage/internal/make_options_span.h"
#include "google/cloud/storage/well_known_parameters.h"
#include "google/cloud/common_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::CurrentOptions;

google::cloud::Options SimulateRawClientOptions() {
  return Options{}
      .set<UserProjectOption>("u-p-default")
      .set<AuthorityOption>("a-default");
}

TEST(MakeOptionSpanTest, JustDefaults) {
  auto const span = MakeOptionsSpan(SimulateRawClientOptions());
  auto const& current = CurrentOptions();
  EXPECT_EQ("u-p-default", current.get<UserProjectOption>());
  EXPECT_EQ("a-default", current.get<AuthorityOption>());
}

TEST(MakeOptionSpanTest, Overrides) {
  auto const span =
      MakeOptionsSpan(SimulateRawClientOptions(),
                      Options{}
                          .set<EndpointOption>("test-endpoint")
                          .set<AuthorityOption>("a-override-1"),
                      Options{}.set<AuthorityOption>("a-override-2"));
  auto const& current = CurrentOptions();
  EXPECT_EQ("u-p-default", current.get<UserProjectOption>());
  EXPECT_EQ("a-override-2", current.get<AuthorityOption>());
  EXPECT_EQ("test-endpoint", current.get<EndpointOption>());
}

TEST(MakeOptionSpanTest, OverridesMixedWithRequestOptions) {
  auto const span = MakeOptionsSpan(
      SimulateRawClientOptions(), IfGenerationMatch(0),
      Options{}.set<EndpointOption>("test-endpoint"), IfGenerationNotMatch(0),
      Options{}.set<AuthorityOption>("a-override-1"), IfMetagenerationMatch(0),
      Options{}.set<AuthorityOption>("a-override-2"),
      IfMetagenerationNotMatch(0), Generation(7), IfGenerationMatch(0));
  auto const& current = CurrentOptions();
  EXPECT_EQ("u-p-default", current.get<UserProjectOption>());
  EXPECT_EQ("a-override-2", current.get<AuthorityOption>());
  EXPECT_EQ("test-endpoint", current.get<EndpointOption>());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google
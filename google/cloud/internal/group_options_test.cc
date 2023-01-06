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

#include "google/cloud/internal/group_options.h"
#include "google/cloud/common_options.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

Options TestOptions() {
  return Options{}
      .set<UserProjectOption>("u-p-default")
      .set<AuthorityOption>("a-default");
}

TEST(MakeOptionSpanTest, JustOptions) {
  auto const group = GroupOptions(TestOptions());
  EXPECT_EQ("u-p-default", group.get<UserProjectOption>());
  EXPECT_EQ("a-default", group.get<AuthorityOption>());
}

TEST(MakeOptionSpanTest, Overrides) {
  auto const group =
      GroupOptions(TestOptions(),
                   Options{}
                       .set<EndpointOption>("test-endpoint")
                       .set<AuthorityOption>("a-override-1"),
                   Options{}.set<AuthorityOption>("a-override-2"));
  EXPECT_EQ("u-p-default", group.get<UserProjectOption>());
  EXPECT_EQ("a-override-2", group.get<AuthorityOption>());
  EXPECT_EQ("test-endpoint", group.get<EndpointOption>());
}

TEST(MakeOptionSpanTest, OverridesMixedWithRequestOptions) {
  int x = 5;
  int const& y = x;
  int const z = 10;
  struct Thing {};

  auto const group = GroupOptions(
      "string", TestOptions(), Thing{},
      Options{}.set<EndpointOption>("test-endpoint"), 5,
      Options{}.set<AuthorityOption>("a-override-1"), x,
      Options{}.set<AuthorityOption>("a-override-2"), y, std::move(z));
  EXPECT_EQ("u-p-default", group.get<UserProjectOption>());
  EXPECT_EQ("a-override-2", group.get<AuthorityOption>());
  EXPECT_EQ("test-endpoint", group.get<EndpointOption>());
}

TEST(MakeOptionSpanTest, Declaration) {
  auto const g1 = GroupOptions(TestOptions(), 5);
  EXPECT_EQ("u-p-default", g1.get<UserProjectOption>());

  auto const g2 = GroupOptions(5, TestOptions());
  EXPECT_EQ("u-p-default", g2.get<UserProjectOption>());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

// Copyright 2019 Google LLC
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

#include "google/cloud/storage/iam_policy.h"
#include "google/cloud/storage/version.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
TEST(Expression, Trivial) {
  auto expr =
      Expression("request.host == \"hr.example.com\"", "title", "descr", "loc");
  EXPECT_EQ("request.host == \"hr.example.com\"", expr["expression"]);
  EXPECT_EQ("title", expr["title"]);
  EXPECT_EQ("descr", expr["description"]);
  EXPECT_EQ("loc", expr["location"]);
}

TEST(IamBinding, IterCtor) {
  std::vector<std::string> expected({"mem1", "mem2", "mem3", "mem1"});
  auto binding = IamBinding("role", expected.begin(), expected.end());
  EXPECT_EQ("role", binding["role"]);
  EXPECT_EQ(expected, std::vector<std::string>(binding["members"].begin(),
                                               binding["members"].end()));
}

TEST(IamBinding, IterCtorCondition) {
  std::vector<std::string> expected({"mem1", "mem2", "mem3", "mem1"});
  auto binding = IamBinding("role", expected.begin(), expected.end(),
                            Expression("condition"));
  EXPECT_EQ("role", binding["role"]);
  EXPECT_EQ(expected, std::vector<std::string>(binding["members"].begin(),
                                               binding["members"].end()));
  EXPECT_EQ("condition", binding["condition"]["expression"]);
}

TEST(IamBinding, IniListCtor) {
  auto binding = IamBinding("role", {"mem1", "mem2", "mem3", "mem1"});
  EXPECT_EQ("role", binding["role"]);
  std::vector<std::string> expected({"mem1", "mem2", "mem3", "mem1"});
  EXPECT_EQ(expected, std::vector<std::string>(binding["members"].begin(),
                                               binding["members"].end()));
}

TEST(IamBinding, IniListCtorCondition) {
  auto binding = IamBinding("role", {"mem1", "mem2", "mem3", "mem1"},
                            Expression("condition"));
  EXPECT_EQ("role", binding["role"]);
  std::vector<std::string> expected({"mem1", "mem2", "mem3", "mem1"});
  EXPECT_EQ(expected, std::vector<std::string>(binding["members"].begin(),
                                               binding["members"].end()));
  EXPECT_EQ("condition", binding["condition"]["expression"]);
}

TEST(IamBinding, VectorCtor) {
  std::vector<std::string> expected({"mem1", "mem2", "mem3", "mem1"});
  auto binding = IamBinding("role", expected);
  EXPECT_EQ("role", binding["role"]);
  EXPECT_EQ(expected, std::vector<std::string>(binding["members"].begin(),
                                               binding["members"].end()));
}

TEST(IamBinding, VectorCtorCondition) {
  std::vector<std::string> expected({"mem1", "mem2", "mem3", "mem1"});
  auto binding = IamBinding("role", expected, Expression("condition"));
  EXPECT_EQ("role", binding["role"]);
  EXPECT_EQ(expected, std::vector<std::string>(binding["members"].begin(),
                                               binding["members"].end()));
  EXPECT_EQ("condition", binding["condition"]["expression"]);
}

template <typename Iter>
std::vector<std::string> BindingRoles(Iter first_binding, Iter last_binding) {
  std::vector<std::string> binding_roles;
  std::transform(
      first_binding, last_binding, std::back_inserter(binding_roles),
      [](internal::nl::json const& binding) { return binding["role"]; });
  return binding_roles;
}

std::vector<std::string> BindingRoles(internal::nl::json const& policy) {
  return BindingRoles(policy["bindings"].begin(), policy["bindings"].end());
}

std::vector<std::string> BindingRoles(
    std::vector<internal::nl::json> bindings) {
  return BindingRoles(bindings.begin(), bindings.end());
}

TEST(IamPolicy, IterCtor) {
  auto binding1 = IamBinding("role1", {"mem1"});
  auto binding2 = IamBinding("role2", {"mem2"});
  std::vector<internal::nl::json> expected({binding1, binding2});
  auto policy = IamPolicy(expected.begin(), expected.end(), "etag1", 13);
  EXPECT_EQ("storage#policy", policy["kind"]);
  EXPECT_EQ("etag1", policy["etag"]);
  EXPECT_EQ(13, policy["version"]);
  EXPECT_EQ(BindingRoles(expected), BindingRoles(policy));
}

TEST(IamPolicy, IniListCtor) {
  auto binding1 = IamBinding("role1", {"mem1"});
  auto binding2 = IamBinding("role2", {"mem2"});
  auto policy = IamPolicy({binding1, binding2}, "etag1", 13);
  EXPECT_EQ("storage#policy", policy["kind"]);
  EXPECT_EQ("etag1", policy["etag"]);
  EXPECT_EQ(13, policy["version"]);
  std::vector<internal::nl::json> expected({binding1, binding2});
  EXPECT_EQ(BindingRoles(expected), BindingRoles(policy));
}

TEST(IamPolicy, VectorCtor) {
  auto binding1 = IamBinding("role1", {"mem1"});
  auto binding2 = IamBinding("role2", {"mem2"});
  std::vector<internal::nl::json> expected({binding1, binding2});
  auto policy = IamPolicy(expected, "etag1", 13);
  EXPECT_EQ("storage#policy", policy["kind"]);
  EXPECT_EQ("etag1", policy["etag"]);
  EXPECT_EQ(13, policy["version"]);
  EXPECT_EQ(BindingRoles(expected), BindingRoles(policy));
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

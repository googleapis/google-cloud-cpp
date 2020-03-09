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

#include "google/cloud/bigtable/iam_policy.h"
#include "google/cloud/bigtable/expr.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace {

template <typename Iter>
std::vector<std::string> BindingRoles(Iter first_binding, Iter last_binding) {
  std::vector<std::string> binding_roles;
  std::transform(
      first_binding, last_binding, std::back_inserter(binding_roles),
      [](google::iam::v1::Binding const& binding) { return binding.role(); });
  return binding_roles;
}

std::vector<std::string> BindingRoles(google::iam::v1::Policy const& policy) {
  return BindingRoles(policy.bindings().begin(), policy.bindings().end());
}

std::vector<std::string> BindingRoles(
    std::vector<google::iam::v1::Binding> bindings) {
  return BindingRoles(bindings.begin(), bindings.end());
}

TEST(IamPolicy, IterCtor) {
  auto binding1 = IamBinding("role1", {"mem1"});
  auto binding2 = IamBinding("role2", {"mem2"});
  std::vector<google::iam::v1::Binding> expected({binding1, binding2});
  auto policy = IamPolicy(expected.begin(), expected.end(), "etag1", 13);
  EXPECT_EQ("etag1", policy.etag());
  EXPECT_EQ(13, policy.version());
  EXPECT_EQ(BindingRoles(expected), BindingRoles(policy));
}

TEST(IamPolicy, IniListCtor) {
  auto binding1 = IamBinding("role1", {"mem1"});
  auto binding2 = IamBinding("role2", {"mem2"});
  auto policy = IamPolicy({binding1, binding2}, "etag1", 13);
  EXPECT_EQ("etag1", policy.etag());
  EXPECT_EQ(13, policy.version());
  std::vector<google::iam::v1::Binding> expected({binding1, binding2});
  EXPECT_EQ(BindingRoles(expected), BindingRoles(policy));
}

TEST(IamPolicy, VectorCtor) {
  auto binding1 = IamBinding("role1", {"mem1"});
  auto binding2 = IamBinding("role2", {"mem2"});
  std::vector<google::iam::v1::Binding> expected({binding1, binding2});
  auto policy = IamPolicy(expected, "etag1", 13);
  EXPECT_EQ("etag1", policy.etag());
  EXPECT_EQ(13, policy.version());
  EXPECT_EQ(BindingRoles(expected), BindingRoles(policy));
}

TEST(IamPolicy, Printing) {
  auto binding = IamBinding("role", {"mem1", "mem2", "mem3", "mem1"},
                            Expression("condition"));
  auto policy = IamPolicy({binding});
  std::stringstream stream;
  stream << policy;
  EXPECT_EQ(
      "IamPolicy={version=0, bindings=IamBindings={role: [mem1, mem2, mem3, "
      "mem1] when (condition)}, etag=}",
      stream.str());
}

}  // namespace
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

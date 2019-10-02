// Copyright 2018 Google LLC
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

#include "google/cloud/iam_bindings.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {
TEST(IamBindingsTest, DefaultConstructor) {
  std::string role = "writer";
  std::set<std::string> members = {"abc@gmail.com", "xyz@gmail.com"};

  auto iam_binding = IamBinding(role, members);

  std::vector<IamBinding> bindings_vector = {iam_binding};

  auto iam_bindings = IamBindings(bindings_vector);

  EXPECT_EQ(1, iam_bindings.bindings().size());
  EXPECT_EQ("writer", iam_bindings.bindings().begin()->first);
  EXPECT_EQ(2, iam_bindings.bindings().begin()->second.size());
}

TEST(IamBindingsTest, AddMemberTestRoleExists) {
  std::string role = "writer";
  std::set<std::string> members = {"abc@gmail.com", "xyz@gmail.com"};

  auto iam_binding = IamBinding(role, members);

  std::vector<IamBinding> bindings_vector = {iam_binding};

  auto iam_bindings = IamBindings(bindings_vector);

  iam_bindings.AddMember(role, "jkl@gmail.com");

  EXPECT_EQ(3, iam_bindings.bindings().begin()->second.size());
}

TEST(IamBindingsTest, AddMemberTestNewRole) {
  std::string role = "writer";
  std::set<std::string> members = {"abc@gmail.com", "xyz@gmail.com"};

  auto iam_binding = IamBinding(role, members);

  std::vector<IamBinding> bindings_vector = {iam_binding};

  auto iam_bindings = IamBindings(bindings_vector);

  std::string new_role = "reader";
  iam_bindings.AddMember(new_role, "jkl@gmail.com");

  EXPECT_EQ(2, iam_bindings.bindings().size());
}

TEST(IamBindingsTest, AddMembersTestRoleExists) {
  std::string role = "writer";
  std::set<std::string> members = {"abc@gmail.com", "xyz@gmail.com"};

  auto iam_binding = IamBinding(role, members);

  std::vector<IamBinding> bindings_vector = {iam_binding};

  auto iam_bindings = IamBindings(bindings_vector);
  std::set<std::string> new_members = {"jkl@gmail.com", "pqr@gmail.com"};
  iam_bindings.AddMembers(role, new_members);

  EXPECT_EQ(4, iam_bindings.bindings().begin()->second.size());
}

TEST(IamBindingsTest, AddMembersTestIamBindingParma) {
  std::string role = "writer";
  std::set<std::string> members = {"abc@gmail.com", "xyz@gmail.com"};

  auto iam_binding = IamBinding(role, members);

  std::vector<IamBinding> bindings_vector = {iam_binding};

  auto iam_bindings = IamBindings(bindings_vector);

  std::set<std::string> new_members = {"jkl@gmail.com", "pqr@gmail.com"};
  auto iam_binding_for_addition = IamBinding(role, new_members);
  iam_bindings.AddMembers(iam_binding_for_addition);

  EXPECT_EQ(4, iam_bindings.bindings().begin()->second.size());
}

TEST(IamBindingsTest, AddMembersTestNewRole) {
  std::string role = "writer";
  std::set<std::string> members = {"abc@gmail.com", "xyz@gmail.com"};

  auto iam_binding = IamBinding(role, members);

  std::vector<IamBinding> bindings_vector = {iam_binding};

  auto iam_bindings = IamBindings(bindings_vector);

  std::string new_role = "reader";
  std::set<std::string> new_members = {"jkl@gmail.com", "pqr@gmail.com"};
  iam_bindings.AddMembers(new_role, new_members);

  EXPECT_EQ(2, iam_bindings.bindings().size());
}

TEST(IamBindingsTest, RemoveMemberTest) {
  std::string role = "writer";
  std::set<std::string> members = {"abc@gmail.com", "xyz@gmail.com"};

  auto iam_binding = IamBinding(role, members);

  std::vector<IamBinding> bindings_vector = {iam_binding};

  auto iam_bindings = IamBindings(bindings_vector);
  iam_bindings.RemoveMember(role, "abc@gmail.com");

  auto temp_binding = iam_bindings.bindings();
  auto it = temp_binding[role].find("abc@gmail.com");
  EXPECT_EQ(temp_binding[role].end(), it);

  iam_bindings.RemoveMember("writer", "xyz@gmail.com");
  EXPECT_TRUE(iam_bindings.end() == iam_bindings.find(role));
}

TEST(IamBindingsTest, RemoveMembersTest) {
  std::string role = "writer";
  std::set<std::string> members = {"abc@gmail.com", "xyz@gmail.com"};

  auto iam_binding = IamBinding(role, members);

  std::vector<IamBinding> bindings_vector = {iam_binding};

  auto iam_bindings = IamBindings(bindings_vector);
  std::set<std::string> member_list = {"abc@gmail.com"};
  iam_bindings.RemoveMembers(role, member_list);

  auto temp_binding = iam_bindings.bindings();
  EXPECT_TRUE(temp_binding[role].end() ==
              temp_binding[role].find("abc@gmail.com"));

  iam_bindings.RemoveMembers(role, std::set<std::string>{"xyz@gmail.com"});
  EXPECT_TRUE(iam_bindings.end() == iam_bindings.find(role));
}

TEST(IamBindingsTest, RemoveMembersTestIamBindingParam) {
  std::string role = "writer";
  std::set<std::string> members = {"abc@gmail.com", "xyz@gmail.com"};

  auto iam_binding = IamBinding(role, members);

  std::vector<IamBinding> bindings_vector = {iam_binding};

  auto iam_bindings = IamBindings(bindings_vector);
  std::set<std::string> member_list = {"abc@gmail.com"};
  auto iam_binding_for_removal = IamBinding(role, member_list);
  iam_bindings.RemoveMembers(iam_binding_for_removal);

  auto temp_binding = iam_bindings.bindings();
  bool has_removed_member = false;

  for (auto it : temp_binding[role]) {
    if (it == *member_list.begin()) {
      has_removed_member = true;
      break;
    }
  }

  EXPECT_FALSE(has_removed_member);
}
}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

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
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

TEST(NativeIamPolicy, CtorAndAccessors) {
  NativeIamPolicy policy({NativeIamBinding("role1", {"member1", "member2"})},
                         14, "etag");
  NativeIamPolicy const& const_policy = policy;
  ASSERT_EQ(14, const_policy.version());
  ASSERT_EQ("etag", const_policy.etag());
  policy.set_version(13);
  ASSERT_EQ(13, const_policy.version());
  policy.set_etag("etag_1");
  ASSERT_EQ("etag_1", const_policy.etag());

  ASSERT_EQ(std::list<NativeIamBinding>({
                NativeIamBinding("role1", {"member1", "member2"}),
            }),
            const_policy.bindings());
  policy.bindings().emplace_back(
      NativeIamBinding("role2", {"member1", "member3"}));
  ASSERT_EQ(std::list<NativeIamBinding>(
                {NativeIamBinding("role1", {"member1", "member2"}),
                 NativeIamBinding("role2", {"member1", "member3"})}),
            const_policy.bindings());
}

TEST(NativeIamPolicy, Protos) {
  NativeIamPolicy policy;
  policy.set_version(17);
  policy.set_etag("etag1");
  policy.bindings().emplace_back(
      NativeIamBinding(NativeIamBinding("role1", {"member1", "member2"})));

  policy = NativeIamPolicy(std::move(policy).ToProto());

  ASSERT_EQ(17, policy.version());
  ASSERT_EQ("etag1", policy.etag());
  ASSERT_EQ(1U, policy.bindings().size());
  ASSERT_EQ("role1", policy.bindings().front().role());
  ASSERT_EQ(std::set<std::string>({"member1", "member2"}),
            policy.bindings().front().members());
}

template <typename PROTO, typename WRAPPER>
PROTO AddUnknownField(WRAPPER const& base, std::string const& content) {
  std::string serialized;
  auto binding_proto = base.ToProto();

  auto reflection = binding_proto.GetReflection();
  reflection->MutableUnknownFields(&binding_proto)
      ->AddLengthDelimited(17, content);
  return binding_proto;
}

google::iam::v1::Policy CreatePolicyWithUnknownFields() {
  NativeIamPolicy policy;
  policy.set_version(17);
  policy.set_etag("etag1");
  policy.bindings().emplace_back(NativeIamBinding(
      AddUnknownField<google::iam::v1::Binding, NativeIamBinding>(
          NativeIamBinding("role1", {"member1", "member2"}),
          "unknown_binding_field")));
  return AddUnknownField<google::iam::v1::Policy, NativeIamPolicy>(
      policy, "unknown_policy_field");
}

TEST(NativeIamPolicy, ProtosWithUnknownFields) {
  NativeIamPolicy policy(CreatePolicyWithUnknownFields());
  policy.bindings().emplace_back(NativeIamBinding("role2", {"member3"}));
  auto policy_proto = policy.ToProto();

  std::string serialized;
  google::protobuf::TextFormat::PrintToString(policy_proto, &serialized);
  ASSERT_THAT(serialized, testing::ContainsRegex("unknown_binding_field"));
  ASSERT_THAT(serialized, testing::ContainsRegex("unknown_policy_field"));
}

TEST(NativeIamPolicy, RemoveAllBindings) {
  NativeIamPolicy policy;

  policy.bindings().emplace_back(
      NativeIamBinding("role1", {"member1", "member2"}));
  policy.bindings().emplace_back(
      NativeIamBinding("role2", {"member1", "member3"}));
  policy.bindings().emplace_back(
      NativeIamBinding("role1", {"member3", "member4"}));
  policy.RemoveAllBindingsByRole("role1");
  ASSERT_EQ(std::list<NativeIamBinding>(
                {NativeIamBinding("role2", {"member1", "member3"})}),
            policy.bindings());
}

TEST(NativeIamPolicy, RemoveMemberFromAllBindings) {
  NativeIamPolicy policy;

  policy.bindings().emplace_back(
      NativeIamBinding("role1", {"member1", "member2"}));
  policy.bindings().emplace_back(NativeIamBinding("role2", {"member1"}));
  policy.bindings().emplace_back(
      NativeIamBinding("role3", {"member3", "member4"}));
  policy.RemoveMemberFromAllBindings("member1");
  ASSERT_EQ(std::list<NativeIamBinding>(
                {NativeIamBinding("role1", {"member2"}),
                 NativeIamBinding("role3", {"member3", "member4"})}),
            policy.bindings());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

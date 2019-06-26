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

#include "google/cloud/bigtable/iam_binding.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <fstream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {
TEST(NativeIamBinding, CtorAndAccessors) {
  NativeIamBinding binding("role", {"member1", "member2"});
  NativeIamBinding const& const_binding = binding;
  ASSERT_EQ("role", const_binding.role());
  binding.set_role("role2");
  ASSERT_EQ("role2", const_binding.role());
  ASSERT_EQ(std::set<std::string>({"member1", "member2"}),
            const_binding.members());
  ASSERT_TRUE(binding.members().emplace("member3").second);
  ASSERT_EQ(std::set<std::string>({"member1", "member2", "member3"}),
            const_binding.members());
}

google::iam::v1::Binding CreateBindingWithUnknownFields() {
  NativeIamBinding binding("role", {"member1", "member2"});
  std::string serialized;
  auto binding_proto = binding.ToProto();

  auto reflection = binding_proto.GetReflection();
  reflection->MutableUnknownFields(&binding_proto)
      ->AddLengthDelimited(17, "unknown_field_content");
  return binding_proto;
}

TEST(NativeIamBinding, Protos) {
  NativeIamBinding binding("my_role", {"my_member1", "my_member2"});
  auto proto = std::move(binding).ToProto();
  binding = NativeIamBinding(std::move(proto));

  ASSERT_EQ("my_role", binding.role());
  ASSERT_EQ(std::set<std::string>({"my_member1", "my_member2"}),
            binding.members());
}

TEST(NativeIamBinding, ProtosUnknownFields) {
  NativeIamBinding binding(CreateBindingWithUnknownFields());

  binding.members().emplace("my_unique_member");
  auto binding_proto = binding.ToProto();
  std::string serialized;
  google::protobuf::TextFormat::PrintToString(binding_proto, &serialized);
  ASSERT_THAT(serialized, testing::ContainsRegex("unknown_field_content"));
  ASSERT_THAT(serialized, testing::ContainsRegex("my_unique_member"));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

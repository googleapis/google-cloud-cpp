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
#include <google/protobuf/util/message_differencer.h>
#include <algorithm>
#include <iostream>
#include <iterator>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace GOOGLE_CLOUD_CPP_NS {
google::iam::v1::Binding IamBinding(
    std::string role, std::initializer_list<std::string> members) {
  return IamBinding(std::move(role), members.begin(), members.end());
}

google::iam::v1::Binding IamBinding(std::string role,
                                    std::vector<std::string> members) {
  google::iam::v1::Binding res;
  for (auto& member : members) {
    *res.add_members() = std::move(member);
  }
  res.set_role(std::move(role));
  return res;
}

std::ostream& operator<<(std::ostream& os,
                         google::iam::v1::Binding const& binding) {
  os << binding.role() << ": [";
  bool first = true;
  for (auto const& member : binding.members()) {
    os << (first ? "" : ", ") << member;
    first = false;
  }
  return os << "]";
}

size_t RemoveMemberFromBinding(google::iam::v1::Binding& binding,
                               std::string const& name) {
  return RemoveMembersFromBindingIf(
      binding, [&name](std::string const& member) { return name == member; });
}

void RemoveMemberFromBinding(
    google::iam::v1::Binding& binding,
    ::google::protobuf::RepeatedPtrField<::std::string>::iterator to_remove) {
  RemoveMembersFromBindingIf(binding, [&to_remove](std::string const& member) {
    return &(*to_remove) == &member;
  });
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

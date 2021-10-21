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
#include "google/cloud/bigtable/expr.h"
#include <google/protobuf/util/message_differencer.h>
#include <algorithm>
#include <iostream>
#include <iterator>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
google::iam::v1::Binding IamBinding(
    std::string role, std::initializer_list<std::string> members) {
  return IamBinding(std::move(role), members.begin(), members.end());
}

google::iam::v1::Binding IamBinding(std::string role,
                                    std::initializer_list<std::string> members,
                                    google::type::Expr condition) {
  return IamBindingSetCondition(IamBinding(std::move(role), members),
                                std::move(condition));
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

google::iam::v1::Binding IamBinding(std::string role,
                                    std::vector<std::string> members,
                                    google::type::Expr condition) {
  return IamBindingSetCondition(IamBinding(std::move(role), std::move(members)),
                                std::move(condition));
}

google::iam::v1::Binding IamBindingSetCondition(
    google::iam::v1::Binding binding, google::type::Expr condition) {
  *binding.mutable_condition() = std::move(condition);
  return binding;
}

std::ostream& operator<<(std::ostream& os,
                         google::iam::v1::Binding const& binding) {
  os << binding.role() << ": [";
  bool first = true;
  for (auto const& member : binding.members()) {
    os << (first ? "" : ", ") << member;
    first = false;
  }
  os << "]";
  if (binding.has_condition()) {
    os << " when " << binding.condition();
  }
  return os;
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

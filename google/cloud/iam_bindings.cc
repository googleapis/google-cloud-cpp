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
#include "google/cloud/internal/absl_str_join_quiet.h"
#include <algorithm>
#include <iostream>
#include <iterator>
// TODO(#5929) - remove after decommission is completed
#include "google/cloud/internal/disable_deprecation_warnings.inc"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
void IamBindings::AddMember(std::string const& role, std::string member) {
  bindings_[role].emplace(std::move(member));
}

void IamBindings::AddMembers(google::cloud::IamBinding const& iam_binding) {
  std::string const& role(iam_binding.role());
  std::set<std::string> const& members = iam_binding.members();

  bindings_[role].insert(members.begin(), members.end());
}

void IamBindings::AddMembers(std::string const& role,
                             std::set<std::string> const& members) {
  bindings_[role].insert(members.begin(), members.end());
}

void IamBindings::RemoveMember(std::string const& role,
                               std::string const& member) {
  auto it = bindings_.find(role);
  if (it == bindings_.end()) {
    return;
  }

  auto& members = it->second;
  auto member_loc = members.find(member);

  if (member_loc != members.end()) {
    members.erase(member_loc);
  }
  if (members.empty()) {
    bindings_.erase(it);
  }
}

void IamBindings::RemoveMembers(google::cloud::IamBinding const& iam_binding) {
  RemoveMembers(iam_binding.role(), iam_binding.members());
}

void IamBindings::RemoveMembers(std::string const& role,
                                std::set<std::string> const& members) {
  auto it = bindings_.find(role);
  if (it == bindings_.end()) {
    return;
  }

  auto& binding_members = it->second;
  for (auto const& member : members) {
    auto member_loc = binding_members.find(member);
    if (member_loc != binding_members.end()) {
      binding_members.erase(member_loc);
    }
  }
  if (binding_members.empty()) {
    bindings_.erase(it);
  }
}

std::ostream& operator<<(std::ostream& os, IamBindings const& rhs) {
  os << "IamBindings={";
  struct IamBindingFormatter {
    void operator()(std::string* out,
                    std::pair<std::string, std::set<std::string>> const& rhs) {
      out->append(rhs.first);
      out->append(": [");
      out->append(absl::StrJoin(rhs.second, ", "));
      out->append("]");
    }
  };
  os << absl::StrJoin(rhs, ", ", IamBindingFormatter{});
  return os << "}";
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

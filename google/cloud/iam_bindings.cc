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
#include <algorithm>
#include <iterator>

namespace google {
namespace cloud {

void IamBindings::add_member(std::string const& role,
                             std::string const& member) {
  if (bindings_.find(role) != bindings_.end()) {
    bindings_[role].insert(std::move(member));
  } else {
    std::set<std::string> members;
    members.insert(std::move(member));
    bindings_.insert({std::move(role), std::move(members)});
  }
}

void IamBindings::add_members(google::cloud::IamBinding iam_binding) {
  std::string role(std::move(iam_binding.role()));
  std::set<std::string> members = iam_binding.members();

  if (bindings_.find(role) != bindings_.end()) {
    bindings_[role].insert(members.begin(), members.end());
  } else {
    bindings_.insert({std::move(role), std::move(members)});
  }
}

void IamBindings::add_members(std::string const& role,
                              std::set<std::string> const& members) {
  if (bindings_.find(role) != bindings_.end()) {
    bindings_[role].insert(members.begin(), members.end());
  } else {
    bindings_.insert({std::move(role), std::move(members)});
  }
}

void IamBindings::remove_member(std::string const& role,
                                std::string const& member) {
  if (bindings_.find(role) != bindings_.end()) {
    auto it = bindings_[role].find(member);
    if (it != bindings_[role].end()) bindings_[role].erase(it);
  }
}

void IamBindings::remove_members(google::cloud::IamBinding iam_binding) {
  std::string role(std::move(iam_binding.role()));
  std::set<std::string> members = iam_binding.members();

  if (bindings_.find(role) != bindings_.end()) {
    std::set<std::string> binding_members = bindings_[role];
    std::set<std::string> new_members;
    std::set_difference(binding_members.begin(), binding_members.end(),
                        members.begin(), members.end(),
                        std::inserter(new_members, new_members.end()));
    bindings_[role] = new_members;
  }
}

void IamBindings::remove_members(std::string const& role,
                                 std::set<std::string> const& members) {
  if (bindings_.find(role) != bindings_.end()) {
    std::set<std::string> binding_members = bindings_[role];
    std::set<std::string> new_members;
    std::set_difference(binding_members.begin(), binding_members.end(),
                        members.begin(), members.end(),
                        std::inserter(new_members, new_members.end()));
    bindings_[role] = new_members;
  }
}

}  // namespace cloud
}  // namespace google

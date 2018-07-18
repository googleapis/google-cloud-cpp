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

namespace google {
namespace cloud {

void IamBindings::add_member(std::string const& role,
                             std::string const& member) {
  if (bindings_.find(role) != bindings_.end()) {
    bindings_[role].push_back(member);
  } else {
    std::vector<std::string> members;
    members.push_back(member);
    bindings_[role] = members;
  }
}

void IamBindings::add_members(google::cloud::IamBinding iam_binding) {
  std::string role = iam_binding.role();
  std::vector<std::string> members = iam_binding.members();

  if (bindings_.find(role) != bindings_.end()) {
    bindings_[role].insert(std::end(bindings_[role]), std::begin(members),
                           std::end(members));
  } else {
    bindings_[role] = members;
  }
}

void IamBindings::add_members(std::string const& role,
                              std::vector<std::string> const& members) {
  if (bindings_.find(role) != bindings_.end()) {
    bindings_[role].insert(std::end(bindings_[role]), std::begin(members),
                           std::end(members));
  } else {
    bindings_[role] = members;
  }
}

void IamBindings::remove_member(std::string const& role,
                                std::string const& member) {
  if (bindings_.find(role) != bindings_.end()) {
    auto it = std::find(bindings_[role].begin(), bindings_[role].end(), member);
    if (it != bindings_[role].end()) bindings_[role].erase(it);
  }
}

void IamBindings::remove_members(google::cloud::IamBinding iam_binding) {
  std::string role = iam_binding.role();
  std::vector<std::string> members = iam_binding.members();

  if (bindings_.find(role) != bindings_.end()) {
    std::vector<std::string> binding_members = bindings_[role];
    std::vector<std::string> new_members(binding_members.size());
    std::sort(binding_members.begin(), binding_members.end());
    std::sort(members.begin(), members.end());
    auto it = std::set_difference(binding_members.begin(),
                                  binding_members.end(), members.begin(),
                                  members.end(), new_members.begin());
    new_members.resize(it - new_members.begin());
    bindings_[role] = new_members;
  }
}

void IamBindings::remove_members(std::string const& role,
                                 std::vector<std::string> members) {
  if (bindings_.find(role) != bindings_.end()) {
    std::vector<std::string> binding_members = bindings_[role];
    std::vector<std::string> new_members(binding_members.size());
    std::sort(binding_members.begin(), binding_members.end());
    std::sort(members.begin(), members.end());
    auto it = std::set_difference(binding_members.begin(),
                                  binding_members.end(), members.begin(),
                                  members.end(), new_members.begin());
    new_members.resize(it - new_members.begin());
    bindings_[role] = new_members;
  }
}

}  // namespace cloud
}  // namespace google
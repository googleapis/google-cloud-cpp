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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_BINDINGS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_BINDINGS_H_

#include "google/cloud/iam_binding.h"
#include "google/cloud/version.h"
#include <map>
#include <vector>

namespace google {
namespace cloud {

class IamBindings {
 public:
  IamBindings(std::vector<IamBinding> bindings) {
    for (auto it : bindings) {
      bindings_[it.role()] = it.members();
    }
  }

  IamBindings(std::string const& role,
              std::vector<std::string> const& members) {
    bindings_[role] = members;
  }

  std::map<std::string, std::vector<std::string> > const& bindings() const {
    return bindings_;
  };

  void add_member(std::string const& role, std::string const& member);

  void add_members(google::cloud::IamBinding binding);

  void add_members(std::string const& role,
                   std::vector<std::string> const& members);

  void remove_member(std::string const& role, std::string const& member);

  void remove_members(google::cloud::IamBinding binding);

  void remove_members(std::string const& role,
                      std::vector<std::string> members);

 private:
  std::map<std::string, std::vector<std::string> > bindings_;
};

}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_BINDINGS_H_

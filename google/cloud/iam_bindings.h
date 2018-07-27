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
#include <set>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * Represents a container for providing users with a handful of operation to
 * users which they can use to add and remove members to Binding which is used
 * for defining IAM Policy for Cloud Platform Resources.
 *
 * For more information about a Binding please refer to:
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Binding
 */
class IamBindings {
 public:
  IamBindings() = default;

  explicit IamBindings(std::vector<IamBinding> bindings) {
    for (auto& it : bindings) {
      bindings_.insert({std::move(it.role()), std::move(it.members())});
    }
  }

  IamBindings(std::string const& role, std::set<std::string> const& members) {
    bindings_.insert({std::move(role), std::move(members)});
  }

  using iterator = std::map<std::string, std::set<std::string>>::const_iterator;

  /**
   * Returns an iterator referring to the first element in IamBindings
   * container.
   */
  iterator begin() { return bindings_.begin(); }

  /**
   * Returns an iterator referring to the past-the-end element in IamBindings
   * container.
   */
  iterator end() { return bindings_.end(); }

  /**
   * Returns whether the Bindings container is empty.
   *
   * @return bool whether the container is empty or not.
   */
  bool empty() const { return bindings_.empty(); }

  /**
   * Return number of Bindings in container.
   *
   * @return int the size of the container.
   */
  std::size_t size() const { return bindings_.size(); }

  std::map<std::string, std::set<std::string>> const& bindings() const {
    return bindings_;
  }

  /**
   * Adds a new member if a binding exists with given role otherwise inserts
   * a new key-value pair of role and member to the container.
   *
   * @param role role of the new member.
   * @param member specifies the identity requesting access for a cloud
   * platform resource.
   */
  void AddMember(std::string const& role, std::string member);

  /**
   * Adds a new key-value pair of role and members to the container if there is
   * none for the role of given binding else appends members of given binding
   * to the associated role's key-value entry.
   *
   * @param iam_binding binding representing a set of members and role for them.
   */
  void AddMembers(google::cloud::IamBinding const& iam_binding);

  /**
   * Adds a new key-value pair of role and members to the container if there no
   * existing for given role else appends the given members to the give role's
   * member set.
   *
   * @param role role of the member set to be added.
   * @param members a set of member which are needed to be added.
   */
  void AddMembers(std::string const& role,
                  std::set<std::string> const& members);

  /**
   * Removes the given member from the given role's member set if there exists
   * one in container.
   *
   * @param role role of the member to be removed.
   * @param member specifies the identity requesting access for a cloud
   * platform resource.
   */
  void RemoveMember(std::string const& role, std::string const& member);

  /**
   * Removes the given binding's member from the given binding's role's member
   * set if there exists one in container.
   *
   * @param iam_binding binding representing a set of members and role for them.
   */
  void RemoveMembers(google::cloud::IamBinding const& iam_binding);

  /**
   * Removes the given members from given role's member set if there exists one
   * in container.
   *
   * @param role role of the member set to be removed.
   * @param members a set of members which are needed to be removed.
   */
  void RemoveMembers(std::string const& role,
                     std::set<std::string> const& members);

 private:
  std::map<std::string, std::set<std::string>> bindings_;
};

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_BINDINGS_H_

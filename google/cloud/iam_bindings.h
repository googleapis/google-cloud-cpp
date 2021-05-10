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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_BINDINGS_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_BINDINGS_H

#include "google/cloud/iam_binding.h"
#include "google/cloud/version.h"
// TODO(#5929) - remove after decommission is completed
#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * Simplified view of multiple roles and their members for IAM.
 *
 * @deprecated this class is deprecated. Any functions that use it have also
 *     been deprecated. The class was defined before IAM conditional bindings,
 *     and does not support them. Nor will it be able to support future IAM
 *     features. Please use the alternative functions.
 *
 * @see [Identity and Access Management](https://cloud.google.com/iam)
 * @see [Overview of IAM Conditions][iam-conditions]
 *
 * [iam-conditions]: https://cloud.google.com/iam/docs/conditions-overview
 */
class IamBindings {
 public:
  IamBindings() = default;

  GOOGLE_CLOUD_CPP_IAM_DEPRECATED explicit IamBindings(
      // NOLINTNEXTLINE(performance-unnecessary-value-param)
      std::vector<IamBinding> bindings) {
    for (auto& it : bindings) {
      bindings_.insert({std::move(it.role()), std::move(it.members())});
    }
  }

  GOOGLE_CLOUD_CPP_IAM_DEPRECATED IamBindings(std::string role,
                                              std::set<std::string> members) {
    bindings_.insert({std::move(role), std::move(members)});
  }

  using iterator = std::map<std::string, std::set<std::string>>::const_iterator;

  /**
   * Returns an iterator referring to the first element in IamBindings
   * container.
   */
  GOOGLE_CLOUD_CPP_IAM_DEPRECATED iterator begin() const {
    return bindings_.begin();
  }

  /**
   * Returns an iterator referring to the past-the-end element in IamBindings
   * container.
   */
  GOOGLE_CLOUD_CPP_IAM_DEPRECATED iterator end() const {
    return bindings_.end();
  }

  /**
   * Returns whether the Bindings container is empty.
   *
   * @return bool whether the container is empty or not.
   */
  GOOGLE_CLOUD_CPP_IAM_DEPRECATED bool empty() const {
    return bindings_.empty();
  }

  /**
   * Return number of Bindings in container.
   *
   * @return int the size of the container.
   */
  GOOGLE_CLOUD_CPP_IAM_DEPRECATED std::size_t size() const {
    return bindings_.size();
  }

  GOOGLE_CLOUD_CPP_IAM_DEPRECATED
  std::map<std::string, std::set<std::string>> const& bindings() const {
    return bindings_;
  }

  /**
   * Finds the members for a role.
   */
  GOOGLE_CLOUD_CPP_IAM_DEPRECATED iterator find(std::string const& role) const {
    return bindings_.find(role);
  }

  /// Returns the members for a role.
  GOOGLE_CLOUD_CPP_IAM_DEPRECATED std::set<std::string> at(
      std::string const& role) const {
    auto loc = bindings_.find(role);
    if (loc == bindings_.end()) {
      return {};
    }
    return loc->second;
  }

  /**
   * Adds a new member if a binding exists with given role otherwise inserts
   * a new key-value pair of role and member to the container.
   *
   * @param role role of the new member.
   * @param member specifies the identity requesting access for a cloud
   * platform resource.
   */
  GOOGLE_CLOUD_CPP_IAM_DEPRECATED void AddMember(std::string const& role,
                                                 std::string member);

  /**
   * Adds a new key-value pair of role and members to the container if there is
   * none for the role of given binding else appends members of given binding
   * to the associated role's key-value entry.
   *
   * @param iam_binding binding representing a set of members and role for them.
   */
  GOOGLE_CLOUD_CPP_IAM_DEPRECATED void AddMembers(
      google::cloud::IamBinding const& iam_binding);

  /**
   * Adds a new key-value pair of role and members to the container if there no
   * existing for given role else appends the given members to the give role's
   * member set.
   *
   * @param role role of the member set to be added.
   * @param members a set of member which are needed to be added.
   */
  GOOGLE_CLOUD_CPP_IAM_DEPRECATED void AddMembers(
      std::string const& role, std::set<std::string> const& members);

  /**
   * Removes the given member from the given role's member set if there exists
   * one in container.
   *
   * @param role role of the member to be removed.
   * @param member specifies the identity requesting access for a cloud
   * platform resource.
   */
  GOOGLE_CLOUD_CPP_IAM_DEPRECATED void RemoveMember(std::string const& role,
                                                    std::string const& member);

  /**
   * Removes the given binding's member from the given binding's role's member
   * set if there exists one in container.
   *
   * @param iam_binding binding representing a set of members and role for them.
   */
  GOOGLE_CLOUD_CPP_IAM_DEPRECATED void RemoveMembers(
      google::cloud::IamBinding const& iam_binding);

  /**
   * Removes the given members from given role's member set if there exists one
   * in container.
   *
   * @param role role of the member set to be removed.
   * @param members a set of members which are needed to be removed.
   */
  GOOGLE_CLOUD_CPP_IAM_DEPRECATED void RemoveMembers(
      std::string const& role, std::set<std::string> const& members);

 private:
  std::map<std::string, std::set<std::string>> bindings_;
};

inline bool operator==(IamBindings const& lhs, IamBindings const& rhs) {
  return lhs.bindings() == rhs.bindings();
}

inline bool operator<(IamBindings const& lhs, IamBindings const& rhs) {
  return lhs.bindings() < rhs.bindings();
}

inline bool operator!=(IamBindings const& lhs, IamBindings const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(IamBindings const& lhs, IamBindings const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(IamBindings const& lhs, IamBindings const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(IamBindings const& lhs, IamBindings const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

std::ostream& operator<<(std::ostream& os, IamBindings const& rhs);

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#include "google/cloud/internal/diagnostics_pop.inc"

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_BINDINGS_H

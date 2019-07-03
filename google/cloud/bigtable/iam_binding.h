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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_IAM_BINDING_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_IAM_BINDING_H_

#include "google/cloud/version.h"
#include <google/bigtable/admin/v2/bigtable_instance_admin.grpc.pb.h>
#include <set>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * Create a google::iam::v1::Binding.
 *
 * @see
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Policy
 *     for more information about a IAM policies.
 *
 * @param role the role which is assigned to members
 * @param first_binding iterator pointing to the first member
 * @param last_binding iterator pointing to past last member
 *
 * @return The binding
 */
template <class InputIt>
google::iam::v1::Binding IamBinding(std::string role, InputIt first_member,
                                    InputIt last_member) {
  google::iam::v1::Binding res;
  for (auto member = first_member; member != last_member; ++member) {
    *res.add_members() = *member;
  }
  res.set_role(std::move(role));
  return res;
}

/**
 * Create a google::iam::v1::Binding.
 *
 * @see
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Policy
 *     for more information about a IAM policies.
 *
 * @param role the role which is assigned to members
 * @param members initializer_list of members
 *
 * @return The binding
 */
google::iam::v1::Binding IamBinding(std::string role,
                                    std::initializer_list<std::string> members);

/**
 * Create a google::iam::v1::Binding.
 *
 * @see
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Policy
 *     for more information about a IAM policies.
 *
 * @param role the role which is assigned to members
 * @param members vector of members
 *
 * @return The binding
 */
google::iam::v1::Binding IamBinding(std::string role,
                                    std::vector<std::string> members);

std::ostream& operator<<(std::ostream& os,
                         google::iam::v1::Binding const& binding);

/**
 * Remove all members matching a predicate from a binding.
 *
 * @param binding the binding to remove from
 * @param pred the predicate indicating whether to remove a member
 *
 * @tparam Functor the type of the predicate; it should be invocable with
 *     `string const&` and return a bool.
 *
 * @return number of members removed.
 */
template <typename Functor>
size_t RemoveMembersFromBindingIf(google::iam::v1::Binding& binding,
                                  Functor pred) {
  auto& members = *binding.mutable_members();
  auto new_end =
      std::remove_if(members.begin(), members.end(), std::move(pred));
  size_t res = std::distance(new_end, members.end());
  for (size_t i = 0; i < res; ++i) {
    members.RemoveLast();
  }
  return res;
}

/**
 * Remove all members with a given name.
 *
 * @param binding the binding to remove from
 * @param the member name to remove
 *
 * @return number of members removed.
 */
size_t RemoveMemberFromBinding(google::iam::v1::Binding& binding,
                               std::string const& name);

/**
 * Remove a specific member from a binding.
 *
 * @param binding the binding to remove from
 * @param to_remove the iterator indicating the member; it should be retrieved
 *     from the `mutable_members()` member
 */
void RemoveMemberFromBinding(
    google::iam::v1::Binding& binding,
    ::google::protobuf::RepeatedPtrField<::std::string>::iterator to_remove);

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_IAM_BINDING_H_

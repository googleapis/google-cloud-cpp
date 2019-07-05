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
 * @param begin iterator pointing to the first member
 * @param end iterator pointing to past last member
 *
 * @return The binding
 */
template <class InputIt>
google::iam::v1::Binding IamBinding(std::string role, InputIt begin,
                                    InputIt end);

/**
 * Create a google::iam::v1::Binding.
 *
 * @see
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Policy
 *     for more information about a IAM policies.
 *
 * @param role the role which is assigned to members
 * @param begin iterator pointing to the first member
 * @param end iterator pointing to past last member
 * @param condition expression indicating when the binding is effective
 *
 * @return The binding
 */
template <class InputIt>
google::iam::v1::Binding IamBinding(std::string role, InputIt begin,
                                    InputIt end, google::type::Expr condition);

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
 * @param members initializer_list of members
 * @param condition expression indicating when the binding is effective
 *
 * @return The binding
 */
google::iam::v1::Binding IamBinding(std::string role,
                                    std::initializer_list<std::string> members,
                                    google::type::Expr condition);

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

/**
 * Create a google::iam::v1::Binding.
 *
 * @see
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Policy
 *     for more information about a IAM policies.
 *
 * @param role the role which is assigned to members
 * @param members vector of members
 * @param condition expression indicating when the binding is effective
 *
 * @return The binding
 */
google::iam::v1::Binding IamBinding(std::string role,
                                    std::vector<std::string> members,
                                    google::type::Expr condition);

std::ostream& operator<<(std::ostream& os,
                         google::iam::v1::Binding const& binding);

/**
 * Append members to a google::iam::v1::Binding.
 *
 * @param binding the role which is assigned to members
 * @param begin iterator pointing to the first member
 * @param end iterator pointing to past last member
 *
 * @return The binding with appended members
 */
template <class InputIt>
google::iam::v1::Binding IamBindingAppendMembers(
    google::iam::v1::Binding binding, InputIt begin, InputIt end) {
  for (auto member = begin; member != end; ++member) {
    *binding.add_members() = *member;
  }
  return binding;
}

/**
 * Set a condition to an google::iam::v1::Binding.
 *
 * @param binding the binding to which the condition is added
 * @param condition the added condition
 *
 * @return the binding with the condition set
 */
google::iam::v1::Binding IamBindingSetCondition(
    google::iam::v1::Binding binding, google::type::Expr condition);

template <class InputIt>
google::iam::v1::Binding IamBinding(std::string role, InputIt begin,
                                    InputIt end) {
  google::iam::v1::Binding res;
  res.set_role(std::move(role));
  return IamBindingAppendMembers(res, begin, end);
}

template <class InputIt>
google::iam::v1::Binding IamBinding(std::string role, InputIt begin,
                                    InputIt end, google::type::Expr condition) {
  return IamBindingSetCondition(IamBinding(std::move(role), begin, end),
                                std::move(condition));
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_IAM_BINDING_H_

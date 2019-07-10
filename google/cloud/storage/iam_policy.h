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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_IAM_POLICY_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_IAM_POLICY_H_

#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/version.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
internal::nl::json Expression(std::string expression, std::string title = "",
                              std::string description = "",
                              std::string location = "");

/**
 * Create a json object resembling a google::iam::v1::Binding.
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
internal::nl::json IamBinding(std::string role, InputIt begin, InputIt end);

/**
 * Create a json object resembling a google::iam::v1::Binding.
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
internal::nl::json IamBinding(std::string role, InputIt begin, InputIt end,
                              internal::nl::json condition);

/**
 * Create a json object resembling a google::iam::v1::Binding.
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
internal::nl::json IamBinding(std::string role,
                              std::initializer_list<std::string> members);

/**
 * Create a json object resembling a google::iam::v1::Binding.
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
internal::nl::json IamBinding(std::string role,
                              std::initializer_list<std::string> members,
                              internal::nl::json condition);

/**
 * Create a json object resembling a google::iam::v1::Binding.
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
internal::nl::json IamBinding(std::string role,
                              std::vector<std::string> members);

/**
 * Create a json object resembling a google::iam::v1::Binding.
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
internal::nl::json IamBinding(std::string role,
                              std::vector<std::string> members,
                              internal::nl::json condition);

/**
 * Append members to a json object resembling a google::iam::v1::Binding.
 *
 * @param binding the role which is assigned to members
 * @param begin iterator pointing to the first member
 * @param end iterator pointing to past last member
 *
 * @return The binding with appended members
 */
template <class InputIt>
internal::nl::json IamBindingAppendMembers(internal::nl::json binding,
                                           InputIt begin, InputIt end) {
  for (auto it = begin; it != end; ++it) {
    binding["members"].emplace_back(*it);
  }
  return binding;
}

/**
 * Set a condition to a json object resembling a google::iam::v1::Binding.
 *
 * @param binding the binding to which the condition is added
 * @param condition the added condition
 *
 * @return the binding with the condition set
 */
internal::nl::json IamBindingSetCondition(internal::nl::json binding,
                                          internal::nl::json condition);

template <class InputIt>
internal::nl::json IamBinding(std::string role, InputIt begin, InputIt end) {
  internal::nl::json res{{"role", std::move(role)}};
  return IamBindingAppendMembers(res, begin, end);
}

/**
 * Create a json object resembling a google::iam::v1::Policy.
 *
 * @see
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Policy
 *     for more information about a IAM policies.
 *
 * @see https://tools.ietf.org/html/rfc7232#section-2.3 for more information
 *     about ETags.
 *
 * @warning ETags are currently not used by Cloud Storage.
 *
 * @param begin iterator pointing to the first json resembling a
 *     google::iam::v1::Binding
 * @param end iterator pointing to past last json resembling a
 *     google::iam::v1::Binding
 * @param etag used for optimistic concurrency control
 * @param version currently unused
 *
 *
 * @return The json object representing the policy.
 */
template <class InputIt>
internal::nl::json IamPolicy(InputIt begin, InputIt end, std::string etag = "",
                             std::int32_t version = 0);

/**
 * Create a json object resembling a google::iam::v1::Policy.
 *
 * @see
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Policy
 *     for more information about a IAM policies.
 *
 * @see https://tools.ietf.org/html/rfc7232#section-2.3 for more information
 *     about ETags.
 *
 * @warning ETags are currently not used by Cloud Bigtable.
 *
 * @param bindings initializer_list of google::iam::v1::Binding
 * @param etag used for optimistic concurrency control
 * @param version currently unused
 *
 * @return The policy
 */
internal::nl::json IamPolicy(std::initializer_list<internal::nl::json> bindings,
                             std::string etag = "", std::int32_t version = 0);

/**
 * Create a json object resembling a google::iam::v1::Policy.
 *
 * @see
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Policy
 *     for more information about a IAM policies.
 *
 * @see https://tools.ietf.org/html/rfc7232#section-2.3 for more information
 *     about ETags.
 *
 * @warning ETags are currently not used by Cloud Bigtable.
 *
 * @param bindings vector of google::iam::v1::Binding
 * @param etag used for optimistic concurrency control
 * @param version currently unused
 *
 * @return The policy
 */
internal::nl::json IamPolicy(std::vector<internal::nl::json> bindings,
                             std::string etag = "", std::int32_t version = 0);

/**
 * Append bindings to a json object resembling a google::iam::v1::Policy.
 *
 * @param policy the policy to which bindings are appended
 * @param begin iterator pointing to the first binding
 * @param end iterator pointing to past last binding
 *
 * @return The policy with appended bindings
 */
template <class InputIt>
internal::nl::json IamPolicyAppendBindings(internal::nl::json policy,
                                           InputIt begin, InputIt end) {
  for (auto it = begin; it != end; ++it) {
    policy["bindings"].emplace_back(*it);
  }
  return policy;
}

template <class InputIt>
internal::nl::json IamBinding(std::string role, InputIt begin, InputIt end,
                              internal::nl::json condition) {
  return IamBindingSetCondition(IamBinding(std::move(role), begin, end),
                                std::move(condition));
}

template <class InputIt>
internal::nl::json IamPolicy(InputIt begin, InputIt end, std::string etag,
                             std::int32_t version) {
  internal::nl::json res{{"kind", "storage#policy"}, {"version", version}};
  if (!etag.empty()) {
    res["etag"] = etag;
  }
  return IamPolicyAppendBindings(std::move(res), begin, end);
}

}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_IAM_POLICY_H_

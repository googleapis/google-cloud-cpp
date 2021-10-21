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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_IAM_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_IAM_POLICY_H

#include "google/cloud/bigtable/iam_binding.h"
#include "google/cloud/bigtable/version.h"
#include <list>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
template <class InputIt>
/**
 * Create a google::iam::v1::Policy.
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
 * @param first_binding iterator pointing to the first google::iam::v1::Binding
 * @param last_binding iterator pointing to past last google::iam::v1::Binding
 * @param etag used for optimistic concurrency control
 * @param version currently unused
 *
 *
 * @return The policy
 */
google::iam::v1::Policy IamPolicy(InputIt first_binding, InputIt last_binding,
                                  std::string etag = "",
                                  std::int32_t version = 0) {
  google::iam::v1::Policy res;
  for (auto binding = first_binding; binding != last_binding; ++binding) {
    *res.add_bindings() = *binding;
  }
  res.set_version(version);
  res.set_etag(std::move(etag));
  return res;
}

/**
 * Create a google::iam::v1::Policy.
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
google::iam::v1::Policy IamPolicy(
    std::initializer_list<google::iam::v1::Binding> bindings,
    std::string etag = "", std::int32_t version = 0);

/**
 * Create a google::iam::v1::Policy.
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
google::iam::v1::Policy IamPolicy(
    std::vector<google::iam::v1::Binding> bindings, std::string etag = "",
    std::int32_t version = 0);

std::ostream& operator<<(std::ostream& os, google::iam::v1::Policy const& rhs);

/**
 * Remove all bindings matching a predicate from a policy.
 *
 * @param policy the policy to remove from
 * @param pred predicate indicating whether to remove a binding
 *
 * @tparam Functor the type of the predicate; it should be invocable with
 *     `google::iam::v1::Binding const&` and return a bool.
 *
 * @return number of bindings removed.
 */
template <typename Functor>
size_t RemoveBindingsFromPolicyIf(google::iam::v1::Policy& policy,
                                  Functor pred) {
  auto& bindings = *policy.mutable_bindings();
  auto new_end =
      std::remove_if(bindings.begin(), bindings.end(), std::move(pred));
  size_t res = std::distance(new_end, bindings.end());
  for (size_t i = 0; i < res; ++i) {
    bindings.RemoveLast();
  }
  return res;
}

/**
 * Remove a specific binding from a policy.
 *
 * @param policy the policy to remove from
 * @param to_remove the iterator indicating the binding; it should be retrieved
 *     from the `mutable_bindings()` member
 */
void RemoveBindingFromPolicy(
    google::iam::v1::Policy& policy,
    google::protobuf::RepeatedPtrField<google::iam::v1::Binding>::iterator
        to_remove);

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_IAM_POLICY_H

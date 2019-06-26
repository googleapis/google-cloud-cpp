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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_IAM_POLICY_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_IAM_POLICY_H_

#include "google/cloud/bigtable/iam_binding.h"
#include <list>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * Represent the result of a GetIamPolicy or SetIamPolicy request.
 *
 * @see
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Policy
 *     for more information about a AIM policies.
 *
 * @see https://tools.ietf.org/html/rfc7232#section-2.3 for more information
 *     about ETags.
 *
 * Compared to `IamPolicy`, `NativeIamPolicy` is a more future-proof
 * solution - it gracefully tolerates changes in the underlying protocol.
 * If IamPolicy is extended with additional fields in the future,
 * `NativeIamPolicy` will preserve them (contrary to IamPolicy).
 */
class NativeIamPolicy {
 public:
  NativeIamPolicy() = default;
  explicit NativeIamPolicy(std::list<NativeIamBinding> bindings,
                           std::int32_t version = 0,
                           std::string const& etag = "");
  explicit NativeIamPolicy(google::iam::v1::Policy impl);

  /**
   * Remove all bindings for the given role.
   *
   * @param role role to be removed from bindings.
   */
  void RemoveAllBindingsByRole(std::string const& role);

  /**
   * Remove the given member from all bindings.
   *
   * @param member specifies the identity requesting access for a cloud
   * platform resource.
   */
  void RemoveMemberFromAllBindings(std::string const& member);

  std::int32_t version() const { return impl_.version(); }
  void set_version(std::int32_t version) { impl_.set_version(version); }
  std::string const& etag() const { return impl_.etag(); }
  void set_etag(std::string etag) { impl_.set_etag(std::move(etag)); }
  std::list<NativeIamBinding>& bindings() { return bindings_; }
  std::list<NativeIamBinding> const& bindings() const { return bindings_; }

  google::iam::v1::Policy ToProto() &&;
  google::iam::v1::Policy ToProto() const& {
    return NativeIamPolicy(*this).ToProto();
  }

 private:
  google::iam::v1::Policy impl_;
  std::list<NativeIamBinding> bindings_;
  friend bool operator==(NativeIamPolicy const& lhs,
                         NativeIamPolicy const& rhs);
};

bool operator==(NativeIamPolicy const& lhs, NativeIamPolicy const& rhs);

inline bool operator!=(NativeIamPolicy const& lhs, NativeIamPolicy const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, NativeIamPolicy const& rhs);

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_IAM_POLICY_H_

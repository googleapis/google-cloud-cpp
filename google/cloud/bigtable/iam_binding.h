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
 * Represents a Binding which associates a `member` with a particular `role`
 * which can be used for Identity and Access management for Cloud Platform
 * Resources.
 *
 * For more information about a Binding please refer to:
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Binding
 *
 * Compared to `IamBinding`, `NativeIamBinding` is a more future-proof
 * solution - it gracefully tolerates changes in the underlying protocol.
 * If IamBinding contains more fields than just a role and members, in the
 * future, `NativeIamBinding` will preserve them (contrary to IamBinding).
 */
class NativeIamBinding {
 public:
  NativeIamBinding(std::string role, std::set<std::string> members);
  explicit NativeIamBinding(google::iam::v1::Binding impl);

  std::string const& role() const { return impl_.role(); };
  void set_role(std::string const& role) { impl_.set_role(role); };
  std::set<std::string> const& members() const { return members_; }
  std::set<std::string>& members() { return members_; }
  google::iam::v1::Binding ToProto() &&;
  google::iam::v1::Binding ToProto() const& {
    return NativeIamBinding(*this).ToProto();
  }

 private:
  google::iam::v1::Binding impl_;
  std::set<std::string> members_;
  friend bool operator==(NativeIamBinding const& lhs,
                         NativeIamBinding const& rhs);
};

bool operator==(NativeIamBinding const& lhs, NativeIamBinding const& rhs);

inline bool operator!=(NativeIamBinding const& lhs,
                       NativeIamBinding const& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, NativeIamBinding const& binding);

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_IAM_BINDING_H_

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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_BINDING_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_BINDING_H

#include "google/cloud/version.h"
#include <set>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * Simplified view of roles and members for IAM.
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
class IamBinding {
 public:
  GOOGLE_CLOUD_CPP_IAM_DEPRECATED IamBinding(std::string role,
                                             std::set<std::string> members)
      : role_(std::move(role)), members_(std::move(members)) {}

  std::string const& role() const { return role_; };
  std::set<std::string> const& members() const { return members_; };

 private:
  std::string role_;
  std::set<std::string> members_;
};

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_BINDING_H

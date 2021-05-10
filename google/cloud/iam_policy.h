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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_POLICY_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_POLICY_H

#include "google/cloud/iam_bindings.h"
#include "google/cloud/version.h"
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * A deprecated representation of IAM policies.
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
struct IamPolicy {
  std::int32_t version;
  IamBindings bindings;
  std::string etag;
};

inline bool operator==(IamPolicy const& lhs, IamPolicy const& rhs) {
  return std::tie(lhs.version, lhs.bindings, lhs.etag) ==
         std::tie(rhs.version, rhs.bindings, rhs.etag);
}

inline bool operator<(IamPolicy const& lhs, IamPolicy const& rhs) {
  return std::tie(lhs.version, lhs.bindings, lhs.etag) <
         std::tie(rhs.version, rhs.bindings, rhs.etag);
}

inline bool operator!=(IamPolicy const& lhs, IamPolicy const& rhs) {
  return std::rel_ops::operator!=(lhs, rhs);
}

inline bool operator>(IamPolicy const& lhs, IamPolicy const& rhs) {
  return std::rel_ops::operator>(lhs, rhs);
}

inline bool operator<=(IamPolicy const& lhs, IamPolicy const& rhs) {
  return std::rel_ops::operator<=(lhs, rhs);
}

inline bool operator>=(IamPolicy const& lhs, IamPolicy const& rhs) {
  return std::rel_ops::operator>=(lhs, rhs);
}

std::ostream& operator<<(std::ostream& os, IamPolicy const& rhs);

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_POLICY_H

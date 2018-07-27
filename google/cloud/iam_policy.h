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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_POLICY_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_POLICY_H_

#include "google/cloud/iam_bindings.h"

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * Represent the result of a GetIamPolicy or SetIamPolicy request.
 *
 * This is a simple struct to hold the version, IamBindings, and ETag.
 *
 * @see
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Policy
 *     for more information about a AIM policies.
 *
 * @see https://tools.ietf.org/html/rfc7232#section-2.3 for more information
 *     about ETags.
 */
struct IamPolicy {
  std::int32_t version;
  IamBindings bindings;
  std::string etag;
};

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_IAM_POLICY_H_

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

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
/**
 * Represents a Binding which associates a `member` with a particular `role`
 * which can be used for Identity and Access management for Cloud Platform
 * Resources.
 *
 * For more information about a Binding please refer to:
 * https://cloud.google.com/resource-manager/reference/rest/Shared.Types/Binding
 */
class IamBinding {
 public:
  IamBinding(std::string role, std::set<std::string> members)
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

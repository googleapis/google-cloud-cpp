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

#include "google/cloud/iam_policy.h"
#include <iostream>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
std::ostream& operator<<(std::ostream& os, IamPolicy const& rhs) {
  return os << "IamPolicy={version=" << rhs.version
            << ", bindings=" << rhs.bindings << ", etag=" << rhs.etag << "}";
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

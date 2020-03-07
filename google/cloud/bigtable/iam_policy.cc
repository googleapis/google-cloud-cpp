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

#include "google/cloud/bigtable/iam_policy.h"
#include <google/protobuf/util/message_differencer.h>
#include <iostream>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
google::iam::v1::Policy IamPolicy(
    std::initializer_list<google::iam::v1::Binding> bindings, std::string etag,
    std::int32_t version) {
  return IamPolicy(bindings.begin(), bindings.end(), std::move(etag), version);
}

google::iam::v1::Policy IamPolicy(
    std::vector<google::iam::v1::Binding> bindings, std::string etag,
    std::int32_t version) {
  google::iam::v1::Policy res;
  for (auto& binding : bindings) {
    *res.add_bindings() = std::move(binding);
  }
  res.set_version(version);
  res.set_etag(std::move(etag));
  return res;
}

std::ostream& operator<<(std::ostream& os, google::iam::v1::Policy const& rhs) {
  os << "IamPolicy={version=" << rhs.version() << ", bindings="
     << "IamBindings={";
  bool first = true;
  for (auto const& binding : rhs.bindings()) {
    os << (first ? "" : ", ") << binding;
    first = false;
  }
  return os << "}, etag=" << rhs.etag() << "}";
}

void RemoveBindingFromPolicy(
    google::iam::v1::Policy& policy,
    google::protobuf::RepeatedPtrField<google::iam::v1::Binding>::iterator
        to_remove) {
  RemoveBindingsFromPolicyIf(
      policy, [&to_remove](google::iam::v1::Binding const& binding) {
        return &(*to_remove) == &binding;
      });
}

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

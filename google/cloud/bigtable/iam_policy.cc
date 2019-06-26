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
inline namespace GOOGLE_CLOUD_CPP_NS {
NativeIamPolicy::NativeIamPolicy(std::list<NativeIamBinding> bindings,
                                 std::int32_t version, std::string const& etag)
    : bindings_(std::move(bindings)) {
  impl_.set_version(version);
  impl_.set_etag(etag);
}

NativeIamPolicy::NativeIamPolicy(google::iam::v1::Policy impl)
    : impl_(std::move(impl)) {
  for (auto& binding : impl_.bindings()) {
    bindings_.emplace_back(NativeIamBinding(binding));
  }
  impl_.clear_bindings();
}

void NativeIamPolicy::RemoveAllBindingsByRole(std::string const& role) {
  for (auto binding_it = bindings_.begin(); binding_it != bindings_.end();) {
    if (binding_it->role() == role) {
      bindings_.erase(binding_it++);
    } else {
      ++binding_it;
    }
  }
}

void NativeIamPolicy::RemoveMemberFromAllBindings(std::string const& member) {
  for (auto binding_it = bindings_.begin(); binding_it != bindings_.end();) {
    if (binding_it->members().erase(member) > 0 &&
        binding_it->members().empty()) {
      // Remove the binding if it doesn't hold any members now.
      bindings_.erase(binding_it++);
    } else {
      ++binding_it;
    }
  }
}

google::iam::v1::Policy NativeIamPolicy::ToProto() && {
  google::iam::v1::Policy res(std::move(impl_));
  for (auto& binding : bindings_) {
    *res.add_bindings() = std::move(binding).ToProto();
  }
  return res;
}

bool operator==(NativeIamPolicy const& lhs, NativeIamPolicy const& rhs) {
  return google::protobuf::util::MessageDifferencer::Equals(lhs.impl_,
                                                            rhs.impl_) &&
         lhs.bindings_ == rhs.bindings_;
}

std::ostream& operator<<(std::ostream& os, NativeIamPolicy const& rhs) {
  os << "NativeIamPolicy={version=" << rhs.version() << ", bindings="
     << "NativeIamBindings={";
  bool first = true;
  for (auto const& binding : rhs.bindings()) {
    os << (first ? "" : ", ") << binding;
    first = false;
  }
  return os << "}, etag=" << rhs.etag() << "}";
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

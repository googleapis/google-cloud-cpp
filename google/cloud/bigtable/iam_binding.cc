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

#include "google/cloud/bigtable/iam_binding.h"
#include <google/protobuf/util/message_differencer.h>
#include <algorithm>
#include <iostream>
#include <iterator>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace GOOGLE_CLOUD_CPP_NS {
NativeIamBinding::NativeIamBinding(google::iam::v1::Binding impl)
    : impl_(std::move(impl)),
      members_(impl_.members().begin(), impl_.members().end()) {
  impl_.clear_members();
}

NativeIamBinding::NativeIamBinding(std::string role,
                                   std::set<std::string> members)
    : members_(std::move(members)) {
  impl_.set_role(std::move(role));
}

std::ostream& operator<<(std::ostream& os, NativeIamBinding const& binding) {
  os << binding.role() << ": [";
  bool first = true;
  for (auto const& member : binding.members()) {
    os << (first ? "" : ", ") << member;
    first = false;
  }
  return os << "]";
}

google::iam::v1::Binding NativeIamBinding::ToProto() && {
  google::iam::v1::Binding res(std::move(impl_));
  for (auto const& member : members_) {
    res.add_members(member);
  }
  return res;
}

bool operator==(NativeIamBinding const& lhs, NativeIamBinding const& rhs) {
  return google::protobuf::util::MessageDifferencer::Equals(lhs.impl_,
                                                            rhs.impl_) &&
         lhs.members_ == rhs.members_;
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

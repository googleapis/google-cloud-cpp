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

#include "google/cloud/spanner/instance.h"
#include <array>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

Instance::Instance(std::string const& project_id,
                   std::string const& instance_id)
    : full_name_("projects/" + project_id + "/instances/" + instance_id) {}

std::string Instance::FullName() const { return full_name_; }

std::string Instance::InstanceId() const {
  auto pos = full_name_.rfind("/instances/");
  return full_name_.substr(pos + sizeof("/instances/") - 1);
}

bool operator==(Instance const& a, Instance const& b) {
  return a.full_name_ == b.full_name_;
}

bool operator!=(Instance const& a, Instance const& b) { return !(a == b); }

std::ostream& operator<<(std::ostream& os, Instance const& dn) {
  return os << dn.full_name_;
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

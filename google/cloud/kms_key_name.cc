// Copyright 2020 Google LLC
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

#include "google/cloud/kms_key_name.h"
#include <regex>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {

StatusOr<KmsKeyName> MakeKmsKeyName(std::string full_name) {
  std::regex re(
      "projects/[^/]+/locations/[^/]+/keyRings/[^/]+/cryptoKeys/[^/]+");
  if (!std::regex_match(full_name, re)) {
    return Status(StatusCode::kInvalidArgument,
                  "Improperly formatted KmsKeyName: " + full_name);
  }
  return KmsKeyName(std::move(full_name));
}

KmsKeyName::KmsKeyName(std::string const& project_id,
                       std::string const& location, std::string const& key_ring,
                       std::string const& kms_key_name)
    : full_name_("projects/" + project_id + "/locations/" + location +
                 "/keyRings/" + key_ring + "/cryptoKeys/" + kms_key_name) {}

bool operator==(KmsKeyName const& a, KmsKeyName const& b) {
  return a.full_name_ == b.full_name_;
}

std::ostream& operator<<(std::ostream& os, KmsKeyName const& key) {
  return os << key.FullName();
}

}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/storage/well_known_parameters.h"
#include <map>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
std::string PredefinedAcl::HeaderName() const {
  static std::map<std::string, std::string> const kMapping{
      {"authenticatedRead", "authenticated-read"},
      {"bucketOwnerFullControl", "bucket-owner-full-control"},
      {"bucketOwnerRead", "bucket-owner-read"},
      {"private", "private"},
      {"projectPrivate", "project-private"},
      {"publicRead", "public-read"},
  };
  auto loc = kMapping.find(value());
  if (loc == kMapping.end()) {
    return value();
  }
  return loc->second;
}
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

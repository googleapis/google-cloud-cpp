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

#include "google/cloud/storage/internal/common_metadata.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/storage/internal/nljson.h"

namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
CommonMetadata CommonMetadata::ParseFromJson(std::string const& payload) {
  auto json = nl::json::parse(payload);
  return MetadataParser::ParseCommonMetadata(json);
}

bool CommonMetadata::operator==(CommonMetadata const& rhs) const {
  // etag changes each time the metadata changes, so that is the best field
  // to short-circuit this comparison.  The check the name, project number, and
  // metadata generation, which have the next best chance to short-circuit.  The
  // The rest just put in alphabetical order.
  return name_ == rhs.name_ and project_number_ == rhs.project_number_ and
         metadata_generation_ == rhs.metadata_generation_ and id_ == rhs.id_ and
         etag_ == rhs.etag_ and kind_ == rhs.kind_ and
         location_ == rhs.location_ and self_link_ == rhs.self_link_ and
         storage_class_ == rhs.storage_class_ and
         time_created_ == rhs.time_created_ and
         time_updated_ == rhs.time_updated_;
}

std::ostream& operator<<(std::ostream& os, CommonMetadata const& rhs) {
  // TODO(#536) - convert back to JSON for a nicer format.
  return os << "etag=" << rhs.etag() << ", id=" << rhs.id()
            << ", kind=" << rhs.kind() << ", location=" << rhs.location()
            << ", metadata_generation=" << rhs.metadata_generation()
            << ", name=" << rhs.name()
            << ", project_number=" << rhs.project_number()
            << ", self_link=" << rhs.self_link()
            << ", storage_class=" << rhs.storage_class() << ", time_created="
            << rhs.time_created().time_since_epoch().count()
            << ", time_updated="
            << rhs.time_updated().time_since_epoch().count();
}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

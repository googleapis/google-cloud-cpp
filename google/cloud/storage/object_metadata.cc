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

#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/storage/internal/nljson.h"

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
ObjectMetadata ObjectMetadata::ParseFromJson(std::string const& payload) {
  auto json = storage::internal::nl::json::parse(payload);
  ObjectMetadata result{};
  static_cast<CommonMetadata&>(result) =
      internal::MetadataParser::ParseCommonMetadata(json);
  result.bucket_ = json.value("bucket", "");
  result.generation_ =
      internal::MetadataParser::ParseLongField(json, "generation");
  if (json.count("metadata") > 0) {
    for (auto const& kv : json["metadata"].items()) {
      result.metadata_.emplace(kv.key(), kv.value().get<std::string>());
    }
  }
  return result;
}

bool ObjectMetadata::operator==(ObjectMetadata const& rhs) const {
  return static_cast<internal::CommonMetadata const&>(*this) == rhs and
         metadata_ == rhs.metadata_;
}

std::ostream& operator<<(std::ostream& os, ObjectMetadata const& rhs) {
  // TODO(#536) - convert back to JSON for a nicer format.
  os << static_cast<internal::CommonMetadata const&>(rhs)
     << ", bucket=" << rhs.bucket_ << ", generation_=" << rhs.generation_
     << ", metadata={";
  char const* sep = "metadata.";
  for (auto const& kv : rhs.metadata_) {
    os << sep << kv.first << "=" << kv.second;
    sep = ", metadata.";
  }
  return os << "}";
}
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/internal/metadata_parser.h"
#include "google/cloud/storage/internal/nljson.h"

namespace storage {
inline namespace STORAGE_CLIENT_NS {
BucketMetadata BucketMetadata::ParseFromJson(std::string const& payload) {
  auto json = storage::internal::nl::json::parse(payload);
  BucketMetadata result{};
  static_cast<CommonMetadata&>(result) =
      internal::MetadataParser::ParseCommonMetadata(json);
  if (json.count("labels") > 0) {
    for (auto const& kv : json["labels"].items()) {
      result.labels_.emplace(kv.key(), kv.value().get<std::string>());
    }
  }
  return result;
}

bool BucketMetadata::operator==(BucketMetadata const& rhs) const {
  return static_cast<internal::CommonMetadata const&>(*this) == rhs and
         labels_ == rhs.labels_;
}

std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs) {
  // TODO(#536) - convert back to JSON for a nicer format.
  os << static_cast<internal::CommonMetadata const&>(rhs) << ", labels={";
  char const* sep = "labels.";
  for (auto const& kv : rhs.labels_) {
    os << sep << kv.first << "=" << kv.second;
    sep = ", labels.";
  }
  return os << "}";
}

constexpr char BucketMetadata::STORAGE_CLASS_STANDARD[];
constexpr char BucketMetadata::STORAGE_CLASS_MULTI_REGIONAL[];
constexpr char BucketMetadata::STORAGE_CLASS_REGIONAL[];
constexpr char BucketMetadata::STORAGE_CLASS_NEARLINE[];
constexpr char BucketMetadata::STORAGE_CLASS_COLDLINE[];
constexpr char BucketMetadata::STORAGE_CLASS_DURABLE_REDUCED_AVAILABILITY[];
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

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

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
BucketMetadata BucketMetadata::ParseFromJson(internal::nl::json const& json) {
  BucketMetadata result{};
  static_cast<CommonMetadata<BucketMetadata>&>(result) =
      CommonMetadata<BucketMetadata>::ParseFromJson(json);
  result.location_ = json.value("location", "");
  result.project_number_ = internal::ParseLongField(json, "projectNumber");
  if (json.count("labels") > 0) {
    for (auto const& kv : json["labels"].items()) {
      result.labels_.emplace(kv.key(), kv.value().get<std::string>());
    }
  }
  return result;
}

BucketMetadata BucketMetadata::ParseFromString(std::string const& payload) {
  auto json = storage::internal::nl::json::parse(payload);
  return ParseFromJson(json);
}

bool BucketMetadata::operator==(BucketMetadata const& rhs) const {
  return static_cast<internal::CommonMetadata<BucketMetadata> const&>(*this) ==
             rhs and
         project_number_ == rhs.project_number_ and
         location_ == rhs.location_ and labels_ == rhs.labels_;
}

std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs) {
  // TODO(#536) - convert back to JSON for a nicer format.
  os << "BucketMetadata={name=" << rhs.name() << ", etag=" << rhs.etag()
     << ", id=" << rhs.id() << ", kind=" << rhs.kind();
  char const* sep = "labels.";
  for (auto const& kv : rhs.labels_) {
    os << sep << kv.first << "=" << kv.second;
    sep = ", labels.";
  }
  os << ", location=" << rhs.location()
     << ", metageneration=" << rhs.metageneration() << ", name=" << rhs.name()
     << ", self_link=" << rhs.self_link()
     << ", storage_class=" << rhs.storage_class()
     << ", time_created=" << rhs.time_created().time_since_epoch().count()
     << ", updated=" << rhs.updated().time_since_epoch().count() << "}";
  return os;
}

constexpr char BucketMetadata::STORAGE_CLASS_STANDARD[];
constexpr char BucketMetadata::STORAGE_CLASS_MULTI_REGIONAL[];
constexpr char BucketMetadata::STORAGE_CLASS_REGIONAL[];
constexpr char BucketMetadata::STORAGE_CLASS_NEARLINE[];
constexpr char BucketMetadata::STORAGE_CLASS_COLDLINE[];
constexpr char BucketMetadata::STORAGE_CLASS_DURABLE_REDUCED_AVAILABILITY[];
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

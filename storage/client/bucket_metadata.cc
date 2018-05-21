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

#include "storage/client/bucket_metadata.h"
#include "storage/client/internal/nljson.h"
#include "storage/client/internal/parse_rfc3339.h"

namespace storage {
inline namespace STORAGE_CLIENT_NS {
BucketMetadata BucketMetadata::ParseFromJson(std::string const& payload) {
  auto json = storage::internal::nl::json::parse(payload);
  BucketMetadata result{};
  result.etag_ = json["etag"].get<std::string>();
  result.id_ = json["id"].get<std::string>();
  result.kind_ = json["kind"].get<std::string>();
  if (json.count("labels") > 0) {
    auto labels = json["labels"];
    for (auto kv = labels.begin(); kv != labels.end(); ++kv) {
      result.labels_.emplace(kv.key(), kv.value().get<std::string>());
    }
  }
  result.location_ = json["location"].get<std::string>();
  result.metadata_generation_ =
      std::stoll(json["metageneration"].get_ref<std::string const&>());
  result.name_ = json["name"].get<std::string>();
  result.project_number_ =
      std::stoll(json["projectNumber"].get_ref<std::string const&>());
  result.self_link_ = json["selfLink"].get<std::string>();
  result.storage_class_ = json["storageClass"];
  result.time_created_ = storage::internal::ParseRfc3339(json["timeCreated"]);
  result.time_updated_ = storage::internal::ParseRfc3339(json["updated"]);
  return result;
}

bool BucketMetadata::operator==(BucketMetadata const& rhs) const {
  return etag_ == rhs.etag_ and id_ == rhs.id_ and kind_ == rhs.kind_ and
         labels_ == rhs.labels_ and location_ == rhs.location_ and
         metadata_generation_ == rhs.metadata_generation_ and
         name_ == rhs.name_ and project_number_ == rhs.project_number_ and
         self_link_ == rhs.self_link_ and
         storage_class_ == rhs.storage_class_ and
         time_created_ == rhs.time_created_ and
         time_updated_ == rhs.time_updated_;
}

std::ostream& operator<<(std::ostream& os, BucketMetadata const& rhs) {
  // TODO() - convert back to JSON for a nicer format.
  os << "etag=" << rhs.etag() << ", id=" << rhs.id() << ", kind=" << rhs.kind();
  os << ", labels={";
  char const* sep = "";
  for (auto const& kv : rhs.labels_) {
    os << sep << kv.first << " : " << kv.second;
    sep = ",";
  }
  return os << "}, location=" << rhs.location()
            << ", metadata_generation=" << rhs.metadata_generation()
            << ", name=" << rhs.name()
            << ", project_number=" << rhs.project_number()
            << ", self_link=" << rhs.self_link()
            << ", storage_class=" << rhs.storage_class() << ", time_created="
            << rhs.time_created().time_since_epoch().count()
            << ", time_updated="
            << rhs.time_updated().time_since_epoch().count();
}

constexpr char BucketMetadata::STORAGE_CLASS_STANDARD[];
constexpr char BucketMetadata::STORAGE_CLASS_MULTI_REGIONAL[];
constexpr char BucketMetadata::STORAGE_CLASS_REGIONAL[];
constexpr char BucketMetadata::STORAGE_CLASS_NEARLINE[];
constexpr char BucketMetadata::STORAGE_CLASS_COLDLINE[];
constexpr char BucketMetadata::STORAGE_CLASS_DURABLE_REDUCED_AVAILABILITY[];
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

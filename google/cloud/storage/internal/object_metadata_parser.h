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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_METADATA_PARSER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_METADATA_PARSER_H

#include "google/cloud/storage/object_metadata.h"
#include "google/cloud/status.h"
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
struct ObjectMetadataParser {
  static StatusOr<ObjectMetadata> FromJson(nlohmann::json const& json);
  static StatusOr<ObjectMetadata> FromString(std::string const& payload);
};

//@{
/**
 * @name Create the correct JSON payload depending on the operation.
 *
 * Depending on the specific operation being performed the JSON object sent to
 * the server needs to exclude different fields. We handle this by having
 * different functions for each operation, though their implementations are
 * shared.
 */
nlohmann::json ObjectMetadataJsonForCompose(ObjectMetadata const& meta);
nlohmann::json ObjectMetadataJsonForCopy(ObjectMetadata const& meta);
nlohmann::json ObjectMetadataJsonForInsert(ObjectMetadata const& meta);
nlohmann::json ObjectMetadataJsonForRewrite(ObjectMetadata const& meta);
nlohmann::json ObjectMetadataJsonForUpdate(ObjectMetadata const& meta);
//@}

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_OBJECT_METADATA_PARSER_H

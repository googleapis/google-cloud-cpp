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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_METADATA_PARSER_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_METADATA_PARSER_H_

#include "google/cloud/storage/internal/common_metadata.h"
#include "google/cloud/storage/internal/nljson.h"

namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
/**
 * Helper functions to parse JSON objects.
 *
 * These functions are defined inside a class so they can be granted 'friend'
 * status without having to declare them. We want to avoid exposing nl::json
 * in public headers if we can.
 */
struct MetadataParser {
  static CommonMetadata ParseCommonMetadata(nl::json const& json);

  /**
   * Parse a long integer field, even if it is represented by a string type in
   * the JSON object.
   */
  static std::int64_t ParseLongField(nl::json const& json,
                                     char const* field_name);

  /**
   * Parse a RFC 3339 timestamp.
   */
  static std::chrono::system_clock::time_point ParseTimestampField(
      nl::json const& json, char const* field);
};

}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_METADATA_PARSER_H_

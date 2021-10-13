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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_METADATA_PARSER_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_METADATA_PARSER_H

#include "google/cloud/storage/version.h"
#include "google/cloud/status_or.h"
#include <nlohmann/json.hpp>
#include <chrono>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
/**
 * Parses a boolean field, even if it is represented by a string type in the
 * JSON object.
 *
 * @return the value of @p field_name in @p json, or `false` if the field is not
 * present.
 */
StatusOr<bool> ParseBoolField(nlohmann::json const& json,
                              char const* field_name);

/**
 * Parses an integer field, even if it is represented by a string type in the
 * JSON object.
 */
StatusOr<std::int32_t> ParseIntField(nlohmann::json const& json,
                                     char const* field_name);

/**
 * Parses an unsigned integer field, even if it is represented by a string type
 * in the JSON object.
 */
StatusOr<std::uint32_t> ParseUnsignedIntField(nlohmann::json const& json,
                                              char const* field_name);

/**
 * Parses a long integer field, even if it is represented by a string type in
 * the JSON object.
 *
 * @return the value of @p field_name in @p json, or `0` if the field is not
 * present.
 */
StatusOr<std::int64_t> ParseLongField(nlohmann::json const& json,
                                      char const* field_name);

/**
 * Parses an unsigned long integer field, even if it is represented by a string
 * type in the JSON object.
 *
 * @return the value of @p field_name in @p json, or `0` if the field is not
 * present.
 */
StatusOr<std::uint64_t> ParseUnsignedLongField(nlohmann::json const& json,
                                               char const* field_name);

/**
 * Parses a RFC 3339 timestamp.
 *
 * @return the value of @p field_name in @p json, or the epoch if the field is
 * not present.
 */
StatusOr<std::chrono::system_clock::time_point> ParseTimestampField(
    nlohmann::json const& json, char const* field_name);

}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_STORAGE_INTERNAL_METADATA_PARSER_H

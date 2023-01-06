// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_JSON_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_JSON_H

#include "google/cloud/spanner/version.h"
#include <ostream>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A simple representation for the Spanner JSON type: a lightweight,
 * text-based, language-independent data interchange format. JSON (the
 * JavaScript Object Notation) defines a small set of formatting rules
 * for the portable representation of structured data. See RFC 7159.
 *
 * A `Json` value can be constructed from, and converted to a `std::string`.
 * `Json` values can be compared (by string) for equality, and streamed.
 *
 * There is no syntax checking of JSON strings in this interface. The user
 * is expected to only construct `Json` values from well-formatted strings.
 */
class Json {
 public:
  /// A null value.
  Json() : rep_("null") {}

  /// @name Regular value type, supporting copy, assign, move.
  ///@{
  Json(Json const&) = default;
  Json& operator=(Json const&) = default;
  Json(Json&&) = default;
  Json& operator=(Json&&) = default;
  ///@}

  /**
   * Construction from a JSON-formatted string. Note that there is no check
   * here that the argument string is indeed well-formatted. Error detection
   * will be delayed until the value is passed to Spanner.
   */
  explicit Json(std::string s) : rep_(std::move(s)) {}

  /// @name Conversion to a JSON-formatted string.
  ///@{
  explicit operator std::string() const& { return rep_; }
  explicit operator std::string() && { return std::move(rep_); }
  ///@}

 private:
  std::string rep_;  // a (presumably) JSON-formatted string
};

/// @name Relational operators
///@{
inline bool operator==(Json const& lhs, Json const& rhs) {
  return std::string(lhs) == std::string(rhs);
}
inline bool operator!=(Json const& lhs, Json const& rhs) {
  return !(lhs == rhs);
}
///@}

/// Outputs a JSON-formatted string to the provided stream.
inline std::ostream& operator<<(std::ostream& os, Json const& json) {
  return os << std::string(json);
}

/**
 * `JsonB` is a variant of `Json` (see above). While both classes share the
 * same, thin client-side API, `JsonB` stores the data in a decomposed,
 * binary format, whereas `Json` stores an exact copy of the RFC 7159 text.
 *
 * This means that `JsonB` is slower to input, but faster to process as it
 * avoids reparsing. Therefore, applications that utilize the structured
 * state of a JSON value should prefer `JsonB`.
 *
 * It also means that the `JsonB` stored representation does NOT preserve:
 *   - white space,
 *   - the order of object keys, or
 *   - duplicate object keys.
 *
 * Note: `JsonB` is only applicable to PostgreSQL databases (i.e., those
 * created using `DatabaseDialect::POSTGRESQL`).
 */
class JsonB {
 public:
  /// A null value.
  JsonB() : rep_("null") {}

  /// @name Regular value type, supporting copy, assign, move.
  ///@{
  JsonB(JsonB const&) = default;
  JsonB& operator=(JsonB const&) = default;
  JsonB(JsonB&&) = default;
  JsonB& operator=(JsonB&&) = default;
  ///@}

  /**
   * Construction from a JSON-formatted string. Note that there is no check
   * here that the argument string is indeed well-formatted. Error detection
   * will be delayed until the value is passed to Spanner.
   */
  explicit JsonB(std::string s) : rep_(std::move(s)) {}

  /// @name Conversion to a JSON-formatted string.
  ///@{
  explicit operator std::string() const& { return rep_; }
  explicit operator std::string() && { return std::move(rep_); }
  ///@}

 private:
  std::string rep_;  // a (presumably) JSON-formatted string
};

/// @name Relational operators
///@{
inline bool operator==(JsonB const& lhs, JsonB const& rhs) {
  return std::string(lhs) == std::string(rhs);
}
inline bool operator!=(JsonB const& lhs, JsonB const& rhs) {
  return !(lhs == rhs);
}
///@}

/// Outputs a JSON-formatted string to the provided stream.
inline std::ostream& operator<<(std::ostream& os, JsonB const& json) {
  return os << std::string(json);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_JSON_H

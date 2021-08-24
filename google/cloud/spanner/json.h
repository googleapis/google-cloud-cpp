// Copyright 2021 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_JSON_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_JSON_H

#include "google/cloud/spanner/version.h"
#include <ostream>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {

/**
 * A simple representation for the Spanner JSON type: a lightweight,
 * text-based, language-independent data interchange format. JSON (the
 * JavaScript Object Notation) defines a small set of formatting rules
 * for the portable representation of structured data. See RFC 7159.
 *
 * A `JSON` value can be constructed from, and converted to a `std::string`.
 * `JSON` values can be compared (by string) for equality, and streamed.
 *
 * There is no syntax checking of JSON strings in this interface. The user
 * is expected to only construct `JSON` values from well-formatted strings.
 */
class JSON {
 public:
  /// A null value.
  JSON() : rep_("null") {}

  /// Regular value type, supporting copy, assign, move.
  ///@{
  JSON(JSON const&) = default;
  JSON& operator=(JSON const&) = default;
  JSON(JSON&&) = default;
  JSON& operator=(JSON&&) = default;
  ///@}

  /**
   * Construction from a JSON-formatted string. Note that there is no check
   * here that the argument string is indeed well-formatted. Error detection
   * will be delayed until the value is passed to Spanner.
   */
  explicit JSON(std::string s) : rep_(std::move(s)) {}

  /// Conversion to a JSON-formatted string.
  ///@{
  explicit operator std::string() const& { return rep_; }
  explicit operator std::string() && { return std::move(rep_); }
  ///@}

 private:
  std::string rep_;  // a (presumably) JSON-formatted string
};

/// @name Relational operators
///@{
inline bool operator==(JSON const& lhs, JSON const& rhs) {
  return std::string(lhs) == std::string(rhs);
}
inline bool operator!=(JSON const& lhs, JSON const& rhs) {
  return !(lhs == rhs);
}
///@}

/// Outputs a JSON-formatted string to the provided stream.
inline std::ostream& operator<<(std::ostream& os, JSON const& json) {
  return os << std::string(json);
}

}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_JSON_H

// Copyright 2023 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_OID_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_OID_H

#include "google/cloud/spanner/version.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include <ostream>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * A representation of the PostgreSQL Object Identifier (OID) type: used as
 * primary keys for various system tables in the PostgreSQL dialect.
 */
class PgOid {
 public:
  /// Default construction yields OID 0.
  PgOid() : rep_("0") {}

  /// @name Regular value type, supporting copy, assign, move.
  ///@{
  PgOid(PgOid const&) = default;
  PgOid& operator=(PgOid const&) = default;
  PgOid(PgOid&&) = default;
  PgOid& operator=(PgOid&&) = default;
  ///@}

  /**
   * Construction from an OID string. Note that there is no check here that the
   * argument string is indeed well-formatted. Error detection will be delayed
   * until the value is passed to Spanner.
   */
  explicit PgOid(std::string value) : rep_(std::move(value)) {}

  /**
   * Construction from an OID unsigned four-byte integer.
  */
  explicit PgOid(std::uint64_t value) : rep_(absl::StrCat(value)) {}

  /// @name Conversion to an OID string.
  ///@{
  explicit operator std::string() const& { return rep_; }
  explicit operator std::string() && { return std::move(rep_); }
  ///@}

  /// @name Conversion to an OID unsigned four-byte integer.
  ///@{
  explicit operator std::uint64_t() const& { return std::stoull(rep_); }
  explicit operator std::uint64_t() && { return std::move(std::stoull(rep_)); }
  ///@}

 private:
  std::string rep_;
};

/// @name Relational operators
///@{
inline bool operator==(PgOid const& lhs, PgOid const& rhs) {
  return std::string(lhs) == std::string(rhs);
}
inline bool operator!=(PgOid const& lhs, PgOid const& rhs) { return !(lhs == rhs); }
///@}

/// Outputs an OID formatted string to the provided stream.
inline std::ostream& operator<<(std::ostream& os, PgOid const& oid) {
  return os << std::string(oid);
}

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_SPANNER_OID_H

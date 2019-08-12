// Copyright 2019 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_GOOGLE_BYTES_TRAITS_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_GOOGLE_BYTES_TRAITS_H_

#include "google/cloud/bigtable/internal/google_bytes_traits.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/big_endian.h"

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
namespace internal {
//@{
/**
 * @name Define functions to manipulate `std::string`.
 *
 * Inside google, some of the protos for Bigtable have a special mapping. The
 * `bytes` fields do not map to `std::string`, but to a different type that has
 * (unfortunately) a slightly different interface. These functions allow us to
 * manipulate `std::string` and that internal type without having to change the
 * library.
 */
/// Return true if the row key is empty.
inline bool IsEmptyRowKey(std::string const& key) { return key.empty(); }

/// Return true if the row key is empty.
inline bool IsEmptyRowKey(char const* key) { return std::string{} == key; }

#if __cplusplus >= 201703L
/// Return true if the row key is empty.
inline bool IsEmptyRowKey(std::string_view key) { return key.empty(); }
#endif  // __cplusplus >= 201703L

/// Return `< 0` if `lhs < rhs`, 0 if `lhs == rhs`, and `> 0' otherwise.
inline int CompareRowKey(std::string const& lhs, std::string const& rhs) {
  return lhs.compare(rhs);
}

/// Returns true iff a < b and there is no string c such that a < c < b.
bool ConsecutiveRowKeys(std::string const& a, std::string const& b);

/// Return `< 0` if `lhs < rhs`, 0 if `lhs == rhs`, and `> 0' otherwise.
inline int CompareColumnQualifiers(std::string const& lhs,
                                   std::string const& rhs) {
  return lhs.compare(rhs);
}

/// Decode a cell value assuming it contains a 64-bit int in Big Endian order.
template <typename T>
StatusOr<T> DecodeBigEndianCellValue(std::string const& c) {
  return google::cloud::internal::DecodeBigEndian<T>(std::string(c));
}

/// Return `< 0` if `lhs < rhs`, 0 if `lhs == rhs`, and `> 0' otherwise.
inline int CompareCellValues(std::string const& lhs, std::string const& rhs) {
  return lhs.compare(rhs);
}

/// Append @p fragment to @p value.
inline void AppendCellValue(std::string& value, std::string const& fragment) {
  value.append(fragment);
}

/// An adapter for `std::string::reserve()`.
inline void ReserveCellValue(std::string& value, std::size_t reserve) {
  value.reserve(reserve);
}
//@}

}  // namespace internal
}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_INTERNAL_GOOGLE_BYTES_TRAITS_H_

// Copyright 2017 Google Inc.
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CELL_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CELL_H_

#include "google/cloud/bigtable/row_key.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/internal/big_endian.h"
#include "google/cloud/status_or.h"
#include <chrono>
#include <type_traits>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/**
 * The in-memory representation of a Bigtable cell.
 *
 * Bigtable stores data in rows, indexes by row keys.  Each row may contain
 * multiple column families, each column family might contain multiple columns,
 * and each column has multiple cells indexed by timestamp.  Notice that the
 * storage is sparse, column families, columns, and timestamps might contain
 * zero cells.
 *
 * The Cell class owns all its data.
 */
class Cell {
 public:
  /// Create a Cell and fill it with data.
  template <typename KeyType, typename ColumnType, typename ValueType,
            // This function does not participate in overload resolution if
            // ValueType is an integral type. That is handled by the next
            // overload, where the value is stored as a Big Endian number.
            typename std::enable_if<!std::is_integral<ValueType>::value,
                                    int>::type = 0>
  Cell(KeyType row_key, std::string family_name, ColumnType column_qualifier,
       std::int64_t timestamp, ValueType value, std::vector<std::string> labels)
      : row_key_(std::forward<KeyType>(row_key)),
        family_name_(std::move(family_name)),
        column_qualifier_(std::forward<ColumnType>(column_qualifier)),
        timestamp_(timestamp),
        value_(std::forward<ValueType>(value)),
        labels_(std::move(labels)) {}

  /// Create a Cell and fill it with a 64-bit value encoded as big endian.
  template <typename KeyType, typename ColumnType>
  Cell(KeyType row_key, std::string family_name, ColumnType column_qualifier,
       std::int64_t timestamp, std::int64_t value,
       std::vector<std::string> labels)
      : Cell(std::forward<KeyType>(row_key), std::move(family_name),
             std::forward<ColumnType>(column_qualifier), timestamp,
             google::cloud::internal::EncodeBigEndian(value),
             std::move(labels)) {}

  /// Create a cell and fill it with data, but with empty labels.
  template <typename KeyType, typename ColumnType, typename ValueType>
  Cell(KeyType row_key, std::string family_name, ColumnType column_qualifier,
       std::int64_t timestamp, ValueType value)
      : Cell(std::forward<KeyType>(row_key), std::move(family_name),
             std::forward<ColumnType>(column_qualifier), timestamp,
             std::forward<ValueType>(value), std::vector<std::string>{}) {}

  /// Return the row key this cell belongs to. The returned value is not valid
  /// after this object is deleted.
  RowKeyType const& row_key() const { return row_key_; }

  /// Return the family this cell belongs to. The returned value is not valid
  /// after this object is deleted.
  std::string const& family_name() const { return family_name_; }

  /// Return the column this cell belongs to. The returned value is not valid
  /// after this object is deleted.
  ColumnQualifierType const& column_qualifier() const {
    return column_qualifier_;
  }

  /// Return the timestamp of this cell.
  std::chrono::microseconds timestamp() const {
    return std::chrono::microseconds(timestamp_);
  }

  /// Return the contents of this cell. The returned value is not valid after
  /// this object is deleted.
  CellValueType const& value() const { return value_; }

  /**
   * Interpret the value as a big-endian encoded `T` and return it.
   *
   * Google Cloud Bigtable stores arbitrary blobs in each cell. Some
   * applications interpret these blobs as strings, other as encoded protos,
   * and sometimes as big-endian integers. This is a helper function to convert
   * the blob into a T value.
   */
  template <typename T>
  StatusOr<T> decode_big_endian_integer() const {
    return google::cloud::internal::DecodeBigEndian<T>(value_);
  }

  /// Return the labels applied to this cell by label transformer read filters.
  std::vector<std::string> const& labels() const { return labels_; }

 private:
  RowKeyType row_key_;
  std::string family_name_;
  ColumnQualifierType column_qualifier_;
  std::int64_t timestamp_;
  CellValueType value_;
  std::vector<std::string> labels_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_CELL_H_

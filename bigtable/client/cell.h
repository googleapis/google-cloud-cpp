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

#ifndef BIGTABLE_CLIENT_CELL_H_
#define BIGTABLE_CLIENT_CELL_H_

#include <absl/base/thread_annotations.h>
#include <absl/strings/string_view.h>
#include <bigtable/client/version.h>
#include <mutex>
#include <vector>

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
 */
class Cell {
 public:
  Cell(absl::string_view row_key, absl::string_view family_name,
       absl::string_view column_qualifier, int64_t timestamp,
       // TODO: use something more generic than vector
       std::vector<absl::string_view> chunks,
       std::vector<absl::string_view> labels)
      : row_key_(row_key),
        family_name_(family_name),
        column_qualifier_(column_qualifier),
        timestamp_(timestamp),
        chunks_(chunks),
        labels_(labels) {}

  absl::string_view row_key() const { return row_key_; }
  absl::string_view family_name() const { return family_name_; }
  absl::string_view column_qualifier() const { return column_qualifier_; }
  int64_t timestamp() const { return timestamp_; }

  absl::string_view value() const {
    std::lock_guard<std::mutex> l(mu_);
    if (not chunks_.empty()) {
      consolidate();
    }
    return value_;
  };
  const std::vector<absl::string_view>& labels() const { return labels_; }

 private:
  absl::string_view row_key_;
  absl::string_view family_name_;
  absl::string_view column_qualifier_;
  int64_t timestamp_;

  mutable absl::string_view value_ GUARDED_BY(mu_);
  mutable std::string copied_value_ GUARDED_BY(mu_);
  mutable std::vector<absl::string_view> chunks_ GUARDED_BY(mu_);
  std::vector<absl::string_view> labels_;

  mutable std::mutex mu_;
  void consolidate() const;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable

#endif  // BIGTABLE_CLIENT_ROW_H_

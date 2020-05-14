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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_CONFIG_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_CONFIG_H

#include "google/cloud/bigtable/column_family.h"
#include "google/cloud/bigtable/version.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.pb.h>
#include <map>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace BIGTABLE_CLIENT_NS {
/// Specify the initial schema for a new table.
class TableConfig {
 public:
  TableConfig() : granularity_(TIMESTAMP_GRANULARITY_UNSPECIFIED) {}

  TableConfig(std::map<std::string, GcRule> column_families,
              std::vector<std::string> initial_splits)
      : column_families_(std::move(column_families)),
        initial_splits_(std::move(initial_splits)),
        granularity_(TIMESTAMP_GRANULARITY_UNSPECIFIED) {}

  /// Move the contents to the proto to create tables.
  // Note that this function returns a copy intentionally, the object does not
  // have the proto as a member variable, it constructs the proto from its own
  // data structures for "reasons".
  ::google::bigtable::admin::v2::CreateTableRequest as_proto() &&;

  using TimestampGranularity =
      ::google::bigtable::admin::v2::Table::TimestampGranularity;
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static TimestampGranularity MILLIS =
      ::google::bigtable::admin::v2::Table::MILLIS;
  // NOLINTNEXTLINE(readability-identifier-naming)
  constexpr static TimestampGranularity TIMESTAMP_GRANULARITY_UNSPECIFIED =
      ::google::bigtable::admin::v2::Table::TIMESTAMP_GRANULARITY_UNSPECIFIED;

  //@{
  /// @name Accessors and modifiers for all attributes
  std::map<std::string, GcRule> const& column_families() const {
    return column_families_;
  }
  void add_column_family(std::string column_family_name, GcRule gc_rule) {
    column_families_.emplace(std::move(column_family_name), std::move(gc_rule));
  }

  std::vector<std::string> const& initial_splits() const {
    return initial_splits_;
  }
  void add_initial_split(std::string split) {
    initial_splits_.emplace_back(std::move(split));
  }

  /**
   * Return the timestamp granularity parameter.
   *
   * Cloud Bigtable currently supports only millisecond granularity in the
   * cell timestamps, both `TIMESTAMP_GRANULARITY_UNSPECIFIED` and `MILLIS`
   * have the same effect.
   */
  TimestampGranularity timestamp_granularity() const { return granularity_; }

  /**
   * Set the timestamp granularity parameter.
   *
   * Cloud Bigtable currently supports only millisecond granularity in the
   * cell timestamps, both `TIMESTAMP_GRANULARITY_UNSPECIFIED` and `MILLIS`
   * have the same effect.  Creating cells with higher granularity than the
   * supported value is rejected by the server.
   */
  void set_timestamp_granularity(TimestampGranularity new_value) {
    granularity_ = new_value;
  }
  //@}

 private:
  std::map<std::string, GcRule> column_families_;
  std::vector<std::string> initial_splits_;
  ::google::bigtable::admin::v2::Table::TimestampGranularity granularity_;
};

}  // namespace BIGTABLE_CLIENT_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_CONFIG_H

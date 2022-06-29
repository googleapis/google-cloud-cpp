// Copyright 2022 Google LLC
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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_RESOURCE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_RESOURCE_H

#include "google/cloud/bigtable/instance_resource.h"
#include "google/cloud/bigtable/version.h"
#include "google/cloud/status_or.h"
#include <iosfwd>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN

/**
 * This class identifies a Cloud Bigtable Table.
 *
 * Bigtable stores data in massively scalable tables, each of which is a sorted
 * key/value map. A Cloud Bigtable table is identified by the instance it is
 * contained in and its `table_id`.
 *
 * @note This class makes no effort to validate the components of the
 *     table name. It is the application's responsibility to provide valid
 *     project, instance, and table ids. Passing invalid values will not be
 *     checked until the table name is used in a RPC to Bigtable.
 *
 * @see https://cloud.google.com/bigtable/docs/overview for an overview of the
 *     Cloud Bigtable data model.
 */
class TableResource {
 public:
  /**
   * Constructs a TableResource object identified by the given @p instance and
   * @p table_id.
   */
  TableResource(InstanceResource instance, std::string table_id);

  /// Returns the `InstanceResource` containing this table.
  InstanceResource const& instance() const { return instance_; }

  /// Returns the Table ID.
  std::string const& table_id() const { return table_id_; }

  /**
   * Returns the fully qualified table name as a string of the form:
   * "projects/<project-id>/instances/<instance-id>/tables/<table-id>"
   */
  std::string FullName() const;

  /// @name Equality operators
  //@{
  friend bool operator==(TableResource const& a, TableResource const& b);
  friend bool operator!=(TableResource const& a, TableResource const& b);
  //@}

  /// Output the `FullName()` format.
  friend std::ostream& operator<<(std::ostream&, TableResource const&);

 private:
  InstanceResource instance_;
  std::string table_id_;
};

/**
 * Constructs a `TableResource` from the given @p full_name.
 * Returns a non-OK Status if `full_name` is improperly formed.
 */
StatusOr<TableResource> MakeTableResource(std::string const& full_name);

GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TABLE_RESOURCE_H

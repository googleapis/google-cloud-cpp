// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_TABLE_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_TABLE_H

#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <google/bigtable/admin/v2/table.pb.h>
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <google/protobuf/field_mask.pb.h>
#include <google/protobuf/util/time_util.h>
#include "google/cloud/bigtable/emulator/column_family.h"
#include "google/cloud/bigtable/emulator/row_streamer.h"
#include <map>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

struct CellView {
  std::int64_t timestamp;
  std::string const& value;
};

class Table {
 public:
  static StatusOr<std::shared_ptr<Table>> Create(
      google::bigtable::admin::v2::Table schema);

  google::bigtable::admin::v2::Table GetSchema() const;

  Status Update(google::bigtable::admin::v2::Table const& new_schema,
                google::protobuf::FieldMask const& to_update);

  StatusOr<google::bigtable::admin::v2::Table> ModifyColumnFamilies(
      google::bigtable::admin::v2::ModifyColumnFamiliesRequest const& request);

  bool IsDeleteProtected() const;

  Status MutateRow(google::bigtable::v2::MutateRowRequest const & request);

  Status ReadRows(google::bigtable::v2::ReadRowsRequest const& request,
                  RowStreamer& row_streamer) const;

 private:
  Table() = default;
  friend class RowSetIterator;

  template <typename MESSAGE>
  StatusOr<std::reference_wrapper<ColumnFamily>> FindColumnFamily(
      MESSAGE const& message) const;
  bool IsDeleteProtectedNoLock() const;
  Status Construct(google::bigtable::admin::v2::Table schema);

  mutable std::mutex mu_;
  google::bigtable::admin::v2::Table schema_;
  std::map<std::string, std::shared_ptr<ColumnFamily>> column_families_;
};

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_TABLE_H

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

#include "google/cloud/bigtable/emulator/column_family.h"
#include "google/cloud/bigtable/emulator/filter.h"
#include "google/cloud/bigtable/emulator/row_streamer.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "absl/types/variant.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <google/bigtable/admin/v2/table.pb.h>
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <google/bigtable/v2/bigtable.pb.h>
#include <google/protobuf/field_mask.pb.h>
#include <google/protobuf/util/time_util.h>
#include <chrono>
#include <map>
#include <memory>
#include <stack>
#include <utility>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

class RowSampler;

/// Objects of this class represent Bigtable tables.
class Table : public std::enable_shared_from_this<Table> {
 public:
  static StatusOr<std::shared_ptr<Table>> Create(
      google::bigtable::admin::v2::Table schema);

  google::bigtable::admin::v2::Table GetSchema() const;

  Status Update(google::bigtable::admin::v2::Table const& new_schema,
                google::protobuf::FieldMask const& to_update);

  StatusOr<google::bigtable::admin::v2::Table> ModifyColumnFamilies(
      google::bigtable::admin::v2::ModifyColumnFamiliesRequest const& request);

  bool IsDeleteProtected() const;

  Status MutateRow(google::bigtable::v2::MutateRowRequest const& request);

  Status ReadRows(google::bigtable::v2::ReadRowsRequest const& request,
                  RowStreamer& row_streamer) const;
  std::map<std::string, std::shared_ptr<ColumnFamily>>::iterator begin() {
    return column_families_.begin();
  }
  std::map<std::string, std::shared_ptr<ColumnFamily>>::iterator end() {
    return column_families_.end();
  }
  std::map<std::string, std::shared_ptr<ColumnFamily>>::iterator find(
      std::string const& column_family) {
    return column_families_.find(column_family);
  }

  RowSampler SampleRowKeys(
      const google::bigtable::v2::SampleRowKeysRequest&);

  std::shared_ptr<Table> get() { return shared_from_this(); }

 private:
  Table() = default;
  friend class RowSetIterator;
  friend class RowTransaction;
  friend class RowSampler;

  template <typename MESSAGE>
  StatusOr<std::reference_wrapper<ColumnFamily>> FindColumnFamily(
      MESSAGE const& message) const;
  bool IsDeleteProtectedNoLock() const;
  Status Construct(google::bigtable::admin::v2::Table schema);

  mutable std::mutex mu_;
  google::bigtable::admin::v2::Table schema_;
  std::map<std::string, std::shared_ptr<ColumnFamily>> column_families_;
};

struct RestoreValue {
  ColumnFamily& column_family;
  std::string column_qualifier;
  std::chrono::milliseconds timestamp;
  std::string value;
};

struct DeleteValue {
  ColumnFamily& column_family;
  std::string column_qualifier;
  std::chrono::milliseconds timestamp;
};

class RowTransaction {
 public:
  explicit RowTransaction(
      std::shared_ptr<Table> table,
      ::google::bigtable::v2::MutateRowRequest const& request)
      : request_(request) {
    table_ = std::move(table);
    committed_ = false;
  };

  ~RowTransaction() {
    if (!committed_) {
      Undo();
    }
  };

  void commit() { committed_ = true; }

  Status SetCell(::google::bigtable::v2::Mutation_SetCell const& set_cell);
  Status AddToCell(
      ::google::bigtable::v2::Mutation_AddToCell const& add_to_cell);
  Status MergeToCell(
      ::google::bigtable::v2::Mutation_MergeToCell const& merge_to_cell);
  Status DeleteFromColumn(
      ::google::bigtable::v2::Mutation_DeleteFromColumn const&
          delete_from_column);
  Status DeleteFromFamily(
      ::google::bigtable::v2::Mutation_DeleteFromFamily const&
          delete_from_family);
  Status DeleteFromRow();

 private:
  void Undo();

  bool committed_;
  std::shared_ptr<Table> table_;
  std::stack<absl::variant<DeleteValue, RestoreValue>> undo_;
  ::google::bigtable::v2::MutateRowRequest const& request_;
};

class RowSampler {
 public:
  explicit RowSampler(
      std::shared_ptr<Table> table,
      std::function<google::bigtable::v2::SampleRowKeysResponse()>
          next_sample_closure) {
    table_ = std::move(table);
    next_sample_closure_ = std::move(next_sample_closure);

    table_->mu_.lock();
  };

  google::bigtable::v2::SampleRowKeysResponse Next() {
    return next_sample_closure_();
  }

  ~RowSampler() { table_->mu_.unlock(); };

 private:
  std::shared_ptr<Table> table_;
  std::function<google::bigtable::v2::SampleRowKeysResponse()>
      next_sample_closure_;
};

/**
 * A `AbstractCellStreamImpl` which streams filtered contents of the table.
 *
 * Underneath is essentially a collection of `FilteredColumnFamilyStream`s.
 * All filters applied to `FilteredColumnFamilyStream` are propagated to the
 * underlying `FilteredColumnFamilyStream`, except for `FamilyNameRegex`, which
 * is handled by this subclass.
 *
 * This class is public only to enable testing.
 */
class FilteredTableStream : public MergeCellStreams {
 public:
  explicit FilteredTableStream(
      std::vector<std::unique_ptr<FilteredColumnFamilyStream>> cf_streams)
      : MergeCellStreams(CreateCellStreams(std::move(cf_streams))) {}

  bool ApplyFilter(InternalFilter const& internal_filter) override;

 private:
  static std::vector<CellStream> CreateCellStreams(
      std::vector<std::unique_ptr<FilteredColumnFamilyStream>> cf_streams);
};

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_EMULATOR_TABLE_H

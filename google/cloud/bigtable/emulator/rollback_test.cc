// Copyright 2025 Google LLC
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

#include "google/cloud/bigtable/emulator/column_family.h"
#include "google/cloud/bigtable/emulator/table.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <google/bigtable/admin/v2/bigtable_table_admin.pb.h>
#include <google/bigtable/admin/v2/table.pb.h>
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <google/bigtable/v2/bigtable.pb.h>
#include <google/bigtable/v2/data.pb.h>
#include <absl/strings/str_format.h>
#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

struct SetCellParams {
  std::string column_family_name;
  std::string column_qualifier;
  int64_t timestamp_micros;
  std::string data;
};

StatusOr<std::shared_ptr<Table>> create_table(
    std::string const& table_name, std::vector<std::string>& column_families) {
  ::google::bigtable::admin::v2::Table schema;
  schema.set_name(table_name);
  for (auto& column_family_name : column_families) {
    (*schema.mutable_column_families())[column_family_name] =
        ::google::bigtable::admin::v2::ColumnFamily();
  }

  return Table::Create(schema);
}

Status set_cells(
    std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
    std::string const& table_name, std::string const& row_key,
    std::vector<SetCellParams>& set_cell_params) {

  ::google::bigtable::v2::MutateRowRequest mutation_request;
  mutation_request.set_table_name(table_name);
  mutation_request.set_row_key(row_key);

  for (auto m : set_cell_params) {
    auto* mutation_request_mutation = mutation_request.add_mutations();
    auto* set_cell_mutation = mutation_request_mutation->mutable_set_cell();
    set_cell_mutation->set_family_name(m.column_family_name);
    set_cell_mutation->set_column_qualifier(m.column_qualifier);
    set_cell_mutation->set_timestamp_micros(m.timestamp_micros);
    set_cell_mutation->set_value(m.data);
  }

  return table->MutateRow(mutation_request);
}

Status has_cell(
    std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
    std::string const& column_family, std::string const& row_key,
    std::string const& column_qualifier, int64_t timestamp_micros,
    std::string const& value) {
  auto column_family_it = table->find(column_family);
  if (column_family_it == table->end()) {
    return Status(
        StatusCode::kNotFound,
        absl::StrFormat("column family %s not found in table", column_family),
        ErrorInfo());
  }

  auto const& cf = column_family_it->second;
  auto column_family_row_it = cf->find(row_key);
  if (column_family_row_it == cf->end()) {
    return Status(StatusCode::kNotFound,
                  absl::StrFormat("no row key %s found in column famiily %s",
                                  row_key, column_family),
                  ErrorInfo());
  }

  auto& column_family_row = column_family_row_it->second;
  auto column_row_it = column_family_row.find(column_qualifier);
  if (column_row_it == column_family_row.end()) {
    return Status(
        StatusCode::kNotFound,
        absl::StrFormat("no column found with qualifer %s", column_qualifier),
        ErrorInfo());
  }

  auto& column_row = column_row_it->second;
  auto timestamp_it =
      column_row.find(std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::microseconds(timestamp_micros)));
  if (timestamp_it == column_row.end()) {
    return Status(StatusCode::kNotFound, "timestamp not found", ErrorInfo());
  }

  if (timestamp_it->second != value) {
    return Status(StatusCode::kNotFound,
                  absl::StrFormat("wrong value: expected %s, found %s", value,
                                  timestamp_it->second),
                  ErrorInfo());
  }

  return Status(StatusCode::kOk, "", ErrorInfo());
}

Status has_column(
    std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
    std::string const& column_family, std::string const& row_key,
    std::string const& column_qualifier) {
  auto column_family_it = table->find(column_family);
  if (column_family_it == table->end()) {
    return Status(
        StatusCode::kNotFound,
        absl::StrFormat("column family %s not found in table", column_family),
        ErrorInfo());
  }

  auto const& cf = column_family_it->second;
  auto column_family_row_it = cf->find(row_key);
  if (column_family_row_it == cf->end()) {
    return Status(StatusCode::kNotFound,
                  absl::StrFormat("no row key %s found in column famiily %s",
                                  row_key, column_family),
                  ErrorInfo());
  }

  auto& column_family_row = column_family_row_it->second;
  auto column_row_it = column_family_row.find(column_qualifier);
  if (column_row_it == column_family_row.end()) {
    return Status(
        StatusCode::kNotFound,
        absl::StrFormat("no column found with qualifer %s", column_qualifier),
        ErrorInfo());
  }

  return Status(StatusCode::kOk, "", ErrorInfo());
}

Status has_row(std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
               std::string const& column_family, std::string const& row_key) {
  auto column_family_it = table->find(column_family);
  if (column_family_it == table->end()) {
    return Status(
        StatusCode::kNotFound,
        absl::StrFormat("column family %s not found in table", column_family),
        ErrorInfo());
  }

  auto const& cf = column_family_it->second;
  auto column_family_row_it = cf->find(row_key);
  if (column_family_row_it == cf->end()) {
    return Status(StatusCode::kNotFound,
                  absl::StrFormat("no row key %s found in column famiily %s",
                                  row_key, column_family),
                  ErrorInfo());
  }

  return Status(StatusCode::kOk, "", ErrorInfo());
}

// Does the SetCell mutation work to set a cell to a specific value?
TEST(TransactonRollback, SetCellBasicFunction) {
  ::google::bigtable::admin::v2::Table schema;
  ::google::bigtable::admin::v2::ColumnFamily column_family;

  auto const* const table_name = "projects/test/instances/test/tables/test";
  auto const* const row_key = "0";
  auto const* const column_family_name = "test";
  auto const* const column_qualifer = "test";
  auto const timestamp_micros = 1234;
  auto const* data = "test";

  std::vector<std::string> column_families = {column_family_name};
  auto maybe_table = create_table(table_name, column_families);

  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  std::vector<SetCellParams> v;
  SetCellParams p = {column_family_name, column_qualifer, timestamp_micros, data};
  v.push_back(p);

  auto status = set_cells(table, table_name, row_key, v);

  ASSERT_STATUS_OK(status);

  ASSERT_STATUS_OK(has_cell(table, column_family_name, row_key, column_qualifer,
                            timestamp_micros, data));
}

// Test that an old value is correctly restored in a pre-populated
// cell, when one of a set of SetCell mutations fails after the cell
// had been updated with a new value.
TEST(TransactonRollback, TestRestoreValue) {
  ::google::bigtable::admin::v2::Table schema;
  ::google::bigtable::admin::v2::ColumnFamily column_family;

  auto const* const table_name = "projects/test/instances/test/tables/test";
  auto const* const row_key = "0";
  // The table will be set up with a schema with
  // valid_column_family_name and mutations with this column family
  // name are expected to succeed. We will simulate a transaction
  // failure by setting some other not-pre-provisioned column family
  // name.
  auto const* const valid_column_family_name = "test";
  auto const* const column_qualifer = "test";
  int64_t good_mutation_timestamp_micros = 1000;
  auto const* const good_mutation_data = "expected to succeed";

  std::vector<std::string> column_families = {valid_column_family_name};
  auto maybe_table = create_table(table_name, column_families);
  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  std::vector<SetCellParams> v;
  SetCellParams p = {valid_column_family_name, column_qualifer,
                     good_mutation_timestamp_micros, good_mutation_data};
  v.push_back(p);

  auto status = set_cells(table, table_name, row_key, v);
  ASSERT_STATUS_OK(status);
  ASSERT_STATUS_OK(has_cell(table, valid_column_family_name, row_key,
                            column_qualifer, good_mutation_timestamp_micros,
                            good_mutation_data));

  // Now atomically try 2 mutations. One modifies the above set cell,
  // and the other one is expected to fail. The test is that
  // RestoreValue will restore the previous value in cell with
  // timestamp 1000.
  std::vector<SetCellParams> w;
  // Everything is the same but we try and modify the value in the cell cell set above.
  p.data = "new data";
  w.push_back(p);

  // Because "invalid_column_family" does not exist in the table
  // schema, a mutation with these SetCell parameters is expected to
  // fail.
  p = {"invalid_column_family", "test2", 1000, "expected to fail"};
  w.push_back(p);

  status = set_cells(table, table_name, row_key, w);
  ASSERT_NE(status.ok(), true); // The whole mutation chain should
                                // fail because the 2nd mutation
                                // contains an invalid column family.

  // And the first mutation should have been rolled back by
  // RestoreValue and so should contain the old value, and not "new
  // data".
  ASSERT_STATUS_OK(has_cell(table, valid_column_family_name, row_key,
                            column_qualifer, good_mutation_timestamp_micros,
                            good_mutation_data));

}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

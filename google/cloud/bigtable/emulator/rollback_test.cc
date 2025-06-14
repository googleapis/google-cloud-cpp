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
#include "google/cloud/bigtable/emulator/row_streamer.h"
#include "google/cloud/bigtable/emulator/table.h"
#include "google/cloud/internal/make_status.h"
#include "google/cloud/status.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "gmock/gmock.h"
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
using std::string;

struct SetCellParams {
  std::string column_family_name;
  std::string column_qualifier;
  int64_t timestamp_micros;
  std::string data;
};

StatusOr<std::shared_ptr<Table>> CreateTable(
    std::string const& table_name, std::vector<std::string>& column_families) {
  ::google::bigtable::admin::v2::Table schema;
  schema.set_name(table_name);
  for (auto& column_family_name : column_families) {
    (*schema.mutable_column_families())[column_family_name] =
        ::google::bigtable::admin::v2::ColumnFamily();
  }

  return Table::Create(schema);
}

Status DeleteFromFamilies(
    std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
    std::string const& table_name, std::string const& row_key,
    std::vector<string> const& column_families) {
  ::google::bigtable::v2::MutateRowRequest mutation_request;
  mutation_request.set_table_name(table_name);
  mutation_request.set_row_key(row_key);

  for (auto column_family : column_families) {
    auto* mutation_request_mutation = mutation_request.add_mutations();
    auto* delete_from_family_mutation =
        mutation_request_mutation->mutable_delete_from_family();
    delete_from_family_mutation->set_family_name(column_family);
  }

  return table->MutateRow(mutation_request);
}

struct DeleteFromColumnParams {
  std::string column_family;
  std::string column_qualifier;
  ::google::bigtable::v2::TimestampRange* timestamp_range;
};

Status DeleteFromColumns(
    std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
    std::string const& table_name, std::string const& row_key,
    std::vector<DeleteFromColumnParams> v) {
  ::google::bigtable::v2::MutateRowRequest mutation_request;
  mutation_request.set_table_name(table_name);
  mutation_request.set_row_key(row_key);

  for (auto& param : v) {
    auto* mutation_request_mutation = mutation_request.add_mutations();
    auto* delete_from_column_mutation =
        mutation_request_mutation->mutable_delete_from_column();
    delete_from_column_mutation->set_family_name(param.column_family);
    delete_from_column_mutation->set_column_qualifier(param.column_qualifier);
    delete_from_column_mutation->set_allocated_time_range(
        param.timestamp_range);
  }

  return table->MutateRow(mutation_request);
}

Status SetCells(
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

Status HasCell(std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
               std::string const& column_family, std::string const& row_key,
               std::string const& column_qualifier, int64_t timestamp_micros,
               std::string const& value) {
  auto column_family_it = table->find(column_family);
  if (column_family_it == table->end()) {
    return NotFoundError(
        "column family not found in table",
        GCP_ERROR_INFO().WithMetadata("column family", column_family));
  }

  auto const& cf = column_family_it->second;
  auto column_family_row_it = cf->find(row_key);
  if (column_family_row_it == cf->end()) {
    return NotFoundError("no row key found in column family",
                         GCP_ERROR_INFO()
                             .WithMetadata("row key", row_key)
                             .WithMetadata("column family", column_family));
  }

  auto& column_family_row = column_family_row_it->second;
  auto column_row_it = column_family_row.find(column_qualifier);
  if (column_row_it == column_family_row.end()) {
    return NotFoundError(
        "no column found with qualifier",
        GCP_ERROR_INFO().WithMetadata("column qualifier", column_qualifier));
  }

  auto& column_row = column_row_it->second;
  auto timestamp_it =
      column_row.find(std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::microseconds(timestamp_micros)));
  if (timestamp_it == column_row.end()) {
    return NotFoundError(
        "timestamp not found",
        GCP_ERROR_INFO().WithMetadata("timestamp",
                                      absl::StrFormat("%d", timestamp_micros)));
  }

  if (timestamp_it->second != value) {
    return NotFoundError("wrong value",
                         GCP_ERROR_INFO()
                             .WithMetadata("expected", value)
                             .WithMetadata("found", timestamp_it->second));
  }

  return Status();
}

Status HasColumn(
    std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
    std::string const& column_family, std::string const& row_key,
    std::string const& column_qualifier) {
  auto column_family_it = table->find(column_family);
  if (column_family_it == table->end()) {
    return NotFoundError(
        "column family not found in table",
        GCP_ERROR_INFO().WithMetadata("column family", column_family));
  }

  auto const& cf = column_family_it->second;
  auto column_family_row_it = cf->find(row_key);
  if (column_family_row_it == cf->end()) {
    return internal::NotFoundError(
        "row key not found in column family",
        GCP_ERROR_INFO()
            .WithMetadata("row key", row_key)
            .WithMetadata("column family", column_family));
  }

  auto& column_family_row = column_family_row_it->second;
  auto column_row_it = column_family_row.find(column_qualifier);
  if (column_row_it == column_family_row.end()) {
    return NotFoundError(
        "no column found with supplied qualifier",
        GCP_ERROR_INFO().WithMetadata("column qualifier", column_qualifier));
  }

  return Status();
}

StatusOr<std::map<std::chrono::milliseconds, std::string>> GetColumn(
    std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
    std::string const& column_family, std::string const& row_key,
    std::string const& column_qualifier) {
  auto column_family_it = table->find(column_family);
  if (column_family_it == table->end()) {
    return NotFoundError(
        "column family not found in table",
        GCP_ERROR_INFO().WithMetadata("column family", column_family));
  }

  auto const& cf = column_family_it->second;
  auto column_family_row_it = cf->find(row_key);
  if (column_family_row_it == cf->end()) {
    return internal::NotFoundError(
        "row key not found in column family",
        GCP_ERROR_INFO()
            .WithMetadata("row key", row_key)
            .WithMetadata("column family", column_family));
  }

  auto& column_family_row = column_family_row_it->second;
  auto column_row_it = column_family_row.find(column_qualifier);
  if (column_row_it == column_family_row.end()) {
    return NotFoundError(
        "no column found with supplied qualifier",
        GCP_ERROR_INFO().WithMetadata("column qualifier", column_qualifier));
  }

  std::map<std::chrono::milliseconds, std::string> ret(
      column_row_it->second.begin(), column_row_it->second.end());

  return ret;
}

Status HasRow(std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
              std::string const& column_family, std::string const& row_key) {
  auto column_family_it = table->find(column_family);
  if (column_family_it == table->end()) {
    return NotFoundError(
        "column family not found in table",
        GCP_ERROR_INFO().WithMetadata("column family", column_family));
  }

  auto const& cf = column_family_it->second;
  auto column_family_row_it = cf->find(row_key);
  if (column_family_row_it == cf->end()) {
    return NotFoundError("row key not found in column family",
                         GCP_ERROR_INFO()
                             .WithMetadata("row key", row_key)
                             .WithMetadata("column family", column_family));
  }

  return Status();
}

// Test that SetCell does the right thing when it receives a zero or
// negative timestamp, and that the cell created can be correctly
// deleted if rollback occurs.
//
// In particular:
//
// Supplied with a timestamp of -1, it should store the current system time as
// timestamp.
//
// Supplied with a timestamp of 0, it should store it as is.
//
// Supplied with a timestamp < -1, it should return an error and fail the entire
// mutation chain.
TEST(TransactionRollback, ZeroOrNegativeTimestampHandling) {
  ::google::bigtable::admin::v2::Table schema;
  ::google::bigtable::admin::v2::ColumnFamily column_family;

  auto const* const table_name = "projects/test/instances/test/tables/test";
  auto const* const row_key = "0";
  auto const* const column_family_name = "test";
  auto const* const column_qualifier = "test";
  auto const timestamp_micros = 0;
  auto const* data = "test";

  std::vector<std::string> column_families = {column_family_name};
  auto maybe_table = CreateTable(table_name, column_families);

  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  std::vector<SetCellParams> v;
  SetCellParams p = {column_family_name, column_qualifier, timestamp_micros,
                     data};
  v.push_back(p);

  auto status = SetCells(table, table_name, row_key, v);
  ASSERT_STATUS_OK(status);

  auto status_or =
      GetColumn(table, column_family_name, row_key, column_qualifier);
  ASSERT_STATUS_OK(status_or.status());
  auto column = status_or.value();
  ASSERT_EQ(1, column.size());
  for (auto const& cell : column) {
    ASSERT_EQ(cell.first.count(), 0);
    ASSERT_EQ(data, cell.second);
  }

  // Test that a mutation with timestamp 0 can be rolled back.
  v.clear();
  v = {{column_family_name, column_qualifier, 0, data},
       {"non_existent_column_family_name_causes_tx_rollbaclk", column_qualifier,
        1000, data}};
  auto const* const row_key_2 = "1";
  status = SetCells(table, table_name, row_key_2, v);
  ASSERT_NE(true, status.ok());
  ASSERT_FALSE(HasRow(table, column_family_name, row_key_2).ok());

  // Test that a mutation with timestamp 0 succeeds and stores 0 as
  // the timestamp.
  v.clear();
  v = {
      {column_family_name, column_qualifier, 0, data},
  };
  auto const* const row_key_3 = "2";
  status = SetCells(table, table_name, row_key_3, v);
  ASSERT_STATUS_OK(status);
  ASSERT_STATUS_OK(HasCell(table, v[0].column_family_name, row_key_3,
                           v[0].column_qualifier, 0, v[0].data));

  // Test that a mutation with timestamp < -1 fails
  v.clear();
  v = {
      {column_family_name, column_qualifier, -2, data},
  };
  auto const* const row_key_4 = "3";
  status = SetCells(table, table_name, row_key_4, v);
  ASSERT_FALSE(status.ok());

  // Test that a mutation with timestamp -1 succeeds and stores the
  // system time.
  v.clear();
  v = {
      {column_family_name, column_qualifier, -1, data},
  };
  auto const* const row_key_5 = "4";
  auto system_time_ms_before =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch());
  status = SetCells(table, table_name, row_key_5, v);
  ASSERT_STATUS_OK(status);
  auto column_or = GetColumn(table, v[0].column_family_name, row_key_5,
                             v[0].column_qualifier);
  ASSERT_STATUS_OK(column_or.status());
  auto col = column_or.value();
  ASSERT_EQ(col.size(), 1);
  auto cell_it = col.begin();
  ASSERT_NE(cell_it, col.end());
  ASSERT_EQ(cell_it->second, v[0].data);
  ASSERT_GE(cell_it->first, system_time_ms_before);
}

// Does the SetCell mutation work to set a cell to a specific value?
TEST(TransactionRollback, SetCellBasicFunction) {
  ::google::bigtable::admin::v2::Table schema;
  ::google::bigtable::admin::v2::ColumnFamily column_family;

  auto const* const table_name = "projects/test/instances/test/tables/test";
  auto const* const row_key = "0";
  auto const* const column_family_name = "test";
  auto const* const column_qualifier = "test";
  auto const timestamp_micros = 1234;
  auto const* data = "test";

  std::vector<std::string> column_families = {column_family_name};
  auto maybe_table = CreateTable(table_name, column_families);

  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  std::vector<SetCellParams> v;
  SetCellParams p = {column_family_name, column_qualifier, timestamp_micros,
                     data};
  v.push_back(p);

  auto status = SetCells(table, table_name, row_key, v);

  ASSERT_STATUS_OK(status);

  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           timestamp_micros, data));
}

// Test that an old value is correctly restored in a pre-populated
// cell, when one of a set of SetCell mutations fails after the cell
// had been updated with a new value.
TEST(TransactionRollback, TestRestoreValue) {
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
  auto const* const column_qualifier = "test";
  int64_t good_mutation_timestamp_micros = 1000;
  auto const* const good_mutation_data = "expected to succeed";

  std::vector<std::string> column_families = {valid_column_family_name};
  auto maybe_table = CreateTable(table_name, column_families);
  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  std::vector<SetCellParams> v;
  SetCellParams p = {valid_column_family_name, column_qualifier,
                     good_mutation_timestamp_micros, good_mutation_data};
  v.push_back(p);

  auto status = SetCells(table, table_name, row_key, v);
  ASSERT_STATUS_OK(status);
  ASSERT_STATUS_OK(HasCell(table, valid_column_family_name, row_key,
                           column_qualifier, good_mutation_timestamp_micros,
                           good_mutation_data));

  // Now atomically try 2 mutations. One modifies the above set cell,
  // and the other one is expected to fail. The test is that
  // RestoreValue will restore the previous value in cell with
  // timestamp 1000.
  std::vector<SetCellParams> w;
  // Everything is the same but we try and modify the value in the cell cell set
  // above.
  p.data = "new data";
  w.push_back(p);

  // Because "invalid_column_family" does not exist in the table
  // schema, a mutation with these SetCell parameters is expected to
  // fail.
  p = {"invalid_column_family", "test2", 1000, "expected to fail"};
  w.push_back(p);

  status = SetCells(table, table_name, row_key, w);
  ASSERT_NE(status.ok(), true);  // The whole mutation chain should
                                 // fail because the 2nd mutation
                                 // contains an invalid column family.

  // And the first mutation should have been rolled back by
  // RestoreValue and so should contain the old value, and not "new
  // data".
  ASSERT_STATUS_OK(HasCell(table, valid_column_family_name, row_key,
                           column_qualifier, good_mutation_timestamp_micros,
                           good_mutation_data));
}

// Test that a new cell introduced in a chain of SetCell mutations is
// deleted on rollback if a subsequent mutation fails.
TEST(TransactionRollback, DeleteValue) {
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
  std::vector<std::string> column_families = {valid_column_family_name};
  auto maybe_table = CreateTable(table_name, column_families);
  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  // To test that we do not delete a row or column that we should not,
  // let us first commit a transaction on the same row where we will
  // do the DeleteValue test.
  std::vector<SetCellParams> v = {
      {valid_column_family_name, "test", 1000, "data"}};
  auto status = SetCells(table, table_name, row_key, v);
  ASSERT_STATUS_OK(status);
  ASSERT_STATUS_OK(HasCell(table, valid_column_family_name, row_key,
                           v[0].column_qualifier, v[0].timestamp_micros,
                           v[0].data));

  // We then setup a transaction chain with 2 SetCells, the first one
  // should succeed to add a new cell and the second one should fail
  // (because it assumes an invalid schema in column family name). We
  // expect the first cell to not exist after the rollback (and of
  // course also no data from the 2nd failing SetCell mutation should
  // exist either).
  v = {{valid_column_family_name, "test", 2000, "new data"},
       {"invalid_column_family_name", "test", 3000, "more new data"}};

  status = SetCells(table, table_name, row_key, v);
  ASSERT_NE(status.ok(), true);  // We expect the chain of mutations to
                                 // fail altogether.
  status = HasCell(table, v[0].column_family_name, row_key,
                   v[0].column_qualifier, v[0].timestamp_micros, v[0].data);
  ASSERT_NE(status.ok(), true);  // Undo should delete the cell
  status = HasCell(table, v[1].column_family_name, row_key,
                   v[1].column_qualifier, v[1].timestamp_micros, v[1].data);
  ASSERT_NE(status.ok(), true);  // Also the SetCell with invalid shema
                                 // should not have set anything.
}

// Test that if a successful SetCell mutation in a chain of SetCell
// mutations in one transaction introduces a new column but a
// subsequent SetCell mutation fails (we simulate this by passing an
// column family name that is not in the table schema) then the column
// and any of the cells introduced is deleted in the rollback, but
// that any pre-transaction-attemot data in the row is unaffected.
TEST(TransactionRollback, DeleteColumn) {
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
  std::vector<std::string> column_families = {valid_column_family_name};
  auto maybe_table = CreateTable(table_name, column_families);
  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  std::vector<SetCellParams> v = {
      {valid_column_family_name, "test", 1000, "data"}};
  auto status = SetCells(table, table_name, row_key, v);
  ASSERT_STATUS_OK(status);
  ASSERT_STATUS_OK(HasCell(table, valid_column_family_name, row_key,
                           v[0].column_qualifier, v[0].timestamp_micros,
                           v[0].data));

  // Introduce a new column in a chain of SetCell mutations, a
  // subsequent one of which must fail due to an invalid schema
  // assumption (bad column family name).
  v = {{valid_column_family_name, "new_column", 2000, "new data"},
       {"invalid_column_family_name", "test", 3000, "more new data"}};

  status = SetCells(table, table_name, row_key, v);
  ASSERT_NE(status.ok(),
            true);  // We expect the chain of mutations to
                    // fail altogether because the last one must fail.

  // The original column ("test") should still exist.
  status = HasColumn(table, valid_column_family_name, row_key, "test");
  ASSERT_STATUS_OK(status);

  // Bit the new column introduced should have been rolled back.
  status =
      HasColumn(table, v[0].column_family_name, row_key, v[0].column_qualifier);
  ASSERT_NE(status.ok(), true);
}

// Test that a chain of SetCell mutations that initially introduces a
// new row, but one of which eventually fails, will end with the whole
// row rolled back.
TEST(TransactionRollback, DeleteRow) {
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
  std::vector<std::string> column_families = {valid_column_family_name};
  auto maybe_table = CreateTable(table_name, column_families);
  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  // First SetCell should succeed and introduce a new row with key
  // "0". The second one will fail due to bad schema settings. We
  // expect not to find the row after the row mutation call returns.
  std::vector<SetCellParams> v = {
      {valid_column_family_name, "test", 1000, "data"},
      {"invalid_column_family_name", "test", 2000,
       "more new data which should never be written"}};

  auto status = SetCells(table, table_name, row_key, v);
  ASSERT_NE(status.ok(),
            true);  // We expect the chain of mutations to
                    // fail altogether because the last one must fail.

  status = HasRow(table, valid_column_family_name, row_key);
  ASSERT_NE(status.ok(), true);
}

// Does the DeleteFromfamily mutation work to delete a row from a
// specific family and does it rows with the same row key in other
// column families alone?
TEST(TransactionRollback, DeleteFromFamilyBasicFunction) {
  ::google::bigtable::admin::v2::Table schema;
  ::google::bigtable::admin::v2::ColumnFamily column_family;

  auto const* const table_name = "projects/test/instances/test/tables/test";
  auto const* const row_key = "0";
  auto const* const column_family_name = "test";
  auto const* const column_qualifier = "test";
  auto const timestamp_micros = 1234;
  auto const* data = "test";

  auto const* const second_column_family_name = "test2";

  std::vector<std::string> column_families = {column_family_name,
                                              second_column_family_name};
  auto maybe_table = CreateTable(table_name, column_families);

  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  std::vector<SetCellParams> v;
  SetCellParams p = {column_family_name, column_qualifier, timestamp_micros,
                     data};
  v.push_back(p);

  p = {second_column_family_name, column_qualifier, timestamp_micros, data};
  v.push_back(p);

  auto status = SetCells(table, table_name, row_key, v);
  ASSERT_STATUS_OK(status);
  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           timestamp_micros, data));
  ASSERT_STATUS_OK(
      HasColumn(table, column_family_name, row_key, column_qualifier));
  ASSERT_STATUS_OK(HasRow(table, column_family_name, row_key));

  // Having established that the data is there, test the basic
  // functionality of the DeleteFromFamily mutation by trying to
  // delete it.
  ASSERT_STATUS_OK(
      DeleteFromFamilies(table, table_name, row_key, {column_family_name}));
  ASSERT_NE(true, HasRow(table, column_family_name, row_key).ok());

  // Ensure that we did not delete a row in another column family.
  ASSERT_EQ(true, HasRow(table, second_column_family_name, row_key).ok());
}

// Test that DeleteFromfamily can be rolled back in case a subsequent
// mutation fails.
TEST(TransactionRollback, DeleteFromFamilyRollback) {
  ::google::bigtable::admin::v2::Table schema;
  ::google::bigtable::admin::v2::ColumnFamily column_family;

  auto const* const table_name = "projects/test/instances/test/tables/test";
  auto const* const row_key = "0";
  auto const* const column_family_name = "test";
  auto const* const column_qualifier = "test";
  auto const timestamp_micros = 1234;
  auto const* data = "test";

  // Failure of one of the mutations is simalted by having a mutation
  // with this column family, which has not been provisioned. Previous
  // successful mutations should be rolled back when RowTransaction
  // sees a mutation with this invalid column family name.
  auto const* const column_family_not_in_schema =
      "i_do_not_exist_in_the_schema";

  std::vector<std::string> column_families = {column_family_name};
  auto maybe_table = CreateTable(table_name, column_families);

  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  std::vector<SetCellParams> v;
  SetCellParams p = {column_family_name, column_qualifier, timestamp_micros,
                     data};
  v.push_back(p);

  auto status = SetCells(table, table_name, row_key, v);
  ASSERT_STATUS_OK(status);
  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           timestamp_micros, data));
  ASSERT_STATUS_OK(
      HasColumn(table, column_family_name, row_key, column_qualifier));
  ASSERT_STATUS_OK(HasRow(table, column_family_name, row_key));

  // Setup two DeleteFromfamily mutation: The first one uses the
  // correct table schema (a column family that exists and is expected
  // to succeed to delete the row saved above. The second one uses a
  // column family not provisioned and should fail, which should
  // trigger a rollback of the previous row deletion. In the end, the
  // above row should still exist and all its data should be intact.
  status =
      DeleteFromFamilies(table, table_name, row_key,
                         {column_family_name, column_family_not_in_schema});
  ASSERT_NE(true, status.ok());  // The overall chain of mutations should fail.

  // Check that the row deleted by the first mutation is restored,
  // with all its data.
  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           timestamp_micros, data));
  ASSERT_STATUS_OK(
      HasColumn(table, column_family_name, row_key, column_qualifier));
  ASSERT_STATUS_OK(HasRow(table, column_family_name, row_key));
}

::google::bigtable::v2::TimestampRange* NewTimestampRange(int64_t start,
                                                          int64_t end) {
  auto* range = new (::google::bigtable::v2::TimestampRange);
  range->set_start_timestamp_micros(start);
  range->set_end_timestamp_micros(end);

  return range;
}

// Does DeleteFromColumn basically work?
TEST(TransactionRollback, DeleteFromColumnBasicFunction) {
  ::google::bigtable::admin::v2::Table schema;
  ::google::bigtable::admin::v2::ColumnFamily column_family;

  auto const* const table_name = "projects/test/instances/test/tables/test";
  auto const* const row_key = "0";
  auto const* const column_family_name = "test";
  auto const* const column_qualifier = "test";
  auto const* data = "test";

  std::vector<std::string> column_families = {column_family_name};
  auto maybe_table = CreateTable(table_name, column_families);

  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  std::vector<SetCellParams> v = {
      {column_family_name, column_qualifier, 1000, data},
      {column_family_name, column_qualifier, 2000, data},
      {column_family_name, column_qualifier, 3000, data},
  };

  auto status = SetCells(table, table_name, row_key, v);
  ASSERT_STATUS_OK(status);
  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           1000, data));
  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           2000, data));
  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           3000, data));

  std::vector<DeleteFromColumnParams> dv = {
      {column_family_name, column_qualifier,
       NewTimestampRange(v[0].timestamp_micros, v[2].timestamp_micros + 1000)}};

  ASSERT_STATUS_OK(DeleteFromColumns(table, table_name, row_key, dv));

  status = HasColumn(table, column_family_name, row_key, column_qualifier);
  ASSERT_EQ(false, status.ok());
}

// Does DeleteFromColumn rollback work?
TEST(TransactionRollback, DeleteFromColumnRollback) {
  ::google::bigtable::admin::v2::Table schema;
  ::google::bigtable::admin::v2::ColumnFamily column_family;

  auto const* const table_name = "projects/test/instances/test/tables/test";
  auto const* const row_key = "0";
  auto const* const column_family_name = "test";
  auto const* const column_qualifier = "test";
  // Simulate mutation failure and cause rollback by attempting a
  // mutation with a non-existent column family name.
  auto const* const bad_column_family_name =
      "this_column_family_does_not_exist";
  auto const* data = "test";

  std::vector<std::string> column_families = {column_family_name};
  auto maybe_table = CreateTable(table_name, column_families);

  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  std::vector<SetCellParams> v = {
      {column_family_name, column_qualifier, 1000, data},
      {column_family_name, column_qualifier, 2000, data},
      {column_family_name, column_qualifier, 3000, data},
  };

  auto status = SetCells(table, table_name, row_key, v);
  ASSERT_STATUS_OK(status);
  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           1000, data));
  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           2000, data));
  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           3000, data));

  // The first mutation will succeed. The second assumes a schema that
  // does not exist - it should fail and cause rollback of the column
  // deletion in the first mutation.
  std::vector<DeleteFromColumnParams> dv = {
      {column_family_name, column_qualifier,
       NewTimestampRange(v[0].timestamp_micros, v[2].timestamp_micros + 1000)},
      {bad_column_family_name, column_qualifier, NewTimestampRange(1000, 2000)},
  };
  // The mutation chains should fail and rollback should occur.
  ASSERT_EQ(false, DeleteFromColumns(table, table_name, row_key, dv).ok());

  // The column should have been restored.
  ASSERT_STATUS_OK(
      HasColumn(table, column_family_name, row_key, column_qualifier));
  // Check that the data is where and what we expect.
  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           1000, data));
  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           2000, data));
  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           3000, data));
}

// Can we delete a row from all column families?
TEST(TransactionRollback, DeleteFromRowBasicFunction) {
  ::google::bigtable::admin::v2::Table schema;
  ::google::bigtable::admin::v2::ColumnFamily column_family;

  auto const* const table_name = "projects/test/instances/test/tables/test";
  auto const* const row_key = "0";
  auto const* const column_family_name = "column_family_1";
  auto const* const column_qualifier = "column_qualifier";
  auto const timestamp_micros = 1000;
  auto const* data = "value";
  auto const* const second_column_family_name = "column_family_2";

  std::vector<std::string> column_families = {column_family_name,
                                              second_column_family_name};
  auto maybe_table = CreateTable(table_name, column_families);

  ASSERT_STATUS_OK(maybe_table);
  auto table = maybe_table.value();

  std::vector<SetCellParams> v;
  SetCellParams p = {column_family_name, column_qualifier, timestamp_micros,
                     data};
  v.push_back(p);

  p = {second_column_family_name, column_qualifier, timestamp_micros, data};
  v.push_back(p);

  auto status = SetCells(table, table_name, row_key, v);
  ASSERT_STATUS_OK(status);
  ASSERT_STATUS_OK(HasCell(table, column_family_name, row_key, column_qualifier,
                           timestamp_micros, data));
  ASSERT_STATUS_OK(
      HasColumn(table, second_column_family_name, row_key, column_qualifier));
  ASSERT_STATUS_OK(HasRow(table, column_family_name, row_key));

  ::google::bigtable::v2::MutateRowRequest mutation_request;
  mutation_request.set_table_name(table_name);
  mutation_request.set_row_key(row_key);

  auto* mutation_request_mutation = mutation_request.add_mutations();
  mutation_request_mutation->mutable_delete_from_row();

  ASSERT_STATUS_OK(table->MutateRow(mutation_request));
  ASSERT_EQ(false, HasCell(table, column_family_name, row_key, column_qualifier,
                           timestamp_micros, data)
                       .ok());
  ASSERT_EQ(false, HasColumn(table, second_column_family_name, row_key,
                             column_qualifier)
                       .ok());
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

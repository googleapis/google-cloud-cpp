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
#include "google/cloud/bigtable/table.h"
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

Status set_cell(
    std::shared_ptr<google::cloud::bigtable::emulator::Table>& table,
    std::string const& table_name, std::string const& row_key,
    std::string const& column_family_name, std::string const& column_qualifier,
    int64_t timestamp_micros, std::string const& data) {
  ::google::bigtable::v2::MutateRowRequest mutation_request;
  mutation_request.set_table_name(table_name);
  mutation_request.set_row_key(row_key);

  auto* mutation_request_mutation = mutation_request.add_mutations();
  auto* set_cell_mutation = mutation_request_mutation->mutable_set_cell();
  set_cell_mutation->set_family_name(column_family_name);
  set_cell_mutation->set_column_qualifier(column_qualifier);
  set_cell_mutation->set_timestamp_micros(timestamp_micros);
  set_cell_mutation->set_value(data);

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

  auto status = set_cell(table, table_name, row_key, column_family_name,
                         column_qualifer, timestamp_micros, data);
  ASSERT_STATUS_OK(status);

  ASSERT_STATUS_OK(has_cell(table, column_family_name, row_key, column_qualifer,
                            timestamp_micros, data));
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

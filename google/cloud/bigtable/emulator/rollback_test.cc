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

#include "google/cloud/bigtable/emulator/table.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <google/bigtable/admin/v2/bigtable_table_admin.pb.h>
#include <google/bigtable/admin/v2/table.pb.h>
#include <google/bigtable/v2/bigtable.grpc.pb.h>
#include <google/bigtable/v2/bigtable.pb.h>
#include <google/bigtable/v2/data.pb.h>
#include <gtest/gtest.h>
#include <memory>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

// Ensure that SetCell still works to set a cell that was not set
// before, when using the RowTransaction class.
TEST(TransactonRollback, SetCellBasicFunction) {
  ::google::bigtable::admin::v2::Table schema;
  ::google::bigtable::admin::v2::ColumnFamily column_family;

  const auto *const table_name = "projects/test/instances/test/tables/test";
  const auto *const row_key = "0";

  schema.set_name(table_name);
  (*schema.mutable_column_families())["test"] = column_family;

  auto maybe_table = Table::Create(schema);
  ASSERT_STATUS_OK(maybe_table);

  auto table = maybe_table.value();

  ::google::bigtable::v2::MutateRowRequest mutation_request;
  mutation_request.set_table_name(table_name);
  mutation_request.set_row_key(row_key);


  auto *mutation_request_mutation = mutation_request.add_mutations();
  auto *set_cell_mutation = mutation_request_mutation->mutable_set_cell();
  set_cell_mutation->set_family_name("test");
  set_cell_mutation->set_column_qualifier("test");
  set_cell_mutation->set_timestamp_micros(1234);
  set_cell_mutation->set_value("test");

  auto status = table->MutateRow(mutation_request);
  ASSERT_STATUS_OK(status);

  auto column_family_it = table->find("test");
  ASSERT_NE(column_family_it, table->end());

  const auto& cf = column_family_it->second;
  auto column_family_row_it = cf->find(row_key);
  ASSERT_NE(column_family_row_it, cf->end());

  auto &column_family_row = column_family_row_it->second;
  auto column_row_it = column_family_row.find("test");
  ASSERT_NE(column_row_it, column_family_row.end());

  auto &column_row = column_row_it->second;
  auto timestamp_it = column_row.find(std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::microseconds(1234)));
  ASSERT_NE(timestamp_it, column_row.end());

  auto value = timestamp_it->second;
  ASSERT_EQ(value, "test");
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

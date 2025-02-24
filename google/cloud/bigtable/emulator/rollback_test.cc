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
#include <google/bigtable/admin/v2/bigtable_table_admin.grpc.pb.h>
#include <google/bigtable/admin/v2/bigtable_table_admin.pb.h>
#include <google/bigtable/admin/v2/table.pb.h>
#include <gtest/gtest.h>
#include <memory>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

// Ensure that SetCell still works to set a cell that was not set
// before.
TEST(TransactonRollback, SetCellBasicFunction) {
  ::google::bigtable::admin::v2::Table schema;
  ::google::bigtable::admin::v2::ColumnFamily column_family;

  schema.set_name("projects/test/instances/test/tables/test");
  (*schema.mutable_column_families())["test"] = column_family;

  auto table = Table::Create(schema);
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

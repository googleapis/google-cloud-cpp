// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gmock/gmock.h>

#include "bigtable/admin/admin_client.h"
#include "bigtable/admin/table_admin.h"
#include "bigtable/client/cell.h"
#include "bigtable/client/data_client.h"
#include "bigtable/client/internal/throw_delegate.h"
#include "bigtable/client/table.h"
#include "bigtable/client/testing/table_integration_test.h"

namespace admin_proto = ::google::bigtable::admin::v2;

namespace bigtable {
namespace testing {

class DataIntegrationTest : public TableIntegrationTest {
 protected:
  /// Use Table::Apply() to insert a single row.
  void Apply(bigtable::Table& table, std::string row_key,
             std::vector<bigtable::Cell> const& cells);

  /// Use Table::BulkApply() to insert multiple rows.
  void BulkApply(bigtable::Table& table,
                 std::vector<bigtable::Cell> const& cells);

  std::string family = "family";
  bigtable::TableConfig table_config = bigtable::TableConfig(
      {{family, bigtable::GcRule::MaxNumVersions(10)}}, {});
};

}  // namespace testing
}  // namespace bigtable

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);

  // Make sure the arguments are valid.
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of("/");
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <instance>" << std::endl;
    return 1;
  }

  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];

  auto admin_client =
      bigtable::CreateDefaultAdminClient(project_id, bigtable::ClientOptions());
  bigtable::TableAdmin admin(admin_client, instance_id);

  auto table_list = admin.ListTables(admin_proto::Table::NAME_ONLY);
  if (not table_list.empty()) {
    std::cerr << "Expected empty instance at the beginning of integration "
              << "test";
    return 1;
  }

  (void)::testing::AddGlobalTestEnvironment(
      new ::bigtable::testing::TableTestEnvironment(project_id, instance_id));

  return RUN_ALL_TESTS();
}

namespace bigtable {
namespace testing {

void DataIntegrationTest::Apply(bigtable::Table& table, std::string row_key,
                                std::vector<bigtable::Cell> const& cells) {
  auto mutation = bigtable::SingleRowMutation(row_key);
  for (auto const& cell : cells) {
    mutation.emplace_back(bigtable::SetCell(cell.family_name(),
                                            cell.column_qualifier(),
                                            cell.timestamp(), cell.value()));
  }
  table.Apply(std::move(mutation));
}

void DataIntegrationTest::BulkApply(bigtable::Table& table,
                                    std::vector<bigtable::Cell> const& cells) {
  std::map<std::string, bigtable::SingleRowMutation> mutations;
  for (auto const& cell : cells) {
    std::string key = cell.row_key();
    auto inserted = mutations.emplace(key, bigtable::SingleRowMutation(key));
    inserted.first->second.emplace_back(
        bigtable::SetCell(cell.family_name(), cell.column_qualifier(),
                          cell.timestamp(), cell.value()));
  }
  bigtable::BulkMutation bulk;
  for (auto& kv : mutations) {
    bulk.emplace_back(std::move(kv.second));
  }
  table.BulkApply(std::move(bulk));
}

TEST_F(DataIntegrationTest, TableApply) {
  std::string const table_name = "table-apply-test";
  auto table = CreateTable(table_name, table_config);
  std::string const row_key = "row-key-1";
  std::vector<bigtable::Cell> created{
      {row_key, family, "c0", 1000, "v1000", {}},
      {row_key, family, "c1", 2000, "v2000", {}}};
  Apply(*table, row_key, created);

  std::vector<bigtable::Cell> expected{
      {row_key, family, "c0", 1000, "v1000", {}},
      {row_key, family, "c1", 2000, "v2000", {}}};

  auto actual = ReadRows(*table, bigtable::Filter::PassAllFilter());
  CheckEqualUnordered(expected, actual);

  DeleteTable(table_name);
}

TEST_F(DataIntegrationTest, TableBulkApply) {
  std::string const table_name = "table-bulk-apply-test";
  auto table = CreateTable(table_name, table_config);

  std::vector<bigtable::Cell> created{
      {"row-key-1", family, "c0", 1000, "v1000", {}},
      {"row-key-1", family, "c1", 2000, "v2000", {}},
      {"row-key-2", family, "c0", 1000, "v1000", {}},
      {"row-key-2", family, "c1", 2000, "v2000", {}},
      {"row-key-3", family, "c0", 1000, "v1000", {}},
      {"row-key-3", family, "c1", 2000, "v2000", {}},
      {"row-key-4", family, "c0", 1000, "v1000", {}},
      {"row-key-4", family, "c1", 2000, "v2000", {}}};

  BulkApply(*table, created);

  auto actual = ReadRows(*table, bigtable::Filter::PassAllFilter());

  std::vector<bigtable::Cell> expected{
      {"row-key-1", family, "c0", 1000, "v1000", {}},
      {"row-key-1", family, "c1", 2000, "v2000", {}},
      {"row-key-2", family, "c0", 1000, "v1000", {}},
      {"row-key-2", family, "c1", 2000, "v2000", {}},
      {"row-key-3", family, "c0", 1000, "v1000", {}},
      {"row-key-3", family, "c1", 2000, "v2000", {}},
      {"row-key-4", family, "c0", 1000, "v1000", {}},
      {"row-key-4", family, "c1", 2000, "v2000", {}}};

  CheckEqualUnordered(expected, actual);

  DeleteTable(table_name);
}

TEST_F(DataIntegrationTest, TableSingleRow) {
  std::string const table_name = "table-single-row-variadic-list-test";
  std::string const row_key = "row-key-1";
  auto table = CreateTable(table_name, table_config);

  auto mutation = bigtable::SingleRowMutation(
      row_key, bigtable::SetCell(family, "c1", 1000, "V1000"),
      bigtable::SetCell(family, "c2", 2000, "V2000"),
      bigtable::SetCell(family, "c3", 3000, "V3000"));

  table->Apply(std::move(mutation));

  auto actual = ReadRows(*table, bigtable::Filter::PassAllFilter());

  std::vector<bigtable::Cell> expected{
      {row_key, family, "c1", 1000, "V1000", {}},
      {row_key, family, "c2", 2000, "V2000", {}},
      {row_key, family, "c3", 3000, "V3000", {}}};

  CheckEqualUnordered(expected, actual);

  DeleteTable(table_name);
}

}  // namespace testing
}  // namespace bigtable

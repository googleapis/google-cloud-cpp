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

#include <absl/memory/memory.h>
#include <absl/strings/str_join.h>

#include "bigtable/admin/admin_client.h"
#include "bigtable/admin/table_admin.h"
#include "bigtable/client/cell.h"
#include "bigtable/client/data_client.h"
#include "bigtable/client/table.h"

namespace admin_proto = ::google::bigtable::admin::v2;

namespace {
/// Store the project and instance captured from the command-line arguments.
class DataTestEnvironment: public ::testing::Environment {
public:
 DataTestEnvironment(std::string project, std::string instance, std::string table_name) {
  project_id_ = std::move(project);
  instance_id_ = std::move(instance);
  table_name_ = std::move(table_name);
 }

 static std::string const& project_id() {
  return project_id_;
 }
 static std::string const& instance_id() {
  return instance_id_;
 }
 static std::string const& table_name() {
   return table_name_;
  }

private:
 static std::string project_id_;
 static std::string instance_id_;
 static std::string table_name_;
};

class DataIntegrationTest: public ::testing::Test {
protected:
 void SetUp() override;

 /**
  * creates the table passed via arguments.
  */
 std::unique_ptr<bigtable::Table> CreateTable();

  /**
  * deletes the table passed via arguments.
  */
 void DeleteTable();
 /**
  * Return all the cells in @p table that pass @p filter.
  */
 std::vector<bigtable::Cell> ReadRows(bigtable::Table& table,
   bigtable::Filter filter);

 void Apply(bigtable::Table& table, std::string row_key,
   std::vector<bigtable::Cell> const& cells);

 void BulkApply(bigtable::Table& table,
   std::vector<bigtable::Cell> const& cells);

 std::shared_ptr<bigtable::AdminClient> admin_client_;
 std::unique_ptr<bigtable::TableAdmin> table_admin_;
 std::shared_ptr<bigtable::DataClient> data_client_;
 std::string family = "family";
};
}  // anonymous namespace

int main(int argc, char* argv[])
try {

 ::testing::InitGoogleTest(&argc, argv);

 // Make sure the arguments are valid.
 if (argc != 4) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(argv[0]).find_last_of("/");
  std::cerr << "Usage: " << cmd.substr(last_slash + 1)
    << " <project> <instance> <table>" << std::endl;
  return 1;
 }
 std::string const project_id = argv[1];
 std::string const instance_id = argv[2];
 std::string const table_name = argv[3];

 (void) ::testing::AddGlobalTestEnvironment(
   new DataTestEnvironment(project_id, instance_id, table_name));

 return RUN_ALL_TESTS();

}
catch (bigtable::PermanentMutationFailure const& ex) {
 std::cerr << "bigtable::PermanentMutationFailure raised: " << ex.what()
   << " - " << ex.status().error_message() << " [" << ex.status().error_code()
   << "], details=" << ex.status().error_details() << std::endl;
 int count = 0;
 for (auto const& failure : ex.failures()) {
  std::cerr << "failure[" << count++ << "] {key="
    << failure.mutation().row_key() << "}" << std::endl;
 }
 return 1;
}
catch (std::exception const& ex) {
 std::cerr << "Standard exception raised: " << ex.what() << std::endl;
 return 1;
}
catch (...) {
 std::cerr << "Unknown exception raised." << std::endl;
 return 1;
}

namespace {
/**
 * Compare two cells, think about the spaceship operator.
 *
 * @return `< 0` if lhs < rhs, `0` if lhs == rhs, and `> 0` otherwise.
 */
int CellCompare(bigtable::Cell const& lhs, bigtable::Cell const& rhs);

/**
 * Compare two sets of cells.
 *
 * Unordered because ReadRows does not guarantee a particular order.
 */
void CheckEqualUnordered(std::vector<bigtable::Cell> expected,
  std::vector<bigtable::Cell> actual);

}  // anonymous namespace

namespace bigtable {
//@{
/// @name Helpers for GTest.
bool operator==(Cell const& lhs, Cell const& rhs) {
 return CellCompare(lhs, rhs) == 0;
}

bool operator<(Cell const& lhs, Cell const& rhs) {
 return CellCompare(lhs, rhs) < 0;
}

/**
 * This function is not used in this file, but it is used by GoogleTest; without
 * it, failing tests will output binary blobs instead of human-readable text.
 */
void PrintTo(bigtable::Cell const& cell, std::ostream* os) {
 *os << "  row_key=" << cell.row_key() << ", family=" << cell.family_name()
   << ", column=" << cell.column_qualifier() << ", timestamp="
   << cell.timestamp() << ", value=" << cell.value() << ", labels={"
   << absl::StrJoin(cell.labels(), ",") << "}";
}
//@}
}// namespace bigtable

TEST_F(DataIntegrationTest, TableApply) {
 auto table = CreateTable();
 std::string const row_key = "row-key-1";
 std::vector<bigtable::Cell> created {
  { row_key, family, "c0", 1000,"v1000", { } },
  { row_key, family, "c1", 2000, "v2000", { } } };
 Apply(*table,row_key, created);

 std::vector<bigtable::Cell> expected {
  { row_key, family, "c0", 1000,"v1000", { } },
  { row_key, family, "c1", 2000, "v2000", { } } };

 auto actual = ReadRows(*table, bigtable::Filter::PassAllFilter());
 CheckEqualUnordered(expected, actual);

 DeleteTable();

}

TEST_F(DataIntegrationTest, TableBulkApply) {
 auto table = CreateTable();

 std::vector<bigtable::Cell> created {
  { "row-key-1", family, "c0", 1000,"v1000", { } },
  { "row-key-1", family, "c1", 2000,"v2000", { } },
  { "row-key-2", family, "c0", 1000,"v1000", { } },
  { "row-key-2", family, "c1", 2000,"v2000", { } },
  { "row-key-3", family, "c0", 1000,"v1000", { } },
  { "row-key-3", family, "c1", 2000,"v2000", { } },
  { "row-key-4", family, "c0", 1000,"v1000", { } },
  { "row-key-4", family, "c1", 2000, "v2000", { } } };

 BulkApply(*table, created);

 auto actual = ReadRows(*table, bigtable::Filter::PassAllFilter());

 std::vector<bigtable::Cell> expected {
  { "row-key-1", family, "c0", 1000,"v1000", { } },
  { "row-key-1", family, "c1", 2000,"v2000", { } },
  { "row-key-2", family, "c0", 1000,"v1000", { } },
  { "row-key-2", family, "c1", 2000,"v2000", { } },
  { "row-key-3", family, "c0", 1000,"v1000", { } },
  { "row-key-3", family, "c1", 2000,"v2000", { } },
  { "row-key-4", family, "c0", 1000,"v1000", { } },
  { "row-key-4", family, "c1", 2000, "v2000", { } } };

 CheckEqualUnordered(expected, actual);

 DeleteTable();

}

namespace {
std::string DataTestEnvironment::project_id_;
std::string DataTestEnvironment::instance_id_;
std::string DataTestEnvironment::table_name_;

void DataIntegrationTest::SetUp() {
 admin_client_ = bigtable::CreateDefaultAdminClient(
   DataTestEnvironment::project_id(), bigtable::ClientOptions());
 table_admin_ = absl::make_unique<bigtable::TableAdmin>(admin_client_,
   DataTestEnvironment::instance_id());
 data_client_ = bigtable::CreateDefaultDataClient(
   DataTestEnvironment::project_id(), DataTestEnvironment::instance_id(),
   bigtable::ClientOptions());
}

std::unique_ptr<bigtable::Table> DataIntegrationTest::CreateTable() {
 table_admin_->CreateTable(DataTestEnvironment::table_name(), bigtable::TableConfig( { { family,
   bigtable::GcRule::MaxNumVersions(10) } }, { }));
 return absl::make_unique<bigtable::Table>(data_client_, DataTestEnvironment::table_name());
}

void DataIntegrationTest::DeleteTable() {
 table_admin_->DeleteTable(DataTestEnvironment::table_name());
}
std::vector<bigtable::Cell> DataIntegrationTest::ReadRows(
  bigtable::Table& table, bigtable::Filter filter) {
 auto reader = table.ReadRows(
   bigtable::RowSet(bigtable::RowRange::Range("", "")), std::move(filter));
 std::vector<bigtable::Cell> result;
 for (auto const& row : reader) {
  std::copy(row.cells().begin(), row.cells().end(), std::back_inserter(result));
 }
 return result;
}

//lets insert a single row at a time
void DataIntegrationTest::Apply(bigtable::Table& table, std::string row_key,
  std::vector<bigtable::Cell> const& cells) {
 auto mutation = bigtable::SingleRowMutation(row_key);
 for (auto const& cell : cells) {
  mutation.emplace_back(bigtable::SetCell(static_cast<std::string>(cell.family_name()),
      static_cast<std::string>(cell.column_qualifier()), cell.timestamp(),
      static_cast<std::string>(cell.value())));
 }
  table.Apply(std::move(mutation));
}

// Insert multiple rows
void DataIntegrationTest::BulkApply(bigtable::Table& table,
  std::vector<bigtable::Cell> const& cells) {
 std::map<std::string, bigtable::SingleRowMutation> mutations;
 for (auto const& cell : cells) {
  std::string key = static_cast<std::string>(cell.row_key());
  auto inserted = mutations.emplace(key, bigtable::SingleRowMutation(key));
  inserted.first->second.emplace_back(
    bigtable::SetCell(static_cast<std::string>(cell.family_name()),
      static_cast<std::string>(cell.column_qualifier()), cell.timestamp(),
      static_cast<std::string>(cell.value())));
 }
 bigtable::BulkMutation bulk;
 for (auto& kv : mutations) {
  bulk.emplace_back(std::move(kv.second));
 }
 table.BulkApply(std::move(bulk));
}

int CellCompare(bigtable::Cell const& lhs, bigtable::Cell const& rhs) {
 auto compare_row_key = lhs.row_key().compare(rhs.row_key());
 if (compare_row_key != 0) {
  return compare_row_key;
 }
 auto compare_family_name = lhs.family_name().compare(rhs.family_name());
 if (compare_family_name != 0) {
  return compare_family_name;
 }
 auto compare_column_qualifier = lhs.column_qualifier().compare(
   rhs.column_qualifier());
 if (compare_column_qualifier != 0) {
  return compare_column_qualifier;
 }
 if (lhs.timestamp() < rhs.timestamp()) {
  return -1;
 }
 if (lhs.timestamp() > rhs.timestamp()) {
  return 1;
 }
 auto compare_value = lhs.value().compare(rhs.value());
 if (compare_value != 0) {
  return compare_value;
 }
 if (lhs.labels() < rhs.labels()) {
  return -1;
 }
 if (lhs.labels() == rhs.labels()) {
  return 0;
 }
 return 1;
}

void CheckEqualUnordered(std::vector<bigtable::Cell> expected,
  std::vector<bigtable::Cell> actual) {
 std::sort(expected.begin(), expected.end());
 std::sort(actual.begin(), actual.end());
 EXPECT_THAT(actual, ::testing::ContainerEq(expected));
}

}

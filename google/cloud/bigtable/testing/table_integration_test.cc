// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/bigtable/testing/table_integration_test.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "absl/memory/memory.h"
#include <google/protobuf/text_format.h>
#include <algorithm>
#include <cctype>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

std::string TableTestEnvironment::project_id_;
std::string TableTestEnvironment::instance_id_;
std::string TableTestEnvironment::zone_a_;
std::string TableTestEnvironment::zone_b_;
google::cloud::internal::DefaultPRNG TableTestEnvironment::generator_;
std::string TableTestEnvironment::table_id_;
bool TableTestEnvironment::using_cloud_bigtable_emulator_;

TableTestEnvironment::TableTestEnvironment() {
  project_id_ =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
  instance_id_ = google::cloud::internal::GetEnv(
                     "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID")
                     .value_or("");
  zone_a_ =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_A")
          .value_or("");
  zone_b_ =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_BIGTABLE_TEST_ZONE_B")
          .value_or("");
}

void TableTestEnvironment::SetUp() {
  ASSERT_FALSE(project_id_.empty());
  ASSERT_FALSE(instance_id_.empty());
  ASSERT_FALSE(zone_a_.empty());
  ASSERT_FALSE(zone_b_.empty());

  using_cloud_bigtable_emulator_ =
      google::cloud::internal::GetEnv("BIGTABLE_EMULATOR_HOST").has_value();

  generator_ = google::cloud::internal::MakeDefaultPRNG();

  auto admin_client = bigtable::CreateDefaultAdminClient(
      TableTestEnvironment::project_id(), ClientOptions());
  auto table_admin =
      bigtable::TableAdmin(admin_client, TableTestEnvironment::instance_id());

  std::string const family1 = "family1";
  std::string const family2 = "family2";
  std::string const family3 = "family3";
  std::string const family4 = "family4";
  auto constexpr kTestMaxVersions = 10;
  auto const test_gc_rule = bigtable::GcRule::MaxNumVersions(kTestMaxVersions);
  bigtable::TableConfig table_config = bigtable::TableConfig(
      {
          {family1, test_gc_rule},
          {family2, test_gc_rule},
          {family3, test_gc_rule},
          {family4, test_gc_rule},
      },
      {});

  table_id_ = RandomTableId();
  ASSERT_STATUS_OK(table_admin.CreateTable(table_id_, table_config));
}

void TableTestEnvironment::TearDown() {
  auto admin_client = bigtable::CreateDefaultAdminClient(
      TableTestEnvironment::project_id(), ClientOptions());
  auto table_admin =
      bigtable::TableAdmin(admin_client, TableTestEnvironment::instance_id());

  ASSERT_STATUS_OK(table_admin.DeleteTable(table_id_));
}

std::string TableTestEnvironment::RandomTableId() {
  return google::cloud::bigtable::testing::RandomTableId(generator_);
}

std::string TableTestEnvironment::RandomBackupId() {
  return google::cloud::bigtable::testing::RandomBackupId(generator_);
}

std::string TableTestEnvironment::RandomInstanceId() {
  return google::cloud::bigtable::testing::RandomInstanceId(generator_);
}

void TableIntegrationTest::SetUp() {
  admin_client_ = bigtable::CreateDefaultAdminClient(
      TableTestEnvironment::project_id(), ClientOptions());
  table_admin_ = absl::make_unique<bigtable::TableAdmin>(
      admin_client_, TableTestEnvironment::instance_id());
  data_client_ = bigtable::CreateDefaultDataClient(
      TableTestEnvironment::project_id(), TableTestEnvironment::instance_id(),
      ClientOptions());

  // In production, we cannot use `DropAllRows()` to cleanup the table because
  // the integration tests sometimes consume all the 'DropRowRangeGroup' quota.
  // Instead we delete the rows, when possible, using BulkApply().
  BulkMutation bulk;
  auto table = GetTable();
  // Bigtable does not support more than 100,000 mutations in a BulkMutation.
  // If we had that many rows then just fallback on DropAllRows(). Most tests
  // only have a small number of rows, so this is a good strategy to save
  // DropAllRows() quota, and should be fast in most cases.
  std::size_t const maximum_mutations = 100000;

  for (auto const& row :
       table.ReadRows(RowRange::InfiniteRange(), Filter::PassAllFilter())) {
    if (!row) {
      break;
    }
    bulk.emplace_back(
        SingleRowMutation(row->row_key(), bigtable::DeleteFromRow()));
    if (bulk.size() > maximum_mutations) {
      break;
    }
  }

  // The version of the emulator distributed with the Cloud SDK does not handle
  // deleted rows correctly:
  //   https://github.com/googleapis/google-cloud-go/issues/1240
  // This has been fixed at HEAD, but the changes have not made it to a version
  // we can use yet. So just use DropAllRows() in that case.
  if (bulk.size() > maximum_mutations || UsingCloudBigtableEmulator()) {
    ASSERT_STATUS_OK(
        table_admin_->DropAllRows(TableTestEnvironment::table_id()));
    return;
  }
  auto failures = table.BulkApply(std::move(bulk));
  for (auto&& f : failures) {
    ASSERT_STATUS_OK(f.status());
  }
}

bigtable::Table TableIntegrationTest::GetTable() {
  return bigtable::Table(data_client_, TableTestEnvironment::table_id());
}

std::vector<bigtable::Cell> TableIntegrationTest::ReadRows(
    bigtable::Table& table, bigtable::Filter filter) {
  auto reader = table.ReadRows(
      bigtable::RowSet(bigtable::RowRange::InfiniteRange()), std::move(filter));
  std::vector<bigtable::Cell> result;
  for (auto const& row : reader) {
    EXPECT_STATUS_OK(row);
    std::copy(row->cells().begin(), row->cells().end(),
              std::back_inserter(result));
  }
  return result;
}

std::vector<bigtable::Cell> TableIntegrationTest::ReadRows(
    std::string const& table_name, bigtable::Filter filter) {
  bigtable::Table table(data_client_, table_name);
  return ReadRows(table, std::move(filter));
}

std::vector<bigtable::Cell> TableIntegrationTest::ReadRows(
    bigtable::Table& table, std::int64_t rows_limit, bigtable::Filter filter) {
  auto reader =
      table.ReadRows(bigtable::RowSet(bigtable::RowRange::InfiniteRange()),
                     rows_limit, std::move(filter));
  std::vector<bigtable::Cell> result;
  for (auto const& row : reader) {
    EXPECT_STATUS_OK(row);
    std::copy(row->cells().begin(), row->cells().end(),
              std::back_inserter(result));
  }
  return result;
}

std::vector<bigtable::Cell> TableIntegrationTest::MoveCellsFromReader(
    bigtable::RowReader& reader) {
  std::vector<bigtable::Cell> result;
  for (auto const& row : reader) {
    EXPECT_STATUS_OK(row);
    std::move(row->cells().begin(), row->cells().end(),
              std::back_inserter(result));
  }
  return result;
}

/// A helper function to create a list of cells.
void TableIntegrationTest::CreateCells(
    bigtable::Table& table, std::vector<bigtable::Cell> const& cells) {
  std::map<RowKeyType, bigtable::SingleRowMutation> mutations;
  for (auto const& cell : cells) {
    using std::chrono::duration_cast;
    using std::chrono::microseconds;
    using std::chrono::milliseconds;
    auto key = cell.row_key();
    auto inserted = mutations.emplace(key, bigtable::SingleRowMutation(key));
    inserted.first->second.emplace_back(bigtable::SetCell(
        cell.family_name(), cell.column_qualifier(),
        duration_cast<milliseconds>(microseconds(cell.timestamp())),
        cell.value()));
  }
  bigtable::BulkMutation bulk;
  for (auto& kv : mutations) {
    bulk.emplace_back(std::move(kv.second));
  }
  auto failures = table.BulkApply(std::move(bulk));
  ASSERT_THAT(failures, ::testing::IsEmpty());
}

std::vector<bigtable::Cell> TableIntegrationTest::GetCellsIgnoringTimestamp(
    std::vector<bigtable::Cell> cells) {
  // Create the expected_cells and actual_cells with same timestamp
  std::vector<bigtable::Cell> return_cells;
  std::transform(cells.begin(), cells.end(), std::back_inserter(return_cells),
                 [](Cell& cell) {
                   return bigtable::Cell(
                       std::move(cell.row_key()), std::move(cell.family_name()),
                       std::move(cell.column_qualifier()), 0,
                       std::move(cell.value()), std::move(cell.labels()));
                 });

  return return_cells;
}

void TableIntegrationTest::CheckEqualUnordered(
    std::vector<bigtable::Cell> expected, std::vector<bigtable::Cell> actual) {
  std::sort(expected.begin(), expected.end());
  std::sort(actual.begin(), actual.end());
  EXPECT_THAT(actual, ::testing::ContainerEq(expected));
}

std::string TableIntegrationTest::RandomTableId() {
  return TableTestEnvironment::RandomTableId();
}

std::string TableIntegrationTest::project_id() {
  return TableTestEnvironment::project_id();
}

std::string TableIntegrationTest::instance_id() {
  return TableTestEnvironment::instance_id();
}

std::string TableIntegrationTest::RandomBackupId() {
  return TableTestEnvironment::RandomBackupId();
}

}  // namespace testing

int CellCompare(bigtable::Cell const& lhs, bigtable::Cell const& rhs) {
  auto compare_row_key = internal::CompareRowKey(lhs.row_key(), rhs.row_key());
  if (compare_row_key != 0) {
    return compare_row_key;
  }
  auto compare_family_name = lhs.family_name().compare(rhs.family_name());
  if (compare_family_name != 0) {
    return compare_family_name;
  }
  auto compare_column_qualifier = internal::CompareColumnQualifiers(
      lhs.column_qualifier(), rhs.column_qualifier());
  if (compare_column_qualifier != 0) {
    return compare_column_qualifier;
  }
  if (lhs.timestamp() < rhs.timestamp()) {
    return -1;
  }
  if (lhs.timestamp() > rhs.timestamp()) {
    return 1;
  }
  auto compare_value = internal::CompareCellValues(lhs.value(), rhs.value());
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
      << ", column=" << cell.column_qualifier()
      << ", timestamp=" << cell.timestamp().count() << ", value=<"
      << cell.value() << ">, labels={";
  char const* del = "";
  for (auto const& label : cell.labels()) {
    *os << del << label;
    del = ",";
  }
  *os << "}";
}
//@}

}  // namespace bigtable
}  // namespace cloud
}  // namespace google

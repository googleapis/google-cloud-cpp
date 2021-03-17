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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_TABLE_INTEGRATION_TEST_H
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_TABLE_INTEGRATION_TEST_H

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/cell.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/integration_test.h"
#include <gmock/gmock.h>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

/// Store the project and instance captured from the command-line arguments.
class TableTestEnvironment : public ::testing::Environment {
 public:
  TableTestEnvironment();

  void SetUp() override;
  void TearDown() override;

  static std::string const& project_id() { return project_id_; }
  static std::string const& instance_id() { return instance_id_; }
  static std::string const& zone_a() { return zone_a_; }
  static std::string const& zone_b() { return zone_b_; }

  /// Return a random table id.
  static std::string RandomTableId();

  /// Return a random backup id.
  static std::string RandomBackupId();

  /// Return a random instance id.
  static std::string RandomInstanceId();

  static std::string const& table_id() { return table_id_; }

  static bool UsingCloudBigtableEmulator() {
    return using_cloud_bigtable_emulator_;
  }

 private:
  static std::string project_id_;
  static std::string instance_id_;
  static std::string zone_a_;
  static std::string zone_b_;

  static google::cloud::internal::DefaultPRNG generator_;

  static std::string table_id_;

  static bool using_cloud_bigtable_emulator_;
};

/**
 * Fixture for integration tests that need to create tables and check their
 * contents.
 */
class TableIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  void SetUp() override;

  /// Gets a Table object for the current test.
  bigtable::Table GetTable();

  /// Return all the cells in @p table that pass @p filter.
  static std::vector<bigtable::Cell> ReadRows(bigtable::Table& table,
                                              bigtable::Filter filter);

  /// Return all the cells in @p table that pass @p filter.
  std::vector<bigtable::Cell> ReadRows(std::string const& table_name,
                                       bigtable::Filter filter);

  static std::vector<bigtable::Cell> ReadRows(bigtable::Table& table,
                                              std::int64_t rows_limit,
                                              bigtable::Filter filter);

  static std::vector<bigtable::Cell> MoveCellsFromReader(
      bigtable::RowReader& reader);

  /// Return all the cells in @p table that pass @p filter.
  static void CreateCells(bigtable::Table& table,
                          std::vector<bigtable::Cell> const& cells);

  /**
   * Return @p cells with all timestamps set to a fixed value.
   *
   * This is useful to compare sets of cells but ignoring their timestamp
   * values.
   */
  static std::vector<bigtable::Cell> GetCellsIgnoringTimestamp(
      std::vector<bigtable::Cell> cells);

  /**
   * Compare two sets of cells.
   * Unordered because ReadRows does not guarantee a particular order.
   */
  static void CheckEqualUnordered(std::vector<bigtable::Cell> expected,
                                  std::vector<bigtable::Cell> actual);

  /**
   * Generate a random table id.
   *
   * We want to run multiple copies of the integration tests on the same Cloud
   * Bigtable instance.  To avoid conflicts and minimize coordination between
   * the tests, we run each test with a randomly selected table name.
   */
  static std::string RandomTableId();

  /**
   * Generate a random backup id.
   *
   * We want to run multiple copies of the integration tests on the same Cloud
   * Bigtable instance.  To avoid conflicts and minimize coordination between
   * the tests, we run each test with a randomly selected backup name.
   */
  static std::string RandomBackupId();

  static std::string project_id();

  static std::string instance_id();

  /// Some tests cannot run on the emulator.
  static bool UsingCloudBigtableEmulator() {
    return TableTestEnvironment::UsingCloudBigtableEmulator();
  }

  static std::vector<std::string> TableNames(
      std::vector<google::bigtable::admin::v2::Table> const& tables) {
    std::vector<std::string> names(tables.size());
    std::transform(
        tables.begin(), tables.end(), names.begin(),
        [](google::bigtable::admin::v2::Table const& x) { return x.name(); });
    return names;
  }

  static std::vector<std::string> BackupNames(
      std::vector<google::bigtable::admin::v2::Backup> const& backups) {
    std::vector<std::string> names(backups.size());
    std::transform(
        backups.begin(), backups.end(), names.begin(),
        [](google::bigtable::admin::v2::Backup const& x) { return x.name(); });
    return names;
  }

  std::shared_ptr<bigtable::AdminClient> admin_client_;
  std::unique_ptr<bigtable::TableAdmin> table_admin_;
  std::shared_ptr<bigtable::DataClient> data_client_;
};

}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_TABLE_INTEGRATION_TEST_H

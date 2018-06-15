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

#ifndef GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_TABLE_INTEGRATION_TEST_H_
#define GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_TABLE_INTEGRATION_TEST_H_

#include "google/cloud/bigtable/admin_client.h"
#include "google/cloud/bigtable/cell.h"
#include "google/cloud/bigtable/data_client.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/internal/random.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace testing {

/// Store the project and instance captured from the command-line arguments.
class TableTestEnvironment : public ::testing::Environment {
 public:
  TableTestEnvironment(std::string project, std::string instance) {
    project_id_ = std::move(project);
    instance_id_ = std::move(instance);
  }

  static std::string const& project_id() { return project_id_; }
  static std::string const& instance_id() { return instance_id_; }

 private:
  static std::string project_id_;
  static std::string instance_id_;
};

/**
 * Fixture for integration tests that need to create tables and check their
 * contents.
 */
class TableIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override;

  /// Creates the table with @p table_config
  std::unique_ptr<bigtable::Table> CreateTable(
      std::string const& table_name, bigtable::TableConfig& table_config);

  /// Deletes the table passed via arguments.
  void DeleteTable(std::string const& table_name);

  /// Return all the cells in @p table that pass @p filter.
  std::vector<bigtable::Cell> ReadRows(bigtable::Table& table,
                                       bigtable::Filter filter);

  std::vector<bigtable::Cell> ReadRows(bigtable::Table& table,
                                       std::int64_t rows_limit,
                                       bigtable::Filter filter);

  std::vector<bigtable::Cell> MoveCellsFromReader(bigtable::RowReader& reader);

  /// Return all the cells in @p table that pass @p filter.
  void CreateCells(bigtable::Table& table,
                   std::vector<bigtable::Cell> const& cells);

  /**
   * Return @p cells with all timestamps set to a fixed value.
   *
   * This is useful to compare sets of cells but ignoring their timestamp
   * values.
   */
  std::vector<bigtable::Cell> GetCellsIgnoringTimestamp(
      std::vector<bigtable::Cell> cells);

  /**
   * Compare two sets of cells.
   * Unordered because ReadRows does not guarantee a particular order.
   */
  void CheckEqualUnordered(std::vector<bigtable::Cell> expected,
                           std::vector<bigtable::Cell> actual);

  /**
   * Generate a random table id.
   *
   * We want to run multiple copies of the integration tests on the same Cloud
   * Bigtable instance.  To avoid conflicts and minimize coordination between
   * the tests, we run each test with a randomly selected table name.
   */
  std::string RandomTableId();

 protected:
  google::cloud::internal::DefaultPRNG generator_ =
      google::cloud::internal::MakeDefaultPRNG();

  std::shared_ptr<bigtable::AdminClient> admin_client_;
  std::unique_ptr<bigtable::TableAdmin> table_admin_;
  std::shared_ptr<bigtable::DataClient> data_client_;
};
}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_TABLE_INTEGRATION_TEST_H_

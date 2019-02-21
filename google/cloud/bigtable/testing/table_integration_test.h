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

  TableTestEnvironment(std::string project, std::string instance,
                       std::string cluster) {
    project_id_ = std::move(project);
    instance_id_ = std::move(instance);
    cluster_id_ = std::move(cluster);
  }

  TableTestEnvironment(std::string project, std::string instance,
                       std::string zone, std::string replication_zone) {
    project_id_ = std::move(project);
    instance_id_ = std::move(instance);
    zone_ = std::move(zone);
    replication_zone_ = std::move(replication_zone);
  }

  void SetUp() override;
  void TearDown() override;

  static std::string const& project_id() { return project_id_; }
  static std::string const& instance_id() { return instance_id_; }
  static std::string const& cluster_id() { return cluster_id_; }
  static std::string const& zone() { return zone_; }
  static std::string const& replication_zone() { return replication_zone_; }

  /**
   * Generate a random string for instance, cluster, or table identifiers.
   *
   * Return a string starting with @p prefix and with @p count random symbols
   * from the [a-z0-9] set. These strings are useful as identifiers for tables,
   * clusters, or instances. Using a random table id allows us to run two
   * instances of a test with minimal chance of interference, without having
   * to coordinate via some global state.
   */
  static std::string CreateRandomId(std::string const& prefix, int count);

  /// Return a random table id.
  static std::string RandomTableId();

  /// Return a random instance id.
  static std::string RandomInstanceId();

  static std::string const& table_id() { return table_id_; }

 private:
  static std::string project_id_;
  static std::string instance_id_;
  static std::string cluster_id_;
  static std::string zone_;
  static std::string replication_zone_;

  static google::cloud::internal::DefaultPRNG generator_;

  static std::string table_id_;
};

/**
 * Fixture for integration tests that need to create tables and check their
 * contents.
 */
class TableIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override;

  /// Gets a Table object for the current test.
  bigtable::Table GetTable();

  /// Return all the cells in @p table that pass @p filter.
  std::vector<bigtable::Cell> ReadRows(bigtable::Table& table,
                                       bigtable::Filter filter);

  /// Return all the cells in @p table that pass @p filter.
  std::vector<bigtable::Cell> ReadRows(std::string table_name,
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
  std::shared_ptr<bigtable::AdminClient> admin_client_;
  std::unique_ptr<bigtable::TableAdmin> table_admin_;
  std::unique_ptr<bigtable::noex::TableAdmin> noex_table_admin_;
  std::shared_ptr<bigtable::DataClient> data_client_;
};
}  // namespace testing
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

#endif  // GOOGLE_CLOUD_CPP_GOOGLE_CLOUD_BIGTABLE_TESTING_TABLE_INTEGRATION_TEST_H_

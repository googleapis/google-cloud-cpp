// Copyright 2020 Google LLC
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

#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include <chrono>
#include <sstream>

namespace {

using google::cloud::bigtable::examples::Usage;
using std::chrono::milliseconds;

void FilterLimitRowSample(google::cloud::bigtable::Table table,
                          std::vector<std::string> const&) {
  //! [START bigtable_filters_limit_row_sample]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::Table table) {
    // Filter the results, only include rows with a given probability
    cbt::Filter filter = cbt::Filter::RowSample(0.75);

    // Read and print the rows.
    for (StatusOr<cbt::Row> const& row :
         table.ReadRows(cbt::RowSet(cbt::RowRange::InfiniteRange()), filter)) {
      if (!row) throw std::runtime_error(row.status().message());
      std::cout << row->row_key();
      for (auto const& cell : row->cells()) {
        std::cout << " = [columns :" << cell.value() << "],";
      }
      std::cout << "\n";
    }
  }
  //! [END bigtable_filters_limit_row_sample]
  (std::move(table));
}

void FilterLimitRowRegex(google::cloud::bigtable::Table table,
                         std::vector<std::string> const&) {
  //! [START bigtable_filters_limit_row_regex]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::Table table) {
    // Create the range of rows to read.
    auto range = cbt::RowRange::Range("key-000010", "key-000030");
    // Filter the results, only include rows where row_key matchs given regular
    // expression
    cbt::Filter filter = cbt::Filter::RowKeysRegex("key.*9$");
    // Read and print the rows.
    for (StatusOr<cbt::Row> const& row : table.ReadRows(range, filter)) {
      if (!row) throw std::runtime_error(row.status().message());
      std::cout << row->row_key();
      for (auto const& cell : row->cells()) {
        std::cout << " = [columns :" << cell.value() << "],";
      }
      std::cout << "\n";
    }
  }
  //! [END bigtable_filters_limit_row_regex]
  (std::move(table));
}

void FilterLimitCellsPerColumn(google::cloud::bigtable::Table table,
                               std::vector<std::string> const&) {
  //! [START bigtable_filters_limit_cells_per_col]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::Table table) {
    // Create the range of rows to read.
    auto range = cbt::RowRange::Range("key-000010", "key-000020");
    // Filter the results, only include limited cells
    cbt::Filter filter = cbt::Filter::Latest(2);
    // Read and print the rows.
    for (StatusOr<cbt::Row> const& row : table.ReadRows(range, filter)) {
      if (!row) throw std::runtime_error(row.status().message());
      std::cout << row->row_key();
      for (auto const& cell : row->cells()) {
        std::cout << " = [columns :" << cell.value() << "],";
      }
      std::cout << "\n";
    }
  }
  //! [END bigtable_filters_limit_cells_per_col]
  (std::move(table));
}

void FilterLimitCellsPerRow(google::cloud::bigtable::Table table,
                            std::vector<std::string> const&) {
  //! [START bigtable_filters_limit_cells_per_row]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::Table table) {
    // Create the range of rows to read.
    auto range = cbt::RowRange::Range("key-000010", "key-000020");
    // Filter the results, only include limited cells per row
    cbt::Filter filter = cbt::Filter::CellsRowLimit(2);
    // Read and print the rows.
    for (StatusOr<cbt::Row> const& row : table.ReadRows(range, filter)) {
      if (!row) throw std::runtime_error(row.status().message());
      std::cout << row->row_key();
      for (auto const& cell : row->cells()) {
        std::cout << " = [columns :" << cell.value() << "],";
      }
      std::cout << "\n";
    }
  }
  //! [END bigtable_filters_limit_cells_per_row]
  (std::move(table));
}

void FilterLimitCellsPerRowOfset(google::cloud::bigtable::Table table,
                                 std::vector<std::string> const&) {
  //! [START bigtable_filters_limit_cells_per_row_offset]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::Table table) {
    // Create the range of rows to read.
    auto range = cbt::RowRange::Range("key-000010", "key-000020");
    cbt::Filter filter = cbt::Filter::CellsRowOffset(2);
    // Read and print the rows.
    for (StatusOr<cbt::Row> const& row : table.ReadRows(range, filter)) {
      if (!row) throw std::runtime_error(row.status().message());
      std::cout << row->row_key();
      for (auto const& cell : row->cells()) {
        std::cout << " = [columns :" << cell.value() << "],";
      }
      std::cout << "\n";
    }
  }
  //! [END bigtable_filters_limit_cells_per_row_offset]
  (std::move(table));
}

// This command just generates data suitable for other examples to run. This
// code is not extracted into the documentation.
void InsertTestData(google::cloud::bigtable::Table table,
                    std::vector<std::string> const&) {
  // Write several rows in a single operation, each row has some trivial data.
  // This is not a code sample in the normal sense, we do not display this code
  // in the documentation. We use it to populate data in the table used to run
  // the actual examples during the CI builds.
  namespace cbt = google::cloud::bigtable;
  cbt::BulkMutation bulk;
  for (int i = 0; i != 100; ++i) {
    // Note: This example uses sequential numeric IDs for simplicity, but
    // this can result in poor performance in a production application.
    // Since rows are stored in sorted order by key, sequential keys can
    // result in poor distribution of operations across nodes.
    //
    // For more information about how to design a Bigtable schema for the
    // best performance, see the documentation:
    //
    //     https://cloud.google.com/bigtable/docs/schema-design
    char buf[32];
    snprintf(buf, sizeof(buf), "key-%06d", i);
    cbt::SingleRowMutation mutation(buf);
    mutation.emplace_back(cbt::SetCell("fam", "col0", milliseconds(1000),
                                       "value0-0-" + std::to_string(i)));
    mutation.emplace_back(cbt::SetCell("fam", "col0", milliseconds(2000),
                                       "value0-1-" + std::to_string(i)));
    mutation.emplace_back(cbt::SetCell("fam", "col1", milliseconds(1000),
                                       "value1-0-" + std::to_string(i)));
    mutation.emplace_back(cbt::SetCell("fam", "col2", milliseconds(1000),
                                       "value2-0-" + std::to_string(i)));
    mutation.emplace_back(cbt::SetCell("fam", "col2", milliseconds(2000),
                                       "value2-1-" + std::to_string(i)));
    mutation.emplace_back(cbt::SetCell("fam", "col2", milliseconds(3000),
                                       "value2-2-" + std::to_string(i)));
    bulk.emplace_back(std::move(mutation));
  }
  auto failures = table.BulkApply(std::move(bulk));
  if (failures.empty()) {
    return;
  }
  std::cerr << "The following mutations failed:\n";
  for (auto const& f : failures) {
    std::cerr << "index[" << f.original_index() << "]=" << f.status() << "\n";
  }
  throw std::runtime_error(failures.front().status().message());
}

std::string DefaultTablePrefix() { return "tbl-data-"; }

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = google::cloud::bigtable;

  if (!argv.empty()) throw google::cloud::bigtable::examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const instance_id = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID")
                               .value();

  cbt::TableAdmin admin(
      cbt::CreateDefaultAdminClient(project_id, cbt::ClientOptions{}),
      instance_id);

  // If a previous run of these samples crashes before cleaning up there may be
  // old tables left over. As there are quotas on the total number of tables we
  // remove stale tables after 48 hours.
  examples::CleanupOldTables(DefaultTablePrefix(), admin);
  examples::CleanupOldTables("mobile-time-series-", admin);

  // Initialize a generator with some amount of entropy.
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());

  auto table_id = examples::RandomTableId(DefaultTablePrefix(), generator);
  auto schema = admin.CreateTable(
      table_id,
      cbt::TableConfig({{"fam", cbt::GcRule::MaxNumVersions(10)}}, {}));
  if (!schema) throw std::runtime_error(schema.status().message());

  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          admin.project(), admin.instance_id(),
          google::cloud::bigtable::ClientOptions()),
      table_id);

  std::cout << "\nPreparing data for multiple examples" << std::endl;
  InsertTestData(table, {});
  std::cout << "Running FilterLimitRowSample() example [1]" << std::endl;
  FilterLimitRowSample(table, {});
  std::cout << "Running FilterLimitRowRegex() example [2]" << std::endl;
  FilterLimitRowRegex(table, {});
  std::cout << "Running FilterLimitCellsPerColumn() example [3]" << std::endl;
  FilterLimitCellsPerColumn(table, {});
  std::cout << "Running FilterLimitCellsPerRow() example [4]" << std::endl;
  FilterLimitCellsPerRow(table, {});
  std::cout << "Running FilterLimitCellsPerRowOffset() example [5]"
            << std::endl;
  FilterLimitCellsPerRowOfset(table, {});
  admin.DeleteTable(table_id);
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  using google::cloud::bigtable::examples::MakeCommandEntry;
  google::cloud::bigtable::examples::Commands commands = {
      MakeCommandEntry("insert-test-data", {}, InsertTestData),
      MakeCommandEntry("filter-limit-row-sample", {}, FilterLimitRowSample),
      MakeCommandEntry("filter-limit-row-regex", {}, FilterLimitRowRegex),
      MakeCommandEntry("filter-limit-cells-per-column", {},
                       FilterLimitCellsPerColumn),
      MakeCommandEntry("filter-limit-cells-per-row", {},
                       FilterLimitCellsPerRow),
      MakeCommandEntry("filter-limit-cells-per-row-offset", {},
                       FilterLimitCellsPerRowOfset),
      {"auto", RunAll},
  };

  google::cloud::bigtable::examples::Example example(std::move(commands));
  return example.Run(argc, argv);
}

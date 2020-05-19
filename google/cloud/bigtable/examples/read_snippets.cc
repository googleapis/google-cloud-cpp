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
#include "google/cloud/internal/format_time_point.h"
//! [bigtable includes]
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
//! [bigtable includes]
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/crash_handler.h"
#include <chrono>
#include <sstream>

namespace {

using google::cloud::bigtable::examples::Usage;

void PrintRow(google::cloud::bigtable::Row const& row) {
  std::cout << "Reading data for " << row.row_key() << "\n";
  std::string col_family;
  for (auto const& cell : row.cells()) {
    if (col_family != cell.family_name()) {
      col_family = cell.family_name();
      std::cout << "Column Family " << col_family << "\n";
    }
    std::cout << "\t" << cell.column_qualifier() << ": " << cell.value() << "@"
              << google::cloud::internal::FormatRfc3339(
                     std::chrono::system_clock::time_point(cell.timestamp()))
              << "\n";
  }
}

void PrepareReadSamples(google::cloud::bigtable::Table table) {
  namespace cbt = google::cloud::bigtable;
  cbt::BulkMutation bulk;

  std::string const column_family_name = "stats_summary";
  auto const timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch());

  bulk.emplace_back(
      cbt::SingleRowMutation("phone#4c410523#20190501",
                             {cbt::SetCell(column_family_name, "connected_cell",
                                           timestamp, std::int64_t{1}),
                              cbt::SetCell(column_family_name, "connected_wifi",
                                           timestamp, std::int64_t{1}),
                              cbt::SetCell(column_family_name, "os_build",
                                           timestamp, "PQ2A.190405.003")}));
  bulk.emplace_back(
      cbt::SingleRowMutation("phone#4c410523#20190502",
                             {cbt::SetCell(column_family_name, "connected_cell",
                                           timestamp, std::int64_t{1}),
                              cbt::SetCell(column_family_name, "connected_wifi",
                                           timestamp, std::int64_t{1}),
                              cbt::SetCell(column_family_name, "os_build",
                                           timestamp, "PQ2A.190405.003")}));
  bulk.emplace_back(
      cbt::SingleRowMutation("phone#4c410523#20190505",
                             {cbt::SetCell(column_family_name, "connected_cell",
                                           timestamp, std::int64_t{0}),
                              cbt::SetCell(column_family_name, "connected_wifi",
                                           timestamp, std::int64_t{1}),
                              cbt::SetCell(column_family_name, "os_build",
                                           timestamp, "PQ2A.190406.000")}));
  bulk.emplace_back(
      cbt::SingleRowMutation("phone#5c10102#20190501",
                             {cbt::SetCell(column_family_name, "connected_cell",
                                           timestamp, std::int64_t{1}),
                              cbt::SetCell(column_family_name, "connected_wifi",
                                           timestamp, std::int64_t{1}),
                              cbt::SetCell(column_family_name, "os_build",
                                           timestamp, "PQ2A.190401.002")}));
  bulk.emplace_back(
      cbt::SingleRowMutation("phone#5c10102#20190502",
                             {cbt::SetCell(column_family_name, "connected_cell",
                                           timestamp, std::int64_t{1}),
                              cbt::SetCell(column_family_name, "connected_wifi",
                                           timestamp, std::int64_t{0}),
                              cbt::SetCell(column_family_name, "os_build",
                                           timestamp, "PQ2A.190406.000")}));

  std::vector<cbt::FailedMutation> failures = table.BulkApply(std::move(bulk));
  if (failures.empty()) {
    std::cout << "All rows successfully written\n";
    return;
  }
  std::cerr << "The following mutations failed:\n";
  for (auto const& f : failures) {
    std::cerr << "index[" << f.original_index() << "]=" << f.status() << "\n";
  }
  throw std::runtime_error(failures.front().status().message());
}

void ReadRowsWithLimit(google::cloud::bigtable::Table table,
                       std::vector<std::string> const&) {
  //! [read rows with limit]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::Table table) {
    // Create the range of rows to read.
    auto range = cbt::RowRange::Range("phone#4c410523#20190501",
                                      "phone#4c410523#20190502");
    // Filter the results, only include values from the "connected_wifi" column
    // in the "stats_summary" column family, and only get the latest value.
    cbt::Filter filter = cbt::Filter::Chain(
        cbt::Filter::ColumnRangeClosed("stats_summary", "connected_wifi",
                                       "connected_wifi"),
        cbt::Filter::Latest(1));
    // Read and print the first 5 rows in the range.
    for (StatusOr<cbt::Row> const& row : table.ReadRows(range, 5, filter)) {
      if (!row) throw std::runtime_error(row.status().message());
      if (row->cells().size() != 1) {
        std::ostringstream os;
        os << "Unexpected number of cells in " << row->row_key();
        throw std::runtime_error(os.str());
      }
      auto const& cell = row->cells().at(0);
      std::cout << cell.row_key() << " = [" << cell.value() << "]\n";
    }
  }
  //! [read rows with limit]
  (std::move(table));
}

void ReadKeysSet(std::vector<std::string> argv) {
  if (argv.size() < 4) {
    throw Usage{
        "read-keys-set <project-id> <instance-id> <table-id>"
        " key1 [key2 ...]"};
  }

  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          argv[0], argv[1], google::cloud::bigtable::ClientOptions()),
      argv[2]);
  argv.erase(argv.begin(), argv.begin() + 3);

  // [START bigtable_read_keys_set]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::Table table, std::vector<std::string> const& row_keys) {
    auto row_set = cbt::RowSet();

    for (auto const& row_key : row_keys) {
      row_set.Append(row_key);
    }

    cbt::Filter filter = cbt::Filter::Latest(1);
    for (StatusOr<cbt::Row>& row : table.ReadRows(std::move(row_set), filter)) {
      if (!row) throw std::runtime_error(row.status().message());
      std::cout << row->row_key() << ":\n";
      for (auto& cell : row->cells()) {
        std::cout << "\t" << cell.family_name() << ":"
                  << cell.column_qualifier() << "    @ "
                  << cell.timestamp().count() << "us\n"
                  << "\t\"" << cell.value() << '"' << "\n";
      }
    }
  }
  // [END bigtable_read_keys_set]
  (std::move(table), argv);
}

void ReadPrefixList(google::cloud::bigtable::Table table,
                    std::vector<std::string> const& argv) {
  //! [read prefix list] [START bigtable_read_prefix_list]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::Table table, std::vector<std::string> const& prefix_list) {
    cbt::Filter filter = cbt::Filter::Latest(1);
    auto row_set = cbt::RowSet();
    for (auto const& prefix : prefix_list) {
      auto row_range_prefix = cbt::RowRange::Prefix(prefix);
      row_set.Append(row_range_prefix);
    }

    for (StatusOr<cbt::Row>& row : table.ReadRows(std::move(row_set), filter)) {
      if (!row) throw std::runtime_error(row.status().message());
      std::cout << row->row_key() << ":\n";
      for (cbt::Cell const& cell : row->cells()) {
        std::cout << "\t" << cell.family_name() << ":"
                  << cell.column_qualifier() << "    @ "
                  << cell.timestamp().count() << "us\n"
                  << "\t\"" << cell.value() << '"' << "\n";
      }
    }
  }
  //! [read prefix list] [END bigtable_read_prefix_list]
  (std::move(table), argv);
}

void ReadRow(google::cloud::bigtable::Table table,
             std::vector<std::string> const& argv) {
  //! [START bigtable_reads_row]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](google::cloud::bigtable::Table table, std::string const& row_key) {
    StatusOr<std::pair<bool, cbt::Row>> tuple =
        table.ReadRow(row_key, cbt::Filter::PassAllFilter());
    if (!tuple) throw std::runtime_error(tuple.status().message());
    if (!tuple->first) {
      std::cout << "Row " << row_key << " not found\n";
      return;
    }
    PrintRow(tuple->second);
  }
  //! [END bigtable_reads_row]
  (std::move(table), argv.at(0));
}

void ReadRowPartial(google::cloud::bigtable::Table table,
                    std::vector<std::string> const& argv) {
  //! [read row] [START bigtable_reads_row_partial]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](google::cloud::bigtable::Table table, std::string const& row_key) {
    StatusOr<std::pair<bool, cbt::Row>> tuple = table.ReadRow(
        row_key, cbt::Filter::ColumnName("stats_summary", "os_build"));
    if (!tuple) throw std::runtime_error(tuple.status().message());
    if (!tuple->first) {
      std::cout << "Row " << row_key << " not found\n";
      return;
    }
    PrintRow(tuple->second);
  }
  //! [read row] [END bigtable_reads_row_partial]
  (std::move(table), argv.at(0));
}

void ReadRows(google::cloud::bigtable::Table table,
              std::vector<std::string> const&) {
  //! [START bigtable_reads_rows]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::Table table) {
    // Read and print the rows.
    for (StatusOr<cbt::Row> const& row : table.ReadRows(
             cbt::RowSet("phone#4c410523#20190501", "phone#4c410523#20190502"),
             cbt::Filter::PassAllFilter())) {
      if (!row) throw std::runtime_error(row.status().message());
      PrintRow(*row);
    }
  }
  //! [END bigtable_reads_rows]
  (std::move(table));
}

void ReadRowRange(google::cloud::bigtable::Table table,
                  std::vector<std::string> const&) {
  //! [read rows] [START bigtable_reads_row_range]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::Table table) {
    // Read and print the rows.
    for (StatusOr<cbt::Row> const& row :
         table.ReadRows(cbt::RowRange::Range("phone#4c410523#20190501",
                                             "phone#4c410523#201906201"),
                        cbt::Filter::PassAllFilter())) {
      if (!row) throw std::runtime_error(row.status().message());
      PrintRow(*row);
    }
  }
  //! [read rows] [END bigtable_reads_row_range]
  (std::move(table));
}

void ReadRowRanges(google::cloud::bigtable::Table table,
                   std::vector<std::string> const&) {
  //! [START bigtable_reads_row_ranges]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::Table table) {
    // Read and print the rows.
    for (StatusOr<cbt::Row> const& row : table.ReadRows(
             cbt::RowSet({cbt::RowRange::Range("phone#4c410523#20190501",
                                               "phone#4c410523#20190601"),
                          cbt::RowRange::Range("phone#5c10102#20190501",
                                               "phone#5c10102#20190601")}),
             cbt::Filter::PassAllFilter())) {
      if (!row) throw std::runtime_error(row.status().message());
      PrintRow(*row);
    }
  }
  //! [END bigtable_reads_row_ranges]
  (std::move(table));
}

void ReadRowPrefix(google::cloud::bigtable::Table table,
                   std::vector<std::string> const&) {
  //! [read rowset prefix] [START bigtable_reads_prefix]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::Table table) {
    // Read and print the rows.
    for (StatusOr<cbt::Row> const& row : table.ReadRows(
             cbt::RowRange::Prefix("phone"), cbt::Filter::PassAllFilter())) {
      if (!row) throw std::runtime_error(row.status().message());
      PrintRow(*row);
    }
  }
  //! [read rowset prefix] [END bigtable_reads_prefix]
  (std::move(table));
}

void ReadFilter(google::cloud::bigtable::Table table,
                std::vector<std::string> const&) {
  //! [START bigtable_reads_filter]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::StatusOr;
  [](cbt::Table table) {
    // Read and print the rows.
    for (StatusOr<cbt::Row> const& row :
         table.ReadRows(cbt::RowRange::InfiniteRange(),
                        cbt::Filter::ValueRegex("PQ2A.*"))) {
      if (!row) throw std::runtime_error(row.status().message());
      PrintRow(*row);
    }
  }
  //! [END bigtable_reads_filter]
  (std::move(table));
}

std::string DefaultTablePrefix() { return "tbl-read-"; }

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
  std::cout << "Creating table " << table_id << std::endl;
  auto schema = admin.CreateTable(
      table_id, cbt::TableConfig(
                    {{"stats_summary", cbt::GcRule::MaxNumVersions(10)}}, {}));
  if (!schema) throw std::runtime_error(schema.status().message());

  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          admin.project(), admin.instance_id(),
          google::cloud::bigtable::ClientOptions()),
      table_id);

  std::cout << "Preparing data for read examples" << std::endl;
  PrepareReadSamples(table);
  std::cout << "Running ReadRow" << std::endl;
  ReadRow(table, {"phone#4c410523#20190501"});
  std::cout << "Running ReadRowPartial" << std::endl;
  ReadRowPartial(table, {"phone#4c410523#20190501"});
  std::cout << "Running ReadRows" << std::endl;
  ReadRows(table, {});
  std::cout << "Running ReadRowRange" << std::endl;
  ReadRowRange(table, {});
  std::cout << "Running ReadRowRanges" << std::endl;
  ReadRowRanges(table, {});
  std::cout << "Running ReadRowPrefix" << std::endl;
  ReadRowPrefix(table, {});
  std::cout << "Running ReadFilter" << std::endl;
  ReadFilter(table, {});
  std::cout << "Running ReadRowsWithLimit() example" << std::endl;
  ReadRowsWithLimit(table, {});

  std::cout << "Running ReadKeySet() example" << std::endl;
  ReadKeysSet({table.project_id(), table.instance_id(), table.table_id(),
               "root/0/0/1", "root/0/1/0"});
  std::cout << "Running ReadPrefixList() example" << std::endl;
  ReadPrefixList(table, {"root/0/1/", "root/2/1/"});

  admin.DeleteTable(table_id);
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InstallCrashHandler(argv[0]);

  using google::cloud::bigtable::examples::MakeCommandEntry;
  google::cloud::bigtable::examples::Commands commands = {
      MakeCommandEntry("read-row", {"<row-key>"}, ReadRow),
      MakeCommandEntry("read-row-partial", {}, ReadRowPartial),
      MakeCommandEntry("read-rows", {}, ReadRows),
      MakeCommandEntry("read-rows-with-limit", {}, ReadRowsWithLimit),
      {"read-keys-set", ReadKeysSet},
      MakeCommandEntry("read-row-range", {}, ReadRowRange),
      MakeCommandEntry("read-row-ranges", {}, ReadRowRanges),
      MakeCommandEntry("read-row-prefix", {}, ReadRowPrefix),
      MakeCommandEntry("read-filter", {}, ReadFilter),
      {"auto", RunAll},
  };

  google::cloud::bigtable::examples::Example example(std::move(commands));
  return example.Run(argc, argv);
}

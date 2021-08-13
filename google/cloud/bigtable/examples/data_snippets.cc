// Copyright 2018 Google LLC
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
#include "google/cloud/bigtable/testing/cleanup_stale_resources.h"
#include "google/cloud/bigtable/testing/random_names.h"
//! [bigtable includes]
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
//! [bigtable includes]
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/crash_handler.h"
#include "absl/strings/str_split.h"
#include <chrono>
#include <sstream>

namespace {

using ::google::cloud::bigtable::examples::Usage;

void Apply(google::cloud::bigtable::Table table,
           std::vector<std::string> const& argv) {
  //! [apply]
  namespace cbt = ::google::cloud::bigtable;
  [](cbt::Table table, std::string const& row_key) {
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());

    cbt::SingleRowMutation mutation(row_key);
    mutation.emplace_back(
        cbt::SetCell("fam", "column0", timestamp, "value for column0"));
    mutation.emplace_back(
        cbt::SetCell("fam", "column1", timestamp, "value for column1"));
    auto status = table.Apply(std::move(mutation));
    if (!status.ok()) throw std::runtime_error(status.message());
  }
  //! [apply]
  (std::move(table), argv.at(0));
}

void ApplyRelaxedIdempotency(google::cloud::bigtable::Table const& table,
                             std::vector<std::string> const& argv) {
  //! [apply relaxed idempotency]
  namespace cbt = ::google::cloud::bigtable;
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& table_id, std::string const& row_key) {
    cbt::Table table(cbt::CreateDefaultDataClient(project_id, instance_id,
                                                  cbt::ClientOptions()),
                     table_id, cbt::AlwaysRetryMutationPolicy());
    // Normally this is not retried on transient failures, because the operation
    // is not idempotent (each retry would set a different timestamp), in this
    // case it would, because the table is setup to always retry.
    cbt::SingleRowMutation mutation(
        row_key, cbt::SetCell("fam", "some-column", "some-value"));
    google::cloud::Status status = table.Apply(std::move(mutation));
    if (!status.ok()) throw std::runtime_error(status.message());
  }
  //! [apply relaxed idempotency]
  (table.project_id(), table.instance_id(), table.table_id(), argv.at(0));
}

void ApplyCustomRetry(google::cloud::bigtable::Table const& table,
                      std::vector<std::string> const& argv) {
  //! [apply custom retry]
  namespace cbt = ::google::cloud::bigtable;
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& table_id, std::string const& row_key) {
    cbt::Table table(cbt::CreateDefaultDataClient(project_id, instance_id,
                                                  cbt::ClientOptions()),
                     table_id, cbt::LimitedErrorCountRetryPolicy(7));
    cbt::SingleRowMutation mutation(
        row_key, cbt::SetCell("fam", "some-column",
                              std::chrono::milliseconds(0), "some-value"));
    google::cloud::Status status = table.Apply(std::move(mutation));
    if (!status.ok()) throw std::runtime_error(status.message());
  }
  //! [apply custom retry]
  (table.project_id(), table.instance_id(), table.table_id(), argv.at(0));
}

void BulkApply(google::cloud::bigtable::Table table,
               std::vector<std::string> const&) {
  //! [bulk apply] [START bigtable_mutate_insert_rows]
  namespace cbt = ::google::cloud::bigtable;
  [](cbt::Table table) {
    // Write several rows in a single operation, each row has some trivial data.
    cbt::BulkMutation bulk;
    for (int i = 0; i != 5000; ++i) {
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
      mutation.emplace_back(
          cbt::SetCell("fam", "col0", "value0-" + std::to_string(i)));
      mutation.emplace_back(
          cbt::SetCell("fam", "col1", "value1-" + std::to_string(i)));
      bulk.emplace_back(std::move(mutation));
    }
    std::vector<cbt::FailedMutation> failures =
        table.BulkApply(std::move(bulk));
    if (failures.empty()) {
      std::cout << "All mutations applied successfully\n";
      return;
    }
    // By default, the `table` object uses the `SafeIdempotentMutationPolicy`
    // which does not retry if any of the mutations fails and are
    // not-idempotent. In this example we simply print such failures, if any,
    // and ignore them otherwise.
    std::cerr << "The following mutations failed and were not retried:\n";
    for (auto const& f : failures) {
      std::cerr << "index[" << f.original_index() << "]=" << f.status() << "\n";
    }
  }
  //! [bulk apply] [END bigtable_mutate_insert_rows]
  (std::move(table));
}

void CheckAndMutate(google::cloud::bigtable::Table table,
                    std::vector<std::string> const& argv) {
  //! [check and mutate]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::Table table, std::string const& row_key) {
    // Check if the latest value of the flip-flop column is "on".
    cbt::Filter predicate = cbt::Filter::Chain(
        cbt::Filter::ColumnRangeClosed("fam", "flip-flop", "flip-flop"),
        cbt::Filter::Latest(1), cbt::Filter::ValueRegex("on"));
    // If the predicate matches, change the latest value to "off", otherwise,
    // change the latest value to "on".  Modify the "flop-flip" column at the
    // same time.
    StatusOr<cbt::MutationBranch> branch =
        table.CheckAndMutateRow(row_key, std::move(predicate),
                                {cbt::SetCell("fam", "flip-flop", "off"),
                                 cbt::SetCell("fam", "flop-flip", "on")},
                                {cbt::SetCell("fam", "flip-flop", "on"),
                                 cbt::SetCell("fam", "flop-flip", "off")});

    if (!branch) throw std::runtime_error(branch.status().message());
    if (*branch == cbt::MutationBranch::kPredicateMatched) {
      std::cout << "The predicate was matched\n";
    } else {
      std::cout << "The predicate was not matched\n";
    }
  }
  //! [check and mutate]
  (std::move(table), argv.at(0));
}

void CheckAndMutateNotPresent(google::cloud::bigtable::Table table,
                              std::vector<std::string> const& argv) {
  //! [check and mutate not present]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::Table table, std::string const& row_key) {
    // Check if the latest value of the "test-column" column is present,
    // regardless of its value.
    cbt::Filter predicate = cbt::Filter::Chain(
        cbt::Filter::ColumnRangeClosed("fam", "test-column", "test-column"),
        cbt::Filter::Latest(1));
    // If the predicate matches, do nothing, otherwise set the
    // "had-test-column" to "false":
    StatusOr<cbt::MutationBranch> branch = table.CheckAndMutateRow(
        row_key, std::move(predicate), {},
        {cbt::SetCell("fam", "had-test-column", "false")});

    if (!branch) throw std::runtime_error(branch.status().message());
    if (*branch == cbt::MutationBranch::kPredicateMatched) {
      std::cout << "The predicate was matched\n";
    } else {
      std::cout << "The predicate was not matched\n";
    }
  }
  //! [check and mutate not present]
  (std::move(table), argv.at(0));
}

void ReadModifyWrite(google::cloud::bigtable::Table table,
                     std::vector<std::string> const& argv) {
  //! [read modify write]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::Table table, std::string const& row_key) {
    StatusOr<cbt::Row> row = table.ReadModifyWriteRow(
        row_key, cbt::ReadModifyWriteRule::IncrementAmount("fam", "counter", 1),
        cbt::ReadModifyWriteRule::AppendValue("fam", "list", ";element"));

    // As the modify in this example is not idempotent, and this example
    // does not attempt to retry if there is a failure, we simply print
    // such failures, if any, and otherwise ignore them.
    if (!row) {
      std::cout << "Failed to append row: " << row.status().message() << "\n";
      return;
    }
    // Print the contents of the row
    std::cout << row->row_key() << "\n";
    for (auto const& cell : row->cells()) {
      std::cout << "    " << cell.family_name() << ":"
                << cell.column_qualifier() << " = <";
      if (cell.column_qualifier() == "counter") {
        // This example uses "counter" to store 64-bit numbers in big-endian
        // format, extract them as follows:
        std::cout << cell.decode_big_endian_integer<std::int64_t>().value();
      } else {
        std::cout << cell.value();
      }
      std::cout << ">\n";
    }
  }
  //! [read modify write]
  (std::move(table), argv.at(0));
}

void SampleRows(google::cloud::bigtable::Table table,
                std::vector<std::string> const&) {
  //! [sample row keys] [START bigtable_table_sample_splits]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::Table table) {
    StatusOr<std::vector<cbt::RowKeySample>> samples = table.SampleRows();
    if (!samples) throw std::runtime_error(samples.status().message());
    for (auto const& sample : *samples) {
      std::cout << "key=" << sample.row_key << " - " << sample.offset_bytes
                << "\n";
    }
  }
  //! [sample row keys] [END bigtable_table_sample_splits]
  (std::move(table));
}

void DeleteAllCells(google::cloud::bigtable::Table table,
                    std::vector<std::string> const& argv) {
  //! [delete all cells]
  namespace cbt = ::google::cloud::bigtable;
  [](cbt::Table table, std::string const& row_key) {
    google::cloud::Status status =
        table.Apply(cbt::SingleRowMutation(row_key, cbt::DeleteFromRow()));

    if (!status.ok()) throw std::runtime_error(status.message());
  }
  //! [delete all cells]
  (std::move(table), argv.at(0));
}

void DeleteFamilyCells(google::cloud::bigtable::Table table,
                       std::vector<std::string> const& argv) {
  //! [delete family cells]
  namespace cbt = ::google::cloud::bigtable;
  [](cbt::Table table, std::string const& row_key,
     std::string const& family_name) {
    // Delete all cells within a family.
    google::cloud::Status status = table.Apply(
        cbt::SingleRowMutation(row_key, cbt::DeleteFromFamily(family_name)));

    if (!status.ok()) throw std::runtime_error(status.message());
  }
  //! [delete family cells]
  (std::move(table), argv.at(0), argv.at(1));
}

void DeleteSelectiveFamilyCells(google::cloud::bigtable::Table table,
                                std::vector<std::string> const& argv) {
  //! [delete selective family cells]
  namespace cbt = ::google::cloud::bigtable;
  [](cbt::Table table, std::string const& row_key,
     std::string const& family_name, std::string const& column_name) {
    // Delete selective cell within a family.
    google::cloud::Status status = table.Apply(cbt::SingleRowMutation(
        row_key, cbt::DeleteFromColumn(family_name, column_name)));

    if (!status.ok()) throw std::runtime_error(status.message());
  }
  //! [delete selective family cells]
  (std::move(table), argv.at(0), argv.at(1), argv.at(2));
}

void RowExists(google::cloud::bigtable::Table table,
               std::vector<std::string> const& argv) {
  //! [row exists]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::StatusOr;
  [](cbt::Table table, std::string const& row_key) {
    // Filter the results, turn any value into an empty string.
    cbt::Filter filter = cbt::Filter::StripValueTransformer();

    // Read a row, this returns a tuple (bool, row)
    StatusOr<std::pair<bool, cbt::Row>> status = table.ReadRow(row_key, filter);
    if (!status) throw std::runtime_error(status.status().message());

    if (!status->first) {
      std::cout << "Row  not found\n";
      return;
    }
    std::cout << "Row exists.\n";
  }
  //! [row exists]
  (std::move(table), argv.at(0));
}

void MutateDeleteColumns(std::vector<std::string> const& argv) {
  if (argv.size() < 5) {
    // Use the same format as the cbt tool to receive mutations from the
    // command-line.
    throw Usage{
        "mutate-delete-columns <project-id> <instance-id>"
        " <table-id> <row-key> <family:column> [<family:column>...]"};
  }

  auto it = argv.cbegin();
  auto const project_id = *it++;
  auto const instance_id = *it++;
  auto const table_id = *it++;
  auto const row_key = *it++;
  std::vector<std::pair<std::string, std::string>> columns;
  for (; it != argv.cend(); ++it) {
    auto pos = it->find_first_of(':');
    if (pos == std::string::npos) {
      throw std::runtime_error("Invalid argument (" + *it +
                               ") should be in family:column format");
    }
    columns.emplace_back(
        std::make_pair(it->substr(0, pos), it->substr(pos + 1)));
  }
  //! [connect data]
  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          project_id, instance_id, google::cloud::bigtable::ClientOptions()),
      table_id);
  //! [connect data]

  // [START bigtable_mutate_delete_columns]
  namespace cbt = ::google::cloud::bigtable;
  [](cbt::Table table, std::string const& key,
     std::vector<std::pair<std::string, std::string>> const& columns) {
    cbt::SingleRowMutation mutation(key);
    for (auto const& c : columns) {
      mutation.emplace_back(cbt::DeleteFromColumn(c.first, c.second));
    }
    google::cloud::Status status = table.Apply(std::move(mutation));
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Columns successfully deleted from row\n";
  }
  // [END bigtable_mutate_delete_columns]
  (std::move(table), row_key, std::move(columns));
}

void MutateDeleteRows(google::cloud::bigtable::Table table,
                      std::vector<std::string> const& argv) {
  // [START bigtable_mutate_delete_rows]
  namespace cbt = ::google::cloud::bigtable;
  [](cbt::Table table, std::vector<std::string> const& keys) {
    cbt::BulkMutation mutation;
    for (auto const& row_key : keys) {
      mutation.emplace_back(
          cbt::SingleRowMutation(row_key, cbt::DeleteFromRow()));
    }
    std::vector<cbt::FailedMutation> failures =
        table.BulkApply(std::move(mutation));
    if (failures.empty()) {
      std::cout << "All rows successfully deleted\n";
      return;
    }
    std::cerr << "The following mutations failed:\n";
    for (auto const& f : failures) {
      std::cerr << "index[" << f.original_index() << "]=" << f.status() << "\n";
    }
  }
  // [END bigtable_mutate_delete_rows]
  (std::move(table), std::move(argv));
}

void MutateDeleteRowsCommand(std::vector<std::string> const& argv) {
  if (argv.size() < 4) {
    // Use the same format as the cbt tool to receive mutations from the
    // command-line.
    throw Usage{
        "mutate-delete-rows <project-id> <instance-id>"
        " <table-id> <row-key> [<row-key>...]"};
  }
  auto it = argv.cbegin();
  auto const project_id = *it++;
  auto const instance_id = *it++;
  auto const table_id = *it++;
  std::vector<std::string> rows(it, argv.cend());
  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          project_id, instance_id, google::cloud::bigtable::ClientOptions()),
      table_id);
  MutateDeleteRows(table, std::move(rows));
}

void MutateInsertUpdateRows(google::cloud::bigtable::Table table,
                            std::vector<std::string> const& argv) {
  // Fortunately region tags can appear more than once, the segments are merged
  // by the region tag processing tools.

  // [START bigtable_insert_update_rows]
  struct InsertOrUpdate {
    std::string column_family;
    std::string column;
    std::string value;
  };
  // [END bigtable_insert_update_rows]

  // A simple, though probably not very efficient, parser for mutations.
  auto parse = [](std::string const& mut) {
    std::vector<std::string> const tokens =
        absl::StrSplit(mut, absl::ByAnyChar(":="));
    if (tokens.size() != 3U)
      throw std::runtime_error("Invalid argument (" + mut +
                               ") should be in family:column=value format");
    return InsertOrUpdate{tokens[0], tokens[1], tokens[2]};
  };

  auto it = argv.cbegin();
  auto const row_key = *it++;
  std::vector<InsertOrUpdate> mutations;
  for (; it != argv.cend(); ++it) {
    mutations.emplace_back(parse(*it));
  }

  // [START bigtable_insert_update_rows]
  namespace cbt = ::google::cloud::bigtable;
  [](cbt::Table table, std::string const& key,
     std::vector<InsertOrUpdate> const& inserts) {
    cbt::SingleRowMutation mutation(key);
    for (auto const& mut : inserts) {
      mutation.emplace_back(
          cbt::SetCell(mut.column_family, mut.column, mut.value));
    }
    google::cloud::Status status = table.Apply(std::move(mutation));
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Row successfully updated\n";
  }
  // [END bigtable_insert_update_rows]
  (std::move(table), row_key, std::move(mutations));
}

void MutateInsertUpdateRowsCommand(std::vector<std::string> const& argv) {
  if (argv.size() < 5) {
    // Use the same format as the cbt tool to receive mutations from the
    // command-line.
    throw Usage{
        "mutate-insert-update-rows <project-id> <instance-id>"
        " <table-id> <row-key> <family:column=value> "
        "[<family:column=value>...]"};
  }

  auto it = argv.cbegin();
  auto const project_id = *it++;
  auto const instance_id = *it++;
  auto const table_id = *it++;
  std::vector<std::string> rows(it, argv.cend());
  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          project_id, instance_id, google::cloud::bigtable::ClientOptions()),
      table_id);
  MutateInsertUpdateRows(table, std::move(rows));
}

void RenameColumn(google::cloud::bigtable::Table table,
                  std::vector<std::string> const& argv) {
  // [START bigtable_mutate_mix_match]
  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::Status;
  using ::google::cloud::StatusOr;
  [](cbt::Table table, std::string const& key, std::string const& family,
     std::string const& old_name, std::string const& new_name) {
    StatusOr<std::pair<bool, cbt::Row>> row =
        table.ReadRow(key, cbt::Filter::ColumnName(family, old_name));

    if (!row) throw std::runtime_error(row.status().message());
    if (!row->first) throw std::runtime_error("Cannot find row " + key);

    cbt::SingleRowMutation mutation(key);
    for (auto const& cell : row->second.cells()) {
      // Create a new cell
      auto timestamp_in_milliseconds =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              cell.timestamp());
      mutation.emplace_back(cbt::SetCell(family, new_name,
                                         timestamp_in_milliseconds,
                                         std::move(cell).value()));
    }
    mutation.emplace_back(cbt::DeleteFromColumn("fam", old_name));

    google::cloud::Status status = table.Apply(std::move(mutation));
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Row successfully updated\n";
  }
  // [END bigtable_mutate_mix_match]
  (std::move(table), argv.at(0), argv.at(1), argv.at(2), argv.at(3));
}

// This command just generates data suitable for other examples to run. This
// code is not extracted into the documentation.
void InsertTestData(google::cloud::bigtable::Table table,
                    std::vector<std::string> const&) {
  // Write several rows in a single operation, each row has some trivial data.
  // This is not a code sample in the normal sense, we do not display this code
  // in the documentation. We use it to populate data in the table used to run
  // the actual examples during the CI builds.
  namespace cbt = ::google::cloud::bigtable;
  cbt::BulkMutation bulk;
  for (int i = 0; i != 5000; ++i) {
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
    mutation.emplace_back(cbt::SetCell("fam", "col0",
                                       std::chrono::milliseconds(0),
                                       "value0-" + std::to_string(i)));
    mutation.emplace_back(cbt::SetCell("fam", "col1",
                                       std::chrono::milliseconds(0),
                                       "value1-" + std::to_string(i)));
    mutation.emplace_back(cbt::SetCell("fam", "col2",
                                       std::chrono::milliseconds(0),
                                       "value2-" + std::to_string(i)));
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

// This command just generates data suitable for other examples to run. This
// code is not extracted into the documentation.
void PopulateTableHierarchy(google::cloud::bigtable::Table table,
                            std::vector<std::string> const&) {
  namespace cbt = ::google::cloud::bigtable;
  // Write several rows.
  int q = 0;
  for (int i = 0; i != 4; ++i) {
    for (int j = 0; j != 4; ++j) {
      for (int k = 0; k != 4; ++k) {
        std::string row_key = "root/" + std::to_string(i) + "/";
        row_key += std::to_string(j) + "/";
        row_key += std::to_string(k);
        cbt::SingleRowMutation mutation(row_key);
        mutation.emplace_back(cbt::SetCell("fam", "col0",
                                           std::chrono::milliseconds(0),
                                           "value-" + std::to_string(q)));
        ++q;
        google::cloud::Status status = table.Apply(std::move(mutation));
        if (!status.ok()) throw std::runtime_error(status.message());
      }
    }
  }
}

void WriteSimple(google::cloud::bigtable::Table table,
                 std::vector<std::string> const&) {
  // [START bigtable_writes_simple]
  namespace cbt = ::google::cloud::bigtable;
  [](cbt::Table table) {
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());

    std::string row_key = "phone#4c410523#20190501";
    cbt::SingleRowMutation mutation(row_key);
    std::string column_family = "stats_summary";

    mutation.emplace_back(cbt::SetCell(column_family, "connected_cell",
                                       timestamp, std::int64_t{1}));
    mutation.emplace_back(cbt::SetCell(column_family, "connected_wifi",
                                       timestamp, std::int64_t{1}));
    mutation.emplace_back(
        cbt::SetCell(column_family, "os_build", timestamp, "PQ2A.190405.003"));
    google::cloud::Status status = table.Apply(std::move(mutation));
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Successfully wrote row" << row_key << "\n";
  }
  // [END bigtable_writes_simple]
  (std::move(table));
}

void WriteBatch(google::cloud::bigtable::Table table,
                std::vector<std::string> const&) {
  // [START bigtable_writes_batch]
  namespace cbt = ::google::cloud::bigtable;
  [](cbt::Table table) {
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    std::string column_family = "stats_summary";

    cbt::BulkMutation bulk;
    bulk.emplace_back(cbt::SingleRowMutation(
        "tablet#a0b81f74#20190501",
        cbt::SetCell(column_family, "connected_cell", timestamp,
                     std::int64_t{1}),
        cbt::SetCell(column_family, "os_build", timestamp, "12155.0.0-rc1")));
    bulk.emplace_back(cbt::SingleRowMutation(
        "tablet#a0b81f74#20190502",
        cbt::SetCell(column_family, "connected_cell", timestamp,
                     std::int64_t{1}),
        cbt::SetCell(column_family, "os_build", timestamp, "12145.0.0-rc6")));

    std::vector<cbt::FailedMutation> failures =
        table.BulkApply(std::move(bulk));
    if (failures.empty()) {
      std::cout << "Successfully wrote 2 rows.\n";
      return;
    }
    std::cerr << "The following mutations failed:\n";
    for (auto const& f : failures) {
      std::cerr << "rowkey[" << f.original_index() << "]=" << f.status()
                << "\n";
    }
  }
  // [END bigtable_writes_batch]
  (std::move(table));
}

void WriteIncrement(google::cloud::bigtable::Table table,
                    std::vector<std::string> const&) {
  // [START bigtable_writes_increment]
  namespace cbt = ::google::cloud::bigtable;
  [](cbt::Table table) {
    std::string row_key = "phone#4c410523#20190501";
    std::string column_family = "stats_summary";

    google::cloud::StatusOr<cbt::Row> row = table.ReadModifyWriteRow(
        row_key, cbt::ReadModifyWriteRule::IncrementAmount(
                     column_family, "connected_wifi", -1));

    if (!row) throw std::runtime_error(row.status().message());
    std::cout << "Successfully updated row" << row->row_key() << "\n";
  }
  // [END bigtable_writes_increment]
  (std::move(table));
}

void WriteConditionally(google::cloud::bigtable::Table table,
                        std::vector<std::string> const&) {
  // [START bigtable_writes_conditional]
  namespace cbt = ::google::cloud::bigtable;
  [](cbt::Table table) {
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());

    std::string row_key = "phone#4c410523#20190501";
    cbt::SingleRowMutation mutation(row_key);
    std::string column_family = "stats_summary";
    cbt::Filter predicate = cbt::Filter::Chain(
        cbt::Filter::ColumnName(column_family, "os_build"),
        cbt::Filter::Latest(1), cbt::Filter::ValueRegex("PQ2A\\..*"));

    google::cloud::StatusOr<cbt::MutationBranch> branch =
        table.CheckAndMutateRow(
            row_key, std::move(predicate),
            {cbt::SetCell(column_family, "os_name", timestamp, "android")}, {});

    if (!branch) throw std::runtime_error(branch.status().message());
    if (*branch == cbt::MutationBranch::kPredicateMatched) {
      std::cout << "Successfully updated row\n";
    } else {
      std::cout << "The predicate was not matched\n";
    }
  }
  // [END bigtable_writes_conditional]
  (std::move(table));
}

void ConfigureConnectionPoolSize(std::vector<std::string> const& argv) {
  // [START bigtable_configure_connection_pool]
  namespace cbt = ::google::cloud::bigtable;
  [](std::string const& project_id, std::string const& instance_id,
     std::string const& table_id) {
    auto constexpr kPoolSize = 10;
    auto table = cbt::Table(
        cbt::CreateDefaultDataClient(
            project_id, instance_id,
            cbt::ClientOptions().set_connection_pool_size(kPoolSize)),
        table_id);
    std::cout << "Connected with channel pool size of " << kPoolSize << "\n";
  }
  // [END bigtable_configure_connection_pool]
  (argv.at(0), argv.at(1), argv.at(2));
}

void RunMutateExamples(google::cloud::bigtable::TableAdmin admin,
                       google::cloud::internal::DefaultPRNG& generator) {
  namespace cbt = ::google::cloud::bigtable;

  auto table_id = google::cloud::bigtable::testing::RandomTableId(generator);
  auto schema = admin.CreateTable(
      table_id,
      cbt::TableConfig({{"fam", cbt::GcRule::MaxNumVersions(10)}}, {}));
  if (!schema) throw std::runtime_error(schema.status().message());

  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          admin.project(), admin.instance_id(),
          google::cloud::bigtable::ClientOptions()),
      table_id, cbt::AlwaysRetryMutationPolicy());

  std::cout << "Running MutateInsertUpdateRows() example [1]" << std::endl;
  MutateInsertUpdateRows(table, {"row1", "fam:col1=value1.1",
                                 "fam:col2=value1.2", "fam:col3=value1.3"});
  std::cout << "Running MutateInsertUpdateRows() example [2]" << std::endl;
  MutateInsertUpdateRows(table, {"row2", "fam:col1=value2.1",
                                 "fam:col2=value2.2", "fam:col3=value2.3"});

  admin.DeleteTable(table_id);
}

void RunWriteExamples(google::cloud::bigtable::TableAdmin admin,
                      google::cloud::internal::DefaultPRNG& generator) {
  namespace cbt = ::google::cloud::bigtable;

  auto table_id = google::cloud::bigtable::testing::RandomTableId(generator);
  auto schema = admin.CreateTable(
      table_id, cbt::TableConfig(
                    {{"stats_summary", cbt::GcRule::MaxNumVersions(11)}}, {}));
  if (!schema) throw std::runtime_error(schema.status().message());

  // Some temporary variables to make the snippet below more readable.
  auto const project_id = admin.project();
  auto const instance_id = admin.instance_id();
  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          project_id, instance_id, google::cloud::bigtable::ClientOptions()),
      table_id, cbt::AlwaysRetryMutationPolicy());

  std::cout << "Running WriteSimple() example" << std::endl;
  WriteSimple(table, {});
  std::cout << "Running WriteBatch() example" << std::endl;
  WriteBatch(table, {});
  std::cout << "Running WriteIncrement() example" << std::endl;
  WriteIncrement(table, {});
  std::cout << "Running WriteConditionally() example" << std::endl;
  WriteConditionally(table, {});

  admin.DeleteTable(table_id);
}

void RunDataExamples(google::cloud::bigtable::TableAdmin admin,
                     google::cloud::internal::DefaultPRNG& generator) {
  namespace cbt = ::google::cloud::bigtable;

  auto table_id = google::cloud::bigtable::testing::RandomTableId(generator);
  std::cout << "Creating table " << table_id << std::endl;
  auto schema = admin.CreateTable(
      table_id,
      cbt::TableConfig({{"fam", cbt::GcRule::MaxNumVersions(10)}}, {}));
  if (!schema) throw std::runtime_error(schema.status().message());

  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          admin.project(), admin.instance_id(),
          google::cloud::bigtable::ClientOptions()),
      table_id, cbt::AlwaysRetryMutationPolicy());

  std::cout << "\nRunning ConfigureConnectionPoolSize()" << std::endl;
  ConfigureConnectionPoolSize({admin.project(), admin.instance_id(), table_id});

  std::cout << "\nPreparing data for MutateDeleteColumns()" << std::endl;
  MutateInsertUpdateRows(
      table, {"insert-update-01", "fam:col0=value0-0", "fam:col1=value2-0",
              "fam:col3=value3-0", "fam:col4=value4-0"});
  std::cout << "Running MutateDeleteColumns() example" << std::endl;
  MutateDeleteColumns({table.project_id(), table.instance_id(),
                       table.table_id(), "insert-update-01", "fam:col3",
                       "fam:col4"});
  std::cout << "Running MutateDeleteRows() example [1]" << std::endl;
  MutateDeleteRows(table, {"insert-update-01"});

  std::cout << "\nPreparing data for MutateDeleteRows()" << std::endl;
  MutateInsertUpdateRows(
      table, {"to-delete-01", "fam:col0=value0-0", "fam:col1=value2-0",
              "fam:col3=value3-0", "fam:col4=value4-0"});
  MutateInsertUpdateRows(
      table, {"to-delete-02", "fam:col0=value0-0", "fam:col1=value2-0",
              "fam:col3=value3-0", "fam:col4=value4-0"});
  std::cout << "Running MutateDeleteRows() example [2]" << std::endl;
  MutateDeleteRows(table, {"to-delete-01", "to-delete-02"});

  std::cout << "\nPreparing data for RenameColumn()" << std::endl;
  MutateInsertUpdateRows(table, {"mix-match-01", "fam:col0=value0-0"});
  MutateInsertUpdateRows(table, {"mix-match-01", "fam:col0=value0-1"});
  MutateInsertUpdateRows(table, {"mix-match-01", "fam:col0=value0-2"});
  std::cout << "Running RenameColumn() example" << std::endl;
  RenameColumn(table, {"mix-match-01", "fam", "col0", "new-name"});

  std::cout << "\nPreparing data for multiple examples" << std::endl;
  InsertTestData(table, {});
  std::cout << "Running Apply() example" << std::endl;
  Apply(table, {"test-key-for-apply"});
  std::cout << "Running Apply() with relaxed idempotency example" << std::endl;
  ApplyRelaxedIdempotency(table, {"apply-relaxed-idempotency"});
  std::cout << "Running Apply() with custom retry example" << std::endl;
  ApplyCustomRetry(table, {"apply-custom-retry"});
  std::cout << "Running BulkApply() example" << std::endl;
  BulkApply(table, {});

  std::cout << "\nPopulate data for prefix and row set examples" << std::endl;
  PopulateTableHierarchy(table, {});

  std::cout << "Running SampleRows() example" << std::endl;
  SampleRows(table, {});

  std::cout << "Running RowExists example" << std::endl;
  RowExists(table, {"root/0/0/1"});
  std::cout << "Running DeleteAllCells example" << std::endl;
  DeleteAllCells(table, {"root/0/0/1"});
  std::cout << "Running DeleteFamilyCells() example" << std::endl;
  DeleteFamilyCells(table, {"root/0/1/0", "fam"});
  std::cout << "Running DeleteSelectiveFamilyCells() example" << std::endl;
  DeleteSelectiveFamilyCells(table, {"root/0/1/0", "fam", "col2"});

  std::cout << "\nPopulating data for CheckAndMutate() example" << std::endl;
  MutateInsertUpdateRows(table, {"check-and-mutate-row", "fam:flip-flop:on"});
  MutateInsertUpdateRows(
      table, {"check-and-mutate-row-not-present", "fam:unused=unused-value"});
  std::cout << "Running CheckAndMutate() example [1]" << std::endl;
  CheckAndMutate(table, {"check-and-mutate-row"});
  std::cout << "Running CheckAndMutate() example [2]" << std::endl;
  CheckAndMutate(table, {"check-and-mutate-row"});
  std::cout << "Running CheckAndMutate() example [3]" << std::endl;
  CheckAndMutateNotPresent(table, {"check-and-mutate-row-not-present"});
  std::cout << "Running CheckAndMutate() example [4]" << std::endl;
  MutateInsertUpdateRows(
      table, {"check-and-mutate-row-not-present", "fam:unused=unused-value"});
  CheckAndMutateNotPresent(table, {"check-and-mutate-row-not-present"});

  std::cout << "\nRunning ReadModifyWrite() example [1]" << std::endl;
  ReadModifyWrite(table, {"read-modify-write"});
  std::cout << "Running ReadModifyWrite() example [2]" << std::endl;
  ReadModifyWrite(table, {"read-modify-write"});
  std::cout << "Running ReadModifyWrite() example [3]" << std::endl;
  ReadModifyWrite(table, {"read-modify-write"});

  admin.DeleteTable(table_id);
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = ::google::cloud::bigtable;

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
  google::cloud::bigtable::testing::CleanupStaleTables(admin);

  // Initialize a generator with some amount of entropy.
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  RunMutateExamples(admin, generator);
  RunWriteExamples(admin, generator);
  RunDataExamples(admin, generator);
}

}  // anonymous namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  google::cloud::testing_util::InstallCrashHandler(argv[0]);

  using ::google::cloud::bigtable::examples::MakeCommandEntry;
  google::cloud::bigtable::examples::Commands commands = {
      MakeCommandEntry("apply", {"<row-key>"}, Apply),
      MakeCommandEntry("apply-relaxed-idempotency", {"<row-key>"},
                       ApplyRelaxedIdempotency),
      MakeCommandEntry("apply-custom-retry", {"<row-key>"}, ApplyCustomRetry),
      MakeCommandEntry("bulk-apply", {}, BulkApply),
      MakeCommandEntry("check-and-mutate", {"<row-key>"}, CheckAndMutate),
      MakeCommandEntry("check-and-mutate-not-present", {"<row-key>"},
                       CheckAndMutateNotPresent),
      MakeCommandEntry("read-modify-write", {"<row-key>"}, ReadModifyWrite),
      MakeCommandEntry("sample-rows", {}, SampleRows),
      MakeCommandEntry("delete-all-cells", {"<row-key>"}, DeleteAllCells),
      MakeCommandEntry("delete-family-cells", {"<row-key>", "<family-name>"},
                       DeleteFamilyCells),
      MakeCommandEntry("delete-selective-family-cells",
                       {"<row-key>", "<family-name>", "<column-name>"},
                       DeleteSelectiveFamilyCells),
      MakeCommandEntry("row-exists", {"<row-key>"}, RowExists),
      {"mutate-delete-columns", MutateDeleteColumns},
      {"mutate-delete-rows", MutateDeleteRowsCommand},
      {"mutate-insert-update-rows", MutateInsertUpdateRowsCommand},
      MakeCommandEntry("rename-column",
                       {"<row-key> <family> <old-name> <new-name>"},
                       RenameColumn),
      MakeCommandEntry("insert-test-data", {}, InsertTestData),
      MakeCommandEntry("populate-table-hierarchy", {}, PopulateTableHierarchy),
      MakeCommandEntry("write-simple", {}, WriteSimple),
      MakeCommandEntry("write-batch", {}, WriteBatch),
      MakeCommandEntry("write-increment", {}, WriteIncrement),
      MakeCommandEntry("write-conditional", {}, WriteConditionally),
      {"auto", RunAll},
  };

  google::cloud::bigtable::examples::Example example(std::move(commands));
  return example.Run(argc, argv);
}

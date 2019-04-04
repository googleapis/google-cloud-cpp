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

//! [all code]

//! [bigtable includes]
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
//! [bigtable includes]
#include <google/protobuf/text_format.h>
#include <deque>
#include <list>
#include <sstream>

namespace {
/**
 * The key used for ReadRow(), ReadModifyWrite(), CheckAndMutate.
 *
 * Using the same key makes it possible for the user to see the effect of
 * the different APIs on one row.
 */
const std::string MAGIC_ROW_KEY = "key-000009";

struct Usage {
  std::string msg;
};

char const* ConsumeArg(int& argc, char* argv[]) {
  if (argc < 2) {
    return nullptr;
  }
  char const* result = argv[1];
  std::copy(argv + 2, argv + argc, argv + 1);
  argc--;
  return result;
}

std::string command_usage;

void PrintUsage(int argc, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << "\n";
}

void Apply(google::cloud::bigtable::Table table, int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"apply: <project-id> <instance-id> <table-id>"};
  }

  //! [apply] [START bigtable_create_row]
  [](google::cloud::bigtable::Table table) {
    // Write several rows with some trivial data.
    for (int i = 0; i != 20; ++i) {
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
      google::cloud::bigtable::SingleRowMutation mutation(buf);
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col0", "value0-" + std::to_string(i)));
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col1", "value2-" + std::to_string(i)));
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col2", "value3-" + std::to_string(i)));
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col3", "value4-" + std::to_string(i)));
      google::cloud::Status status = table.Apply(std::move(mutation));
      if (!status.ok()) {
        throw std::runtime_error(status.message());
      }
    }
  }
  //! [apply] [END bigtable_create_row]
  (std::move(table));
}

void BulkApply(google::cloud::bigtable::Table table, int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"bulk-apply: <project-id> <instance-id> <table-id>"};
  }

  //! [bulk apply] [START bigtable_mutate_insert_rows]
  [](google::cloud::bigtable::Table table) {
    // Write several rows in a single operation, each row has some trivial data.
    google::cloud::bigtable::BulkMutation bulk;
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
      google::cloud::bigtable::SingleRowMutation mutation(buf);
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col0", "value0-" + std::to_string(i)));
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col1", "value2-" + std::to_string(i)));
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col2", "value3-" + std::to_string(i)));
      mutation.emplace_back(google::cloud::bigtable::SetCell(
          "fam", "col3", "value4-" + std::to_string(i)));
      bulk.emplace_back(std::move(mutation));
    }
    auto failures = table.BulkApply(std::move(bulk));
    if (!failures.empty()) {
      auto status = failures.front().status();
      throw std::runtime_error(status.message());
    }
  }
  //! [bulk apply] [END bigtable_mutate_insert_rows]
  (std::move(table));
}

void ReadRow(google::cloud::bigtable::Table table, int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"read-row: <project-id> <instance-id> <table-id>"};
  }

  //! [read row] [START bigtable_get_row] [START bigtable_read_error]
  [](google::cloud::bigtable::Table table) {
    // Filter the results, only include the latest value on each cell.
    auto filter = google::cloud::bigtable::Filter::Latest(1);
    // Read a row, this returns a tuple (bool, row)
    auto tuple = table.ReadRow(MAGIC_ROW_KEY, std::move(filter));
    if (!tuple) {
      throw std::runtime_error(tuple.status().message());
    }
    if (!tuple->first) {
      std::cout << "Row " << MAGIC_ROW_KEY << " not found\n";
      return;
    }
    std::cout << "key: " << tuple->second.row_key() << "\n";
    for (auto& cell : tuple->second.cells()) {
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
    std::cout << std::flush;
  }
  //! [read row] [END bigtable_get_row] [END bigtable_read_error]
  (std::move(table));
}

void ReadRows(google::cloud::bigtable::Table table, int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"read-rows: <project-id> <instance-id> <table-id>"};
  }

  //! [read rows] [START bigtable_get_rows] [START bigtable_table_readstream]
  // [START bigtable_read_range]
  [](google::cloud::bigtable::Table table) {
    // Create the range of rows to read.
    auto range =
        google::cloud::bigtable::RowRange::Range("key-000010", "key-000020");
    // Filter the results, only include values from the "col0" column in the
    // "fam" column family, and only get the latest value.
    auto filter = google::cloud::bigtable::Filter::Chain(
        google::cloud::bigtable::Filter::ColumnRangeClosed("fam", "col0",
                                                           "col0"),
        google::cloud::bigtable::Filter::Latest(1));
    // Read and print the rows.
    for (auto const& row : table.ReadRows(range, filter)) {
      if (!row) {
        throw std::runtime_error(row.status().message());
      }
      if (row->cells().size() != 1) {
        std::ostringstream os;
        os << "Unexpected number of cells in " << row->row_key();
        throw std::runtime_error(os.str());
      }
      auto const& cell = row->cells().at(0);
      std::cout << cell.row_key() << " = [" << cell.value() << "]\n";
    }
    std::cout << std::flush;
  }
  // [END bigtable_read_range]
  //! [read rows] [END bigtable_get_rows] [END bigtable_table_readstream]
  (std::move(table));
}

void ReadRowsWithLimit(google::cloud::bigtable::Table table, int argc,
                       char* argv[]) {
  if (argc != 1) {
    throw Usage{"read-rows-with-limit: <project-id> <instance-id> <table-id>"};
  }

  //! [read rows with limit] [START bigtable_read_filter]
  [](google::cloud::bigtable::Table table) {
    // Create the range of rows to read.
    auto range =
        google::cloud::bigtable::RowRange::Range("key-000010", "key-000020");
    // Filter the results, only include values from the "col0" column in the
    // "fam" column family, and only get the latest value.
    auto filter = google::cloud::bigtable::Filter::Chain(
        google::cloud::bigtable::Filter::ColumnRangeClosed("fam", "col0",
                                                           "col0"),
        google::cloud::bigtable::Filter::Latest(1));
    // Read and print the first 5 rows in the range.
    for (auto const& row : table.ReadRows(range, 5, filter)) {
      if (!row) {
        throw std::runtime_error(row.status().message());
      }
      if (row->cells().size() != 1) {
        std::ostringstream os;
        os << "Unexpected number of cells in " << row->row_key();
        throw std::runtime_error(os.str());
      }
      auto const& cell = row->cells().at(0);
      std::cout << cell.row_key() << " = [" << cell.value() << "]\n";
    }
    std::cout << std::flush;
  }
  //! [read rows with limit] [END bigtable_read_filter]
  (std::move(table));
}

void PopulateTableHierarchy(google::cloud::bigtable::Table table, int argc,
                            char* argv[]) {
  if (argc != 1) {
    throw Usage{
        "populate-table-hierarchy: <project-id> <instance-id> <table-id>"};
  }

  //! [populate table hierarchy]
  [](google::cloud::bigtable::Table table) {
    // Write several rows.
    int q = 0;
    for (int i = 0; i != 4; ++i) {
      for (int j = 0; j != 4; ++j) {
        for (int k = 0; k != 4; ++k) {
          std::string row_key = "root/" + std::to_string(i) + "/";
          row_key += std::to_string(j) + "/";
          row_key += std::to_string(k);
          google::cloud::bigtable::SingleRowMutation mutation(row_key);
          mutation.emplace_back(google::cloud::bigtable::SetCell(
              "fam", "col0", "value-" + std::to_string(q)));
          ++q;
          google::cloud::Status status = table.Apply(std::move(mutation));
          if (!status.ok()) {
            throw std::runtime_error(status.message());
          }
        }
      }
    }
  }
  //! [populate table hierarchy]
  (std::move(table));
}

void ReadRowSet(google::cloud::bigtable::Table table, int argc, char* argv[]) {
  if (argc < 2) {
    throw Usage{
        "read-rowset: <project-id> <instance-id> <table-id> [row_keys]"};
  }

  std::vector<std::string> row_keys;
  while (argc > 1) {
    row_keys.emplace_back(ConsumeArg(argc, argv));
  }

  //! [read rowset]
  [&row_keys](google::cloud::bigtable::Table table) {
    namespace cbt = google::cloud::bigtable;
    auto row_set = cbt::RowSet();

    for (auto row_key : row_keys) {
      row_set.Append(row_key);
    }

    auto filter = google::cloud::bigtable::Filter::Latest(1);

    for (auto& row : table.ReadRows(std::move(row_set), filter)) {
      if (!row) {
        throw std::runtime_error(row.status().message());
      }
      std::cout << row->row_key() << ":\n";
      for (auto& cell : row->cells()) {
        std::cout << "\t" << cell.family_name() << ":"
                  << cell.column_qualifier() << "    @ "
                  << cell.timestamp().count() << "us\n"
                  << "\t\"" << cell.value() << '"' << "\n";
      }
    }
    std::cout << std::flush;
  }
  //! [read rowset]
  (std::move(table));
}

void ReadRowSetPrefix(google::cloud::bigtable::Table table, int argc,
                      char* argv[]) {
  if (argc != 2) {
    throw Usage{
        "read-rowset-prefix: <project-id> <instance-id> <table-id> <prefix>"};
  }

  std::string prefix = ConsumeArg(argc, argv);

  //! [read rowset prefix] [START bigtable_read_prefix]
  [](google::cloud::bigtable::Table table, std::string prefix) {
    namespace cbt = google::cloud::bigtable;
    auto row_set = cbt::RowSet();

    auto range_prefix = cbt::RowRange::Prefix(prefix);
    row_set.Append(range_prefix);

    auto filter = google::cloud::bigtable::Filter::Latest(1);

    for (auto& row : table.ReadRows(std::move(row_set), filter)) {
      if (!row) {
        throw std::runtime_error(row.status().message());
      }
      std::cout << row->row_key() << ":\n";
      for (auto& cell : row->cells()) {
        std::cout << "\t" << cell.family_name() << ":"
                  << cell.column_qualifier() << "    @ "
                  << cell.timestamp().count() << "us\n"
                  << "\t\"" << cell.value() << '"' << "\n";
      }
    }
    std::cout << std::flush;
  }
  //! [read rowset prefix] [END bigtable_read_prefix]
  (std::move(table), prefix);
}

void ReadPrefixList(google::cloud::bigtable::Table table, int argc,
                    char* argv[]) {
  if (argc < 2) {
    throw Usage{
        "read-prefix-list: <project-id> <instance-id> <table-id> "
        "[prefixes]"};
  }

  std::vector<std::string> prefix_list;
  while (argc > 1) {
    prefix_list.emplace_back(ConsumeArg(argc, argv));
  }

  //! [read prefix list] [START bigtable_read_prefix_list]
  [](google::cloud::bigtable::Table table,
     std::vector<std::string> prefix_list) {
    namespace cbt = google::cloud::bigtable;
    auto row_set = cbt::RowSet();
    auto filter = google::cloud::bigtable::Filter::Latest(1);

    for (auto prefix : prefix_list) {
      auto row_range_prefix = cbt::RowRange::Prefix(prefix);
      row_set.Append(row_range_prefix);
    }

    for (auto& row : table.ReadRows(std::move(row_set), filter)) {
      if (!row) {
        throw std::runtime_error(row.status().message());
      }
      std::cout << row->row_key() << ":\n";
      for (auto& cell : row->cells()) {
        std::cout << "\t" << cell.family_name() << ":"
                  << cell.column_qualifier() << "    @ "
                  << cell.timestamp().count() << "us\n"
                  << "\t\"" << cell.value() << '"' << "\n";
      }
    }
    std::cout << std::flush;
  }
  //! [read prefix list] [END bigtable_read_prefix_list]
  (std::move(table), prefix_list);
}

void CheckAndMutate(google::cloud::bigtable::Table table, int argc,
                    char* argv[]) {
  if (argc != 1) {
    throw Usage{"check-and-mutate: <project-id> <instance-id> <table-id>"};
  }

  //! [check and mutate] [START bigtable_row_filter]
  [](google::cloud::bigtable::Table table) {
    // Check if the latest value of the flip-flop column is "on".
    auto predicate = google::cloud::bigtable::Filter::Chain(
        google::cloud::bigtable::Filter::ColumnRangeClosed("fam", "flip-flop",
                                                           "flip-flop"),
        google::cloud::bigtable::Filter::Latest(1),
        google::cloud::bigtable::Filter::ValueRegex("on"));
    // If the predicate matches, change the latest value to "off", otherwise,
    // change the latest value to "on".  Modify the "flop-flip" column at the
    // same time.
    auto mut = table.CheckAndMutateRow(
        MAGIC_ROW_KEY, std::move(predicate),
        {google::cloud::bigtable::SetCell("fam", "flip-flop", "off"),
         google::cloud::bigtable::SetCell("fam", "flop-flip", "on")},
        {google::cloud::bigtable::SetCell("fam", "flip-flop", "on"),
         google::cloud::bigtable::SetCell("fam", "flop-flip", "off")});

    if (!mut) {
      throw std::runtime_error(mut.status().message());
    }
  }
  //! [check and mutate] [END bigtable_row_filter]
  (std::move(table));
}

void ReadModifyWrite(google::cloud::bigtable::Table table, int argc,
                     char* argv[]) {
  if (argc != 1) {
    throw Usage{"read-modify-write: <project-id> <instance-id> <table-id>"};
  }

  //! [read modify write] [START bigtable_row_increment]
  [](google::cloud::bigtable::Table table) {
    auto row = table.ReadModifyWriteRow(
        MAGIC_ROW_KEY,
        google::cloud::bigtable::ReadModifyWriteRule::IncrementAmount(
            "fam", "counter", 1),
        google::cloud::bigtable::ReadModifyWriteRule::AppendValue("fam", "list",
                                                                  ";element"));

    if (!row) {
      throw std::runtime_error(row.status().message());
    }
    std::cout << row->row_key() << "\n";
  }
  //! [read modify write] [END bigtable_row_increment]
  (std::move(table));
}

void SampleRows(google::cloud::bigtable::Table table, int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"sample-rows: <project-id> <instance-id> <table-id>"};
  }

  //! [sample row keys] [START bigtable_table_sample_splits]
  [](google::cloud::bigtable::Table table) {
    auto samples = table.SampleRows<>();
    if (!samples) {
      throw std::runtime_error(samples.status().message());
    }
    for (auto const& sample : *samples) {
      std::cout << "key=" << sample.row_key << " - " << sample.offset_bytes
                << "\n";
    }
    std::cout << std::flush;
  }
  //! [sample row keys] [END bigtable_table_sample_splits]
  (std::move(table));
}

void SampleRowsCollections(google::cloud::bigtable::Table table, int argc,
                           char* argv[]) {
  if (argc != 1) {
    throw Usage{
        "sample-rows-collections: <project-id> <instance-id> <table-id>"};
  }

  //! [sample row keys collections]
  [](google::cloud::bigtable::Table table) {
    auto list_samples = table.SampleRows<std::list>();
    if (!list_samples) {
      throw std::runtime_error(list_samples.status().message());
    }
    for (auto const& sample : *list_samples) {
      std::cout << "key=" << sample.row_key << " - " << sample.offset_bytes
                << "\n";
    }
    auto deque_samples = table.SampleRows<std::deque>();
    if (!deque_samples) {
      throw std::runtime_error(deque_samples.status().message());
    }
    for (auto const& sample : *deque_samples) {
      std::cout << "key=" << sample.row_key << " - " << sample.offset_bytes
                << "\n";
    }
    std::cout << std::flush;
  }
  //! [sample row keys collections]
  (std::move(table));
}

void GetFamily(google::cloud::bigtable::Table table, int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"get-family: <project-id> <instance-id> <table-id>"};
  }

  //! [get family] [START bigtable_get_family] [START bigtable_family_ref]
  [](google::cloud::bigtable::Table table) {
    // Create the range of rows to read.
    auto range = google::cloud::bigtable::RowRange::InfiniteRange();

    // Filter the results, only get the latest value
    auto filter = google::cloud::bigtable::Filter::Latest(1);

    // Read and print the family name.
    for (auto const& row : table.ReadRows(range, filter)) {
      if (!row) {
        throw std::runtime_error(row.status().message());
      }
      auto const& cell = row->cells().at(0);
      std::cout << cell.family_name() << "\n";
      break;
    }
    std::cout << std::flush;
  }
  //! [get family] [END bigtable_get_family] [END bigtable_family_ref]
  (std::move(table));
}

void DeleteAllCells(google::cloud::bigtable::Table table, int argc,
                    char* argv[]) {
  if (argc != 2) {
    throw Usage{
        "delete-all-cells: <project-id> <instance-id> <table-id> <row-key>"};
  }
  auto row_key = ConsumeArg(argc, argv);

  //! [delete all cells] [START bigtable_delete_all_cells]
  [](google::cloud::bigtable::Table table, std::string row_key) {
    auto status = table.Apply(google::cloud::bigtable::SingleRowMutation(
        row_key, google::cloud::bigtable::DeleteFromRow()));

    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
  }
  //! [delete all cells] [END bigtable_delete_all_cells]
  (std::move(table), row_key);
}

void DeleteFamilyCells(google::cloud::bigtable::Table table, int argc,
                       char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "delete-family-cells: <project-id> <instance-id> <table-id> "
        "<row-key> <family-name>"};
  }
  auto row_key = ConsumeArg(argc, argv);
  auto family_name = ConsumeArg(argc, argv);

  //! [delete family cells] [START bigtable_delete_family_cells]
  [](google::cloud::bigtable::Table table, std::string row_key,
     std::string family_name) {
    // Delete all cells within a family.
    auto status = table.Apply(google::cloud::bigtable::SingleRowMutation(
        row_key, google::cloud::bigtable::DeleteFromFamily(family_name)));

    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
  }
  //! [delete family cells] [END bigtable_delete_family_cells]
  (std::move(table), row_key, family_name);
}

void DeleteSelectiveFamilyCells(google::cloud::bigtable::Table table, int argc,
                                char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "delete-selective-family-cells: <project-id> <instance-id> <table-id> "
        "<row-key> <family-name> <column-name>"};
  }
  auto row_key = ConsumeArg(argc, argv);
  auto family_name = ConsumeArg(argc, argv);
  auto column_name = ConsumeArg(argc, argv);

  //! [delete selective family cells]
  // [START bigtable_delete_selective_family_cells]
  [](google::cloud::bigtable::Table table, std::string row_key,
     std::string family_name, std::string column_name) {
    // Delete selective cell within a family.
    auto status = table.Apply(google::cloud::bigtable::SingleRowMutation(
        row_key,
        google::cloud::bigtable::DeleteFromColumn(family_name, column_name)));

    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
  }
  // [END bigtable_delete_selective_family_cells]
  //! [delete selective family cells]

  (std::move(table), row_key, family_name, column_name);
}
}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType =
      std::function<void(google::cloud::bigtable::Table, int, char* [])>;

  std::map<std::string, CommandType> commands = {
      {"apply", &Apply},
      {"bulk-apply", &BulkApply},
      {"read-row", &ReadRow},
      {"read-rows", &ReadRows},
      {"populate-table-hierarchy", &PopulateTableHierarchy},
      {"read-rowset", &ReadRowSet},
      {"read-rowset-prefix", &ReadRowSetPrefix},
      {"read-prefix-list", &ReadPrefixList},
      {"read-rows-with-limit", &ReadRowsWithLimit},
      {"check-and-mutate", &CheckAndMutate},
      {"read-modify-write", &ReadModifyWrite},
      {"sample-rows", &SampleRows},
      {"sample-rows-collections", &SampleRowsCollections},
      {"get-family", &GetFamily},
      {"delete-all-cells", &DeleteAllCells},
      {"delete-family-cells", &DeleteFamilyCells},
      {"delete-selective-family-cells", &DeleteSelectiveFamilyCells}};

  {
    // Force each command to generate its Usage string, so we can provide a good
    // usage string for the whole program. We need to create an Table
    // object to do this, but that object is never used, it is passed to the
    // commands, without any calls made to it.
    google::cloud::bigtable::Table unused(
        google::cloud::bigtable::CreateDefaultDataClient(
            "unused-project", "Unused-instance",
            google::cloud::bigtable::ClientOptions()),
        "Unused-table");
    for (auto&& kv : commands) {
      try {
        int fake_argc = 0;
        kv.second(unused, fake_argc, argv);
      } catch (Usage const& u) {
        command_usage += "    ";
        command_usage += u.msg;
        command_usage += "\n";
      } catch (...) {
        // ignore other exceptions.
      }
    }
  }

  if (argc < 5) {
    PrintUsage(argc, argv,
               "Missing command and/or project-id/ or instance-id or table-id");
    return 1;
  }

  std::string const command_name = ConsumeArg(argc, argv);
  std::string const project_id = ConsumeArg(argc, argv);
  std::string const instance_id = ConsumeArg(argc, argv);
  std::string const table_id = ConsumeArg(argc, argv);

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    PrintUsage(argc, argv, "Unknown command: " + command_name);
    return 1;
  }

  // Connect to the Cloud Bigtable endpoint.
  //! [connect data]
  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          project_id, instance_id, google::cloud::bigtable::ClientOptions()),
      table_id);
  //! [connect data]

  command->second(table, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
//! [all code]

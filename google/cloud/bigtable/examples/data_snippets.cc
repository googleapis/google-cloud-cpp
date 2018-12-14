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
            << command_usage << std::endl;
}

void Apply(google::cloud::bigtable::Table table, int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"apply: <project-id> <instance-id> <table-id>"};
  }

  //! [apply]
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
      table.Apply(std::move(mutation));
    }
  }
  //! [apply]
  (std::move(table));
}

void BulkApply(google::cloud::bigtable::Table table, int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"bulk-apply: <project-id> <instance-id> <table-id>"};
  }

  //! [bulk apply]
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
    table.BulkApply(std::move(bulk));
  }
  //! [bulk apply]
  (std::move(table));
}

void ReadRow(google::cloud::bigtable::Table table, int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"read-row: <project-id> <instance-id> <table-id>"};
  }

  //! [read row]
  [](google::cloud::bigtable::Table table) {
    // Filter the results, only include the latest value on each cell.
    auto filter = google::cloud::bigtable::Filter::Latest(1);
    // Read a row, this returns a tuple (bool, row)
    std::pair<bool, google::cloud::bigtable::Row> tuple =
        table.ReadRow(MAGIC_ROW_KEY, std::move(filter));
    if (not tuple.first) {
      std::cout << "Row " << MAGIC_ROW_KEY << " not found" << std::endl;
      return;
    }
    std::cout << "key: " << tuple.second.row_key() << "\n";
    for (auto& cell : tuple.second.cells()) {
      std::cout << "    " << cell.family_name() << ":"
                << cell.column_qualifier() << " = <";
      if (cell.column_qualifier() == "counter") {
        // This example uses "counter" to store 64-bit numbers in BigEndian
        // format, extract them as follows:
        std::cout
            << cell.value_as<google::cloud::bigtable::bigendian64_t>().get();
      } else {
        std::cout << cell.value();
      }
      std::cout << ">\n";
    }
    std::cout << std::flush;
  }
  //! [read row]
  (std::move(table));
}

void ReadRows(google::cloud::bigtable::Table table, int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"read-rows: <project-id> <instance-id> <table-id>"};
  }

  //! [read rows]
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
      if (row.cells().size() != 1) {
        std::ostringstream os;
        os << "Unexpected number of cells in " << row.row_key();
        throw std::runtime_error(os.str());
      }
      auto const& cell = row.cells().at(0);
      std::cout << cell.row_key() << " = [" << cell.value() << "]\n";
    }
    std::cout << std::flush;
  }
  //! [read rows]
  (std::move(table));
}

void ReadRowsWithLimit(google::cloud::bigtable::Table table, int argc,
                       char* argv[]) {
  if (argc != 1) {
    throw Usage{"read-rows-with-limit: <project-id> <instance-id> <table-id>"};
  }

  //! [read rows with limit]
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
      if (row.cells().size() != 1) {
        std::ostringstream os;
        os << "Unexpected number of cells in " << row.row_key();
        throw std::runtime_error(os.str());
      }
      auto const& cell = row.cells().at(0);
      std::cout << cell.row_key() << " = [" << cell.value() << "]\n";
    }
    std::cout << std::flush;
  }
  //! [read rows with limit]
  (std::move(table));
}

void CheckAndMutate(google::cloud::bigtable::Table table, int argc,
                    char* argv[]) {
  if (argc != 1) {
    throw Usage{"check-and-mutate: <project-id> <instance-id> <table-id>"};
  }

  //! [check and mutate]
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
    table.CheckAndMutateRow(
        MAGIC_ROW_KEY, std::move(predicate),
        {google::cloud::bigtable::SetCell("fam", "flip-flop", "off"),
         google::cloud::bigtable::SetCell("fam", "flop-flip", "on")},
        {google::cloud::bigtable::SetCell("fam", "flip-flop", "on"),
         google::cloud::bigtable::SetCell("fam", "flop-flip", "off")});
  }
  //! [check and mutate]
  (std::move(table));
}

void ReadModifyWrite(google::cloud::bigtable::Table table, int argc,
                     char* argv[]) {
  if (argc != 1) {
    throw Usage{"read-modify-write: <project-id> <instance-id> <table-id>"};
  }

  //! [read modify write]
  [](google::cloud::bigtable::Table table) {
    auto row = table.ReadModifyWriteRow(
        MAGIC_ROW_KEY,
        google::cloud::bigtable::ReadModifyWriteRule::IncrementAmount(
            "fam", "counter", 1),
        google::cloud::bigtable::ReadModifyWriteRule::AppendValue("fam", "list",
                                                                  ";element"));
    std::cout << row.row_key() << "\n";
  }
  //! [read modify write]
  (std::move(table));
}

void SampleRows(google::cloud::bigtable::Table table, int argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"sample-rows: <project-id> <instance-id> <table-id>"};
  }

  //! [sample row keys]
  [](google::cloud::bigtable::Table table) {
    auto samples = table.SampleRows<>();
    for (auto const& sample : samples) {
      std::cout << "key=" << sample.row_key << " - " << sample.offset_bytes
                << "\n";
    }
    std::cout << std::flush;
  }
  //! [sample row keys]
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
    for (auto const& sample : list_samples) {
      std::cout << "key=" << sample.row_key << " - " << sample.offset_bytes
                << "\n";
    }
    auto deque_samples = table.SampleRows<std::deque>();
    for (auto const& sample : deque_samples) {
      std::cout << "key=" << sample.row_key << " - " << sample.offset_bytes
                << "\n";
    }
    std::cout << std::flush;
  }
  //! [sample row keys collections]
  (std::move(table));
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
      {"read-rows-with-limit", &ReadRowsWithLimit},
      {"check-and-mutate", &CheckAndMutate},
      {"read-modify-write", &ReadModifyWrite},
      {"sample-rows", &SampleRows},
      {"sample-rows-collections", &SampleRowsCollections},
  };

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
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}
//! [all code]

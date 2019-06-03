// Copyright 2019 Google LLC
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
//! [bigtable includes]
#include <google/protobuf/text_format.h>
#include <sstream>

namespace {
struct Usage {
  std::string msg;
};

std::string command_usage;

void PrintUsage(std::string const& cmd, std::string const& msg) {
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << "\n";
}

void AsyncApply(google::cloud::bigtable::Table table,
                google::cloud::bigtable::CompletionQueue cq,
                std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{"async-apply: <project-id> <instance-id> <table-id> <row-key>"};
  }

  //! [async-apply]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::Table table, cbt::CompletionQueue cq, std::string table_id,
     std::string row_key) {
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());

    cbt::SingleRowMutation mutation(row_key);
    mutation.emplace_back(cbt::SetCell("fam", "some-column", "some-value"));
    mutation.emplace_back(
        cbt::SetCell("fam", "another-column", "another-value"));
    mutation.emplace_back(cbt::SetCell("fam", "even-more-columns", timestamp,
                                       "with-explicit-timestamp"));

    future<google::cloud::Status> status_future =
        table.AsyncApply(std::move(mutation), cq);
    auto status = status_future.get();
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
    std::cout << "Successfully applied mutation\n";
  }
  //! [async-apply]
  (std::move(table), std::move(cq), argv[1], argv[2]);
}

void AsyncBulkApply(google::cloud::bigtable::Table table,
                    google::cloud::bigtable::CompletionQueue cq,
                    std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-bulk-apply: <project-id> <instance-id> <table-id>"};
  }

  //! [bulk async-bulk-apply]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  [](cbt::Table table, cbt::CompletionQueue cq, std::string table_id) {
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
          cbt::SetCell("fam", "col1", "value2-" + std::to_string(i)));
      mutation.emplace_back(
          cbt::SetCell("fam", "col2", "value3-" + std::to_string(i)));
      mutation.emplace_back(
          cbt::SetCell("fam", "col3", "value4-" + std::to_string(i)));
      bulk.emplace_back(std::move(mutation));
    }

    table.AsyncBulkApply(std::move(bulk), cq)
        .then([](future<std::vector<cbt::FailedMutation>> ft) {
          auto failures = ft.get();
          if (failures.empty()) {
            std::cout << "All the mutations were successful\n";
            return;
          }
          std::cerr << "The following mutations failed:\n";
          for (auto const& f : failures) {
            std::cerr << "index[" << f.original_index() << "]=" << f.status()
                      << "\n";
          }
          throw std::runtime_error(failures.front().status().message());
        })
        .get();  // block to simplify the example
  }
  //! [bulk async-bulk-apply]
  (std::move(table), std::move(cq), argv[1]);
}

void AsyncReadRows(google::cloud::bigtable::Table table,
                   google::cloud::bigtable::CompletionQueue cq,
                   std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{"async-read-rows: <project-id> <instance-id> <table-id>"};
  }

  //! [async read rows]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::make_ready_future;
  using google::cloud::promise;
  using google::cloud::Status;
  [](cbt::CompletionQueue cq, cbt::Table table) {
    // Create the range of rows to read.
    auto range = cbt::RowRange::Range("key-000010", "key-000020");
    // Filter the results, only include values from the "col0" column in the
    // "fam" column family, and only get the latest value.
    auto filter = cbt::Filter::Chain(
        cbt::Filter::ColumnRangeClosed("fam", "col0", "col0"),
        cbt::Filter::Latest(1));
    promise<Status> stream_status_promise;
    // Read and print the rows.
    table.AsyncReadRows(cq,
                        [](cbt::Row row) {
                          if (row.cells().size() != 1) {
                            std::cout << "Unexpected number of cells in "
                                      << row.row_key() << "\n";
                            return make_ready_future(false);
                          }
                          auto const& cell = row.cells().at(0);
                          std::cout << cell.row_key() << " = [" << cell.value()
                                    << "]\n";
                          return make_ready_future(true);
                        },
                        [&stream_status_promise](Status stream_status) {
                          stream_status_promise.set_value(stream_status);
                        },
                        range, filter);
    Status stream_status = stream_status_promise.get_future().get();
    if (!stream_status.ok()) {
      throw std::runtime_error(stream_status.message());
    }
  }
  //! [async read rows]
  (std::move(cq), std::move(table));
}

void AsyncReadRowsWithLimit(google::cloud::bigtable::Table table,
                            google::cloud::bigtable::CompletionQueue cq,
                            std::vector<std::string> argv) {
  if (argv.size() != 2U) {
    throw Usage{
        "async-read-rows-with-limit: <project-id> <instance-id> <table-id>"};
  }

  //! [async read rows with limit]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::make_ready_future;
  using google::cloud::promise;
  using google::cloud::Status;
  [](cbt::CompletionQueue cq, cbt::Table table) {
    // Create the range of rows to read.
    auto range = cbt::RowRange::Range("key-000010", "key-000020");
    // Filter the results, only include values from the "col0" column in the
    // "fam" column family, and only get the latest value.
    auto filter = cbt::Filter::Chain(
        cbt::Filter::ColumnRangeClosed("fam", "col0", "col0"),
        cbt::Filter::Latest(1));
    promise<Status> stream_status_promise;
    // Read and print the rows.
    table.AsyncReadRows(cq,
                        [](cbt::Row row) {
                          if (row.cells().size() != 1) {
                            std::cout << "Unexpected number of cells in "
                                      << row.row_key() << "\n";
                            return make_ready_future(false);
                          }
                          auto const& cell = row.cells().at(0);
                          std::cout << cell.row_key() << " = [" << cell.value()
                                    << "]\n";
                          return make_ready_future(true);
                        },
                        [&stream_status_promise](Status stream_status) {
                          stream_status_promise.set_value(stream_status);
                        },
                        range, filter);
    Status stream_status = stream_status_promise.get_future().get();
    if (!stream_status.ok()) {
      throw std::runtime_error(stream_status.message());
    }
  }
  //! [async read rows with limit]
  (std::move(cq), std::move(table));
}

void AsyncCheckAndMutate(google::cloud::bigtable::Table table,
                         google::cloud::bigtable::CompletionQueue cq,
                         std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{
        "async-check-and-mutate <project-id> <instance-id> <table-id>"
        " <row-key>"};
  }

  //! [async check and mutate]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::Table table, cbt::CompletionQueue cq, std::string table_id,
     std::string row_key) {
    // Check if the latest value of the flip-flop column is "on".
    cbt::Filter predicate = cbt::Filter::Chain(
        cbt::Filter::ColumnRangeClosed("fam", "flip-flop", "flip-flop"),
        cbt::Filter::Latest(1), cbt::Filter::ValueRegex("on"));
    future<StatusOr<cbt::MutationBranch>> branch_future =
        table.AsyncCheckAndMutateRow(row_key, std::move(predicate),
                                     {cbt::SetCell("fam", "flip-flop", "off"),
                                      cbt::SetCell("fam", "flop-flip", "on")},
                                     {cbt::SetCell("fam", "flip-flop", "on"),
                                      cbt::SetCell("fam", "flop-flip", "off")},
                                     cq);

    branch_future
        .then([](future<StatusOr<cbt::MutationBranch>> f) {
          auto response = f.get();
          if (!response) {
            throw std::runtime_error(response.status().message());
          }
          if (*response == cbt::MutationBranch::kPredicateMatched) {
            std::cout << "The predicate was matched\n";
          } else {
            std::cout << "The predicate was not matched\n";
          }
        })
        .get();  // block to simplify the example.
  }
  //! [async check and mutate]
  (std::move(table), std::move(cq), argv[1], argv[2]);
}

void AsyncReadModifyWrite(google::cloud::bigtable::Table table,
                          google::cloud::bigtable::CompletionQueue cq,
                          std::vector<std::string> argv) {
  if (argv.size() != 3U) {
    throw Usage{
        "async-read-modify-write <project-id> <instance-id> <table-id>"
        " <row-key>"};
  }

  //! [async read modify write]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::Table table, cbt::CompletionQueue cq, std::string table_id,
     std::string row_key) {
    future<StatusOr<cbt::Row>> row_future = table.AsyncReadModifyWriteRow(
        row_key, cq,
        cbt::ReadModifyWriteRule::AppendValue("fam", "list", ";element"));

    row_future
        .then([](future<StatusOr<cbt::Row>> f) {
          auto row = f.get();
          if (!row) {
            throw std::runtime_error(row.status().message());
          }
        })
        .get();  // block to simplify example.
  }
  //! [async read modify write]
  (std::move(table), std::move(cq), argv[1], argv[2]);
}
}  // anonymous namespace

int main(int argc, char* argv[]) try {
  using CommandType = std::function<void(
      google::cloud::bigtable::Table, google::cloud::bigtable::CompletionQueue,
      std::vector<std::string>)>;

  std::map<std::string, CommandType> commands = {
      {"async-apply", &AsyncApply},
      {"async-bulk-apply", &AsyncBulkApply},
      {"async-read-rows", &AsyncReadRows},
      {"async-read-rows-with-limit", &AsyncReadRowsWithLimit},
      {"async-check-and-mutate", &AsyncCheckAndMutate},
      {"async-read-modify-write", &AsyncReadModifyWrite}};

  google::cloud::bigtable::CompletionQueue cq;

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
        kv.second(unused, cq, {});
      } catch (Usage const& u) {
        command_usage += "    ";
        command_usage += u.msg;
        command_usage += "\n";
      }
    }
  }

  if (argc < 5) {
    PrintUsage(argv[0],
               "Missing command and/or project-id/ or instance-id or table-id");
    return 1;
  }

  std::vector<std::string> args;
  args.emplace_back(argv[0]);
  std::string const command_name = argv[1];
  std::string const project_id = argv[2];
  std::string const instance_id = argv[3];
  std::string const table_id = argv[4];
  std::transform(argv + 4, argv + argc, std::back_inserter(args),
                 [](char* x) { return std::string(x); });

  auto command = commands.find(command_name);
  if (commands.end() == command) {
    PrintUsage(argv[0], "Unknown command: " + command_name);
    return 1;
  }

  // Start a thread to run the completion queue event loop.
  std::thread runner([&cq] { cq.Run(); });

  // Connect to the Cloud Bigtable endpoint.
  //! [connect data]
  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          project_id, instance_id, google::cloud::bigtable::ClientOptions()),
      table_id);
  //! [connect data]

  command->second(table, cq, args);

  // Shutdown the completion queue event loop and join the thread.
  cq.Shutdown();
  runner.join();

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argv[0], ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}
//! [all code]

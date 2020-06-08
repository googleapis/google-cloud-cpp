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

#include "google/cloud/bigtable/examples/bigtable_examples_common.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/crash_handler.h"
#include <sstream>

namespace {

void AsyncApply(google::cloud::bigtable::Table table,
                google::cloud::bigtable::CompletionQueue cq,
                std::vector<std::string> const& argv) {
  //! [async-apply]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::Table table, cbt::CompletionQueue cq, std::string const& row_key) {
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());

    cbt::SingleRowMutation mutation(row_key);
    mutation.emplace_back(
        cbt::SetCell("fam", "column0", timestamp, "value for column0"));
    mutation.emplace_back(
        cbt::SetCell("fam", "column1", timestamp, "value for column1"));

    future<google::cloud::Status> status_future =
        table.AsyncApply(std::move(mutation), cq);
    auto status = status_future.get();
    if (!status.ok()) throw std::runtime_error(status.message());
    std::cout << "Successfully applied mutation\n";
  }
  //! [async-apply]
  (std::move(table), std::move(cq), argv.at(0));
}

void AsyncBulkApply(google::cloud::bigtable::Table table,
                    google::cloud::bigtable::CompletionQueue cq,
                    std::vector<std::string> const&) {
  //! [bulk async-bulk-apply]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  [](cbt::Table table, cbt::CompletionQueue cq) {
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
          // By default, the `table` object uses the
          // `SafeIdempotentMutationPolicy` which does not retry if any of the
          // mutations fails and are not idempotent. In this example we simply
          // print such failures, if any, and ignore them otherwise.
          std::cerr << "The following mutations failed and were not retried:\n";
          for (auto const& f : failures) {
            std::cerr << "index[" << f.original_index() << "]=" << f.status()
                      << "\n";
          }
        })
        .get();  // block to simplify the example
  }
  //! [bulk async-bulk-apply]
  (std::move(table), std::move(cq));
}

void AsyncReadRows(google::cloud::bigtable::Table table,
                   google::cloud::bigtable::CompletionQueue cq,
                   std::vector<std::string> const&) {
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
    table.AsyncReadRows(
        cq,
        [](cbt::Row const& row) {
          if (row.cells().size() != 1) {
            std::cout << "Unexpected number of cells in " << row.row_key()
                      << "\n";
            return make_ready_future(false);
          }
          auto const& cell = row.cells().at(0);
          std::cout << cell.row_key() << " = [" << cell.value() << "]\n";
          return make_ready_future(true);
        },
        [&stream_status_promise](Status const& stream_status) {
          stream_status_promise.set_value(stream_status);
        },
        range, filter);
    Status stream_status = stream_status_promise.get_future().get();
    if (!stream_status.ok()) throw std::runtime_error(stream_status.message());
  }
  //! [async read rows]
  (std::move(cq), std::move(table));
}

void AsyncReadRowsWithLimit(google::cloud::bigtable::Table table,
                            google::cloud::bigtable::CompletionQueue cq,
                            std::vector<std::string> const&) {
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
    table.AsyncReadRows(
        cq,
        [](cbt::Row const& row) {
          if (row.cells().size() != 1) {
            std::cout << "Unexpected number of cells in " << row.row_key()
                      << "\n";
            return make_ready_future(false);
          }
          auto const& cell = row.cells().at(0);
          std::cout << cell.row_key() << " = [" << cell.value() << "]\n";
          return make_ready_future(true);
        },
        [&stream_status_promise](Status const& stream_status) {
          stream_status_promise.set_value(stream_status);
        },
        range, filter);
    Status stream_status = stream_status_promise.get_future().get();
    if (!stream_status.ok()) throw std::runtime_error(stream_status.message());
  }
  //! [async read rows with limit]
  (std::move(cq), std::move(table));
}

void AsyncReadRow(google::cloud::bigtable::Table table,
                  google::cloud::bigtable::CompletionQueue cq,
                  std::vector<std::string> const& argv) {
  //! [async read row]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::CompletionQueue cq, google::cloud::bigtable::Table table,
     std::string const& row_key) {
    // Filter the results, only include the latest value on each cell.
    cbt::Filter filter = cbt::Filter::Latest(1);
    table.AsyncReadRow(cq, row_key, std::move(filter))
        .then(
            [row_key](future<StatusOr<std::pair<bool, cbt::Row>>> row_future) {
              // Read a row, this returns a tuple (bool, row)
              auto tuple = row_future.get();
              if (!tuple) throw std::runtime_error(tuple.status().message());
              if (!tuple->first) {
                std::cout << "Row " << row_key << " not found\n";
                return;
              }
              std::cout << "key: " << tuple->second.row_key() << "\n";
              for (auto& cell : tuple->second.cells()) {
                std::cout << "    " << cell.family_name() << ":"
                          << cell.column_qualifier() << " = <";
                if (cell.column_qualifier() == "counter") {
                  // This example uses "counter" to store 64-bit numbers in
                  // big-endian format, extract them as follows:
                  std::cout
                      << cell.decode_big_endian_integer<std::int64_t>().value();
                } else {
                  std::cout << cell.value();
                }
                std::cout << ">\n";
              }
            })
        .get();  // block to simplify the example
  }
  //! [async read row]
  (std::move(cq), std::move(table), argv.at(0));
}

void AsyncCheckAndMutate(google::cloud::bigtable::Table table,
                         google::cloud::bigtable::CompletionQueue cq,
                         std::vector<std::string> const& argv) {
  //! [async check and mutate]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::Table table, cbt::CompletionQueue cq, std::string const& row_key) {
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
          if (!response) throw std::runtime_error(response.status().message());
          if (*response == cbt::MutationBranch::kPredicateMatched) {
            std::cout << "The predicate was matched\n";
          } else {
            std::cout << "The predicate was not matched\n";
          }
        })
        .get();  // block to simplify the example.
  }
  //! [async check and mutate]
  (std::move(table), std::move(cq), argv.at(0));
}

void AsyncReadModifyWrite(google::cloud::bigtable::Table table,
                          google::cloud::bigtable::CompletionQueue cq,
                          std::vector<std::string> const& argv) {
  //! [async read modify write]
  namespace cbt = google::cloud::bigtable;
  using google::cloud::future;
  using google::cloud::StatusOr;
  [](cbt::Table table, cbt::CompletionQueue cq, std::string const& row_key) {
    future<StatusOr<cbt::Row>> row_future = table.AsyncReadModifyWriteRow(
        std::move(row_key), cq,
        cbt::ReadModifyWriteRule::AppendValue("fam", "list", ";element"));

    row_future
        .then([](future<StatusOr<cbt::Row>> f) {
          auto row = f.get();
          // As the modify in this example is not idempotent, and this example
          // does not attempt to retry if there is a failure, we simply print
          // such failures, if any, and otherwise ignore them.
          if (!row) {
            std::cout << "Failed to append row: " << row.status().message()
                      << "\n";
            return;
          }
          std::cout << "Successfully appended to " << row->row_key() << "\n";
        })
        .get();  // block to simplify example.
  }
  //! [async read modify write]
  (std::move(table), std::move(cq), argv.at(0));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::bigtable::examples;
  namespace cbt = google::cloud::bigtable;

  if (!argv.empty()) throw examples::Usage{"auto"};
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
  examples::CleanupOldTables("data-async-", admin);

  // Initialize a generator with some amount of entropy.
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const table_id = examples::RandomTableId("data-async-", generator);

  std::cout << "\nCreating table to run the examples (" << table_id << ")"
            << std::endl;
  auto schema = admin.CreateTable(
      table_id,
      cbt::TableConfig({{"fam", cbt::GcRule::MaxNumVersions(10)}}, {}));
  if (!schema) throw std::runtime_error(schema.status().message());

  google::cloud::bigtable::Table table(
      google::cloud::bigtable::CreateDefaultDataClient(
          admin.project(), admin.instance_id(),
          google::cloud::bigtable::ClientOptions()),
      table_id, cbt::AlwaysRetryMutationPolicy());

  google::cloud::CompletionQueue cq;
  std::thread th([&cq] { cq.Run(); });
  examples::AutoShutdownCQ shutdown(cq, std::move(th));

  std::cout << "\nRunning the AsyncApply() example" << std::endl;
  AsyncApply(table, cq, {"row-0001"});

  std::cout << "\nRunning the AsyncBulkApply() example" << std::endl;
  AsyncBulkApply(table, cq, {});

  std::cout << "\nRunning the AsyncReadRows() example" << std::endl;
  AsyncReadRows(table, cq, {});

  std::cout << "\nRunning the AsyncReadRows() example" << std::endl;
  AsyncReadRowsWithLimit(table, cq, {});

  std::cout << "\nRunning the AsyncReadRow() example [1]" << std::endl;
  AsyncReadRow(table, cq, {"row-0001"});

  std::cout << "\nRunning the AsyncReadRow() example [2]" << std::endl;
  AsyncReadRow(table, cq, {"row-not-found-key"});

  std::cout << "\nRunning the AsyncApply() example [2]" << std::endl;
  AsyncApply(table, cq, {"check-and-mutate-row-key"});

  std::cout << "\nRunning the AsyncCheckAndMutate() example" << std::endl;
  AsyncCheckAndMutate(table, cq, {"check-and-mutate-row-key"});

  std::cout << "\nRunning the AsyncApply() example [3]" << std::endl;
  AsyncApply(table, cq, {"read-modify-write-row-key"});

  std::cout << "\nRunning the AsyncReadModifyWrite() example" << std::endl;
  AsyncReadModifyWrite(table, cq, {"read-modify-write-row-key"});

  (void)admin.DeleteTable(table_id);
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InstallCrashHandler(argv[0]);

  using google::cloud::bigtable::examples::MakeCommandEntry;
  google::cloud::bigtable::examples::Example example({
      MakeCommandEntry("async-apply", {"<row-key>"}, AsyncApply),
      MakeCommandEntry("async-bulk-apply", {}, AsyncBulkApply),
      MakeCommandEntry("async-read-rows", {}, AsyncReadRows),
      MakeCommandEntry("async-read-rows-with-limit", {},
                       AsyncReadRowsWithLimit),
      MakeCommandEntry("async-read-row", {"<row-key>"}, AsyncReadRow),
      MakeCommandEntry("async-check-and-mutate", {"<row-key>"},
                       AsyncCheckAndMutate),
      MakeCommandEntry("async-read-modify-write", {}, AsyncReadModifyWrite),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

// Copyright 2021 Google LLC
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

#include "google/cloud/bigtable/benchmarks/mutation_batcher_throughput_options.h"
#include "google/cloud/bigtable/mutation_batcher.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/bigtable/testing/random_names.h"
#include "google/cloud/internal/background_threads_impl.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/command_line_parsing.h"
#include "absl/time/time.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

namespace {

namespace cbt = ::google::cloud::bigtable;
using cbt::benchmarks::MutationBatcherThroughputOptions;
using cbt::benchmarks::ParseMutationBatcherThroughputOptions;
using ::google::cloud::Status;
using ::google::cloud::StatusCode;
using ::google::cloud::StatusOr;
using ::google::cloud::internal::GetEnv;

char const kDescription[] =
    R"""(A benchmark to measure the throughput of the `MutationBatcher` class.

The purpose of the program is to determine default settings for
`MutationBatcher::Options` that maximize throughput. The specific settings,
which are the main inputs to this program, are maximum mutations per batch and
maximum concurrent batches in flight. It also tests the performance that can be
achieved by providing initial splits to the table and having multiple batchers
send it mutations in parallel.

The program is designed to be run repeatedly. It can be configured to terminate
after a set amount of time. It can also be configured to use a pre-existing
table instead of creating a new one then deleting it when the program is done.

The mutations are all of the same size. There is exactly 1 mutation per row.
The rows fall in the range from "row00000" to "rowNNNNN".

The program will:

1) Conditionally create a table, with initial splits.
2) Echo your configuration settings.
3) Spin off some number of threads. Each thread will:
  a) Configure a `MutationBatcher`.
  b) Send mutations to a `MutationBatcher`. The `MutationBatcher` will apply
     these mutations to the table.
  c) One thread will log progress for those who are impatient.
  d) Conditionally stop batching mutations if the program exceeds a supplied
     deadline.
  e) Block until all mutations have been processed.
  f) Record the number of successful and failed mutations.
4) Join all threads.
5) Report the total time it took to apply the mutations.
6) Report the total number of successful and failed mutations, across all
   threads.
7) Conditionally delete the table.

If, for example, the program is configured to send 2000 mutations to a
table with 4 shards using 2 write threads, the row range will be from "row0000"
to "row1999". The initial splits provided will be at "row0000", "row0500",
"row1000", "row1500". Two threads are created, one does the work for
"row0000"-"row0999", the other does the work for "row1000"-"row1999".
)""";

StatusOr<MutationBatcherThroughputOptions> ParseArgs(int argc, char* argv[]) {
  auto const auto_run =
      GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES").value_or("") == "yes";
  if (auto_run) {
    for (auto const& var : {"GOOGLE_CLOUD_PROJECT",
                            "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID"}) {
      auto const value = GetEnv(var).value_or("");
      if (!value.empty()) continue;
      std::ostringstream os;
      os << "The environment variable " << var << "is not set or empty";
      return Status(StatusCode::kUnknown, std::move(os).str());
    }
    auto const project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value();
    auto const instance_id =
        GetEnv("GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID").value();
    return ParseMutationBatcherThroughputOptions(
        {
            std::string(argv[0]),
            "--project-id=" + project_id,
            "--instance-id=" + instance_id,
            "--mutation-count=1000",
            "--max-batches=3",
        },
        kDescription);
  }

  return ParseMutationBatcherThroughputOptions({argv, argv + argc},
                                               kDescription);
}

std::string MakeRowString(int key_width, int64_t row_index) {
  std::ostringstream os;
  os << "row" << std::setw(key_width) << std::setfill('0') << row_index;
  return std::move(os).str();
}

cbt::SingleRowMutation MakeMutation(
    MutationBatcherThroughputOptions const& options, std::string row_key) {
  return cbt::SingleRowMutation(
      std::move(row_key),
      {cbt::SetCell(options.column_family, options.column, "value")});
}

}  // namespace

int main(int argc, char* argv[]) {
  auto options = ParseArgs(argc, argv);
  if (!options) {
    std::cerr << options.status() << "\n";
    return 1;
  }
  if (options->exit_after_parse) return 0;

  namespace cbt = ::google::cloud::bigtable;
  using ::google::cloud::CompletionQueue;
  using ::google::cloud::future;
  using ::google::cloud::Options;
  using ::google::cloud::Status;
  using ::google::cloud::StatusOr;
  using ::google::cloud::bigtable::testing::RandomTableId;
  using ::google::cloud::internal::AutomaticallyCreatedBackgroundThreads;
  using TimerFuture = future<StatusOr<std::chrono::system_clock::time_point>>;

  cbt::TableAdmin admin(cbt::CreateDefaultAdminClient(options->project_id, {}),
                        options->instance_id);

  int key_width = 0;
  for (auto i = options->mutation_count - 1; i != 0; i /= 10) ++key_width;

  // Create a new table if one was not supplied
  auto table_id = options->table_id;
  if (table_id.empty()) {
    auto generator = google::cloud::internal::MakeDefaultPRNG();
    table_id = RandomTableId(generator);

    // Provide initial splits to the table
    std::vector<std::string> splits;
    for (auto i = 0; i != options->shard_count; ++i) {
      auto row_index = options->mutation_count * i / options->shard_count;
      splits.emplace_back(MakeRowString(key_width, row_index));
    }

    std::cout << "# Creating Table\n";
    auto status = admin.CreateTable(
        table_id, cbt::TableConfig({{options->column_family,
                                     cbt::GcRule::MaxNumVersions(10)}},
                                   splits));
    if (!status) {
      std::cout << status.status() << std::endl;
      return 1;
    }
    std::cout << "#\n";
  } else {
    auto table = admin.GetTable(table_id, cbt::TableAdmin::NAME_ONLY);
    if (!table) {
      std::cout << "Error trying to get Table " << table_id << ":\n"
                << table.status() << std::endl;
      return 1;
    }
  }

  auto opts = Options{}.set<google::cloud::GrpcBackgroundThreadPoolSizeOption>(
      options->max_batches);
  auto table = cbt::Table(
      cbt::CreateDefaultDataClient(options->project_id, options->instance_id,
                                   cbt::ClientOptions(std::move(opts))),
      table_id);

  std::cout << "# Project ID: " << options->project_id
            << "\n# Instance ID: " << options->instance_id
            << "\n# Table ID: " << table_id << "\n# Cutoff Time: "
            << absl::FormatDuration(absl::FromChrono(options->max_time))
            << "\n# Shard Count: " << options->shard_count
            << "\n# Write Thread Count: " << options->write_thread_count
            << "\n# Batcher Thread Count: " << options->batcher_thread_count
            << "\n# Total Mutations: " << options->mutation_count
            << "\n# Mutations per Batch: " << options->batch_size
            << "\n# Concurrent Batches: " << options->max_batches << std::endl;

  // Create the batcher threads
  AutomaticallyCreatedBackgroundThreads batcher_threads(
      options->batcher_thread_count);
  CompletionQueue cq = batcher_threads.cq();

  // Create a deadline timer
  // If there is no deadline set, the timer fires instantly and does nothing
  std::atomic<bool> timeout{false};
  auto timer = cq.MakeRelativeTimer(options->max_time)
                   .then([&timeout, &options](TimerFuture) {
                     timeout = options->max_time.count() > 0;
                   });

  struct BenchmarkResult {
    std::int64_t fails = 0;
    std::int64_t successes = 0;
  } totals;

  auto write = [&options, &table, &cq, &timeout, key_width](int write_index) {
    auto start =
        options->mutation_count * write_index / options->write_thread_count;
    auto end = options->mutation_count * (write_index + 1) /
               options->write_thread_count;

    // Only one write thread will log its progress
    bool log = write_index == 0;
    if (log) std::cout << "#\n# Writing" << std::flush;
    auto progress_period = std::max<std::int64_t>(1, (end - start) / 20);

    BenchmarkResult result;
    result.successes = end - start;

    cbt::MutationBatcher batcher(
        table, cbt::MutationBatcher::Options{}
                   .SetMaxBatches(options->max_batches)
                   .SetMaxMutationsPerBatch(options->batch_size));

    for (auto i = start; i != end; ++i) {
      // Stop writing if we hit the cutoff deadline
      if (timeout) {
        result.successes = i - start;
        break;
      }

      auto mut = MakeMutation(*options, MakeRowString(key_width, i));
      auto admission_completion = batcher.AsyncApply(cq, std::move(mut));
      auto& admission_future = admission_completion.first;
      auto& completion_future = admission_completion.second;
      completion_future.then([&result](future<Status> fut) {
        auto status = fut.get();
        if (!status.ok()) {
          ++result.fails;
        }
      });
      admission_future.get();

      if (log && (i - start) % progress_period == 0) {
        std::cout << "." << std::flush;
      }
    }
    if (log) std::cout << "\n#" << std::endl;

    batcher.AsyncWaitForNoPendingRequests().get();

    result.successes -= result.fails;
    return result;
  };

  auto start_time = std::chrono::steady_clock::now();

  auto write_index = 0;
  std::vector<std::future<BenchmarkResult>> tasks(options->write_thread_count);
  std::generate(tasks.begin(), tasks.end(), [write, &write_index] {
    return std::async(std::launch::async, write, write_index++);
  });

  for (auto& t : tasks) {
    auto thread_result = t.get();
    totals.fails += thread_result.fails;
    totals.successes += thread_result.successes;
  }

  auto end_time = std::chrono::steady_clock::now();
  std::chrono::duration<double> elapsed = end_time - start_time;

  // Shutdown the deadline timer
  timer.cancel();
  timer.get();

  std::cout << "MutationCount,BatchSize,MaxBatches,ShardCount,WriteThreadCount,"
               "BatcherThreadCount,ElapsedSeconds,Successes,Fails\n"
            << options->mutation_count << "," << options->batch_size << ","
            << options->max_batches << "," << options->shard_count << ","
            << options->write_thread_count << ","
            << options->batcher_thread_count << "," << elapsed.count() << ","
            << totals.successes << "," << totals.fails << "\n";

  // If we created a table, delete it.
  if (options->table_id.empty()) {
    std::cout << "#\n# Deleting Table\n";
    auto status = admin.DeleteTable(table_id);
    if (!status.ok()) {
      std::cout << status << std::endl;
      return -1;
    }
  }

  return 0;
}

// Copyright 2021 Google Inc.
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
#include "google/cloud/internal/getenv.h"
#include "google/cloud/status_or.h"
#include "google/cloud/testing_util/command_line_parsing.h"
#include "google/cloud/testing_util/timer.h"
#include "absl/time/time.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>

namespace {

namespace cbt = google::cloud::bigtable;
using cbt::benchmarks::MutationBatcherThroughputOptions;
using cbt::benchmarks::ParseMutationBatcherThroughputOptions;
using google::cloud::Status;
using google::cloud::StatusCode;
using google::cloud::internal::GetEnv;

char const kDescription[] =
    R"""(A benchmark to measure the throughput of the `MutationBatcher` class.

The purpose of the program is to determine effective default settings for
`MutationBatcher::Options` that maximize throughput. The specific settings,
which are the main inputs to this program, are maximum mutations per batch and
maximum concurrent batches in flight.

The program is designed to be run repeatedly. It has the ability to cut itself
off (if really bad options are supplied). It also has the ability to use a
pre-existing table instead of creating a new one then deleting it when the
program is done.

The program will:

1) Conditionally create a table.
2) Echo your configuration settings.
3) Configure a `MutationBatcher`.
4) Spin off some number of threads.
5) Send mutations to a `MutationBatcher` from each thread. The `MutationBatcher`
   will apply these mutations to the table. Progress will be logged for those
   who are impatient.
6) Conditionally stop batching mutations if the program exceeds a supplied deadline.
7) Report the total time it took to apply the mutations.
8) Report the total number of successful and failed mutations.
9) Conditionally delete the table.
)""";

google::cloud::StatusOr<MutationBatcherThroughputOptions> ParseArgs(
    int argc, char* argv[]) {
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

cbt::SingleRowMutation MakeMutation(
    MutationBatcherThroughputOptions const& options, int key_width,
    std::int64_t const& row_key) {
  std::ostringstream os;
  os << "row" << std::setw(key_width) << std::setfill('0') << row_key;
  return cbt::SingleRowMutation(
      std::move(os).str(),
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

  namespace cbt = google::cloud::bigtable;
  using google::cloud::CompletionQueue;
  using google::cloud::future;
  using google::cloud::Options;
  using google::cloud::Status;
  using google::cloud::testing_util::Timer;

  cbt::TableAdmin admin(cbt::CreateDefaultAdminClient(options->project_id, {}),
                        options->instance_id);

  // Create a new table if one was not supplied
  auto table_id = options->table_id;
  if (table_id.empty()) {
    using google::cloud::internal::Sample;
    auto generator = google::cloud::internal::MakeDefaultPRNG();
    std::string table_id_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";
    table_id = "table-" + Sample(generator, 8, table_id_chars);

    std::cout << "# Creating Table\n";
    auto status = admin.CreateTable(
        table_id,
        cbt::TableConfig(
            {{options->column_family, cbt::GcRule::MaxNumVersions(10)}}, {}));
    if (!status) {
      std::cout << status.status() << std::endl;
      return -1;
    }
    std::cout << "#\n";
  } else {
    auto status = admin.GetTable(table_id, cbt::TableAdmin::NAME_ONLY);
    if (!status) {
      std::cout << status.status() << std::endl;
      return -1;
    }
  }

  auto opts = Options{}.set<google::cloud::GrpcBackgroundThreadPoolSizeOption>(
      options->max_batches);
  auto table = cbt::Table(
      cbt::CreateDefaultDataClient(options->project_id, options->instance_id,
                                   cbt::ClientOptions(std::move(opts))),
      table_id);

  cbt::MutationBatcher batcher(
      table, cbt::MutationBatcher::Options{}
                 .SetMaxBatches(options->max_batches)
                 .SetMaxMutationsPerBatch(options->batch_size));

  std::cout << "# Project ID: " << options->project_id
            << "\n# Instance ID: " << options->instance_id
            << "\n# Table ID: " << table_id << "\n# Cutoff Time: "
            << absl::FormatDuration(absl::FromChrono(options->max_time))
            << "\n# Thread Count: " << options->thread_count
            << "\n# Total Mutations: " << options->mutation_count
            << "\n# Mutations per Batch: " << options->batch_size
            << "\n# Concurrent Batches: " << options->max_batches << std::endl;

  std::atomic<int> fails{0};
  std::atomic<int> successes{0};
  std::atomic<bool> timeout{false};

  // Conditionally start the timeout thread
  std::thread* timeout_thread = nullptr;
  if (options->max_time.count() > 0) {
    auto deadline = std::chrono::system_clock::now() + options->max_time;
    timeout_thread = new std::thread([deadline, &timeout] {
      while (std::chrono::system_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
      timeout = true;
    });
  }

  // Start the MutationBatcher thread
  CompletionQueue cq;
  std::thread cq_runner([&cq] { cq.Run(); });

  int key_width = 0;
  for (std::int64_t i = options->mutation_count - 1; i > 0; i /= 10) ++key_width;
  auto write = [&options, &batcher, &cq, &fails, &successes, &timeout,
                key_width](std::int64_t const& start, std::int64_t const& end,
                           bool const& log) {
    // One write thread will log its progress
    if (log) std::cout << "#\n# Writing" << std::flush;
    auto progress_period = (std::max)(1L, (end - start) / 20);

    for (std::int64_t i = start; i < end; ++i) {
      // Stop writing if we hit the cutoff deadline
      if (timeout) break;

      auto mut = MakeMutation(*options, key_width, i);
      auto admission_completion = batcher.AsyncApply(cq, mut);
      auto& admission_future = admission_completion.first;
      auto& completion_future = admission_completion.second;
      completion_future.then([&fails, &successes](future<Status> fut) {
        auto status = fut.get();
        if (!status.ok())
          ++fails;
        else
          ++successes;
      });
      admission_future.get();

      if (log && (i - start) % progress_period == 0)
        std::cout << "." << std::flush;
    }
    if (log) std::cout << "\n#" << std::endl;
  };

  auto timer = Timer::PerThread();

  std::vector<std::future<void>> tasks;
  std::int64_t start = 0;
  for (int j = 0; j < options->thread_count; ++j) {
    auto end =
        (std::min)(options->mutation_count,
                   start + options->mutation_count / options->thread_count);
    tasks.emplace_back(
        std::async(std::launch::async, write, start, end, j == 0));
    start = end;
  }

  for (auto& t : tasks) t.get();

  // Wait for all mutations to complete
  batcher.AsyncWaitForNoPendingRequests().get();

  // Join the MutationBatcher thread
  cq.Shutdown();
  cq_runner.join();

  auto snapshot = timer.Sample();
  std::cout << "MutationCount,BatchSize,MaxBatches,ElapsedMicroseconds,"
               "Successes,Fails\n"
            << options->mutation_count << "," << options->batch_size << ","
            << options->max_batches << "," << snapshot.elapsed_time.count()
            << "," << successes << "," << fails << "\n";

  // If we created a table, delete it.
  if (options->table_id.empty()) {
    std::cout << "#\n# Deleting Table\n";
    auto status = admin.DeleteTable(table_id);
    if (!status.ok()) {
      std::cout << status << std::endl;
      return -1;
    }
  }

  // Detach the timeout thread if it is running
  if (timeout_thread) {
    timeout_thread->detach();
    delete timeout_thread;
  }

  return 0;
}

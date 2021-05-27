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

#include "google/cloud/bigtable/mutation_batcher.h"
#include "google/cloud/bigtable/table.h"
#include "google/cloud/bigtable/table_admin.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/random.h"
#include "absl/strings/str_split.h"
#include <condition_variable>
#include <fstream>
#include <future>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <vector>

/**
 * @file
 *
 * Shows how to upload CSV data from Google Cloud Storage to Google Cloud
 * Bigtable.
 */
namespace cbt = google::cloud::bigtable;
namespace gcs = google::cloud::storage;

namespace {

struct Options {
  char separator;
  std::vector<int> keys;
  std::string keys_separator;
  std::string project_id;
  std::string instance_id;
  std::string table_id;
  std::string family;
  std::string bucket;
  std::string object;
};

Options ParseArgs(int argc, char* argv[]);

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  auto const options = ParseArgs(argc, argv);

  // Create a connection to Cloud Bigtable and an object to manipulate the
  // specific table used in this demo.
  cbt::Table table(cbt::CreateDefaultDataClient(
                       options.project_id, options.instance_id,
                       cbt::ClientOptions().set_connection_pool_size(
                           std::thread::hardware_concurrency())),
                   options.table_id);
  cbt::MutationBatcher batcher(table);

  // How often do we print a progress marker ('.') in the reader thread.
  int const report_reader_progress_rate = 500000;
  // How often do we print a progress marker ('+') in the worker threads.
  int const report_worker_progress_rate = 500000;
  // The size of the thread pool pushing data to Cloud Bigtable
  std::size_t const thread_pool_size = []() -> std::size_t {
    if (std::thread::hardware_concurrency() != 0) {
      return std::thread::hardware_concurrency() - 1;
    }
    return 1;
  }();

  std::cout << "Starting " << thread_pool_size << " workers ..." << std::flush;
  google::cloud::bigtable::CompletionQueue cq;
  std::vector<std::thread> thread_pool;
  for (std::size_t i = 0; i != thread_pool_size; ++i) {
    thread_pool.emplace_back([&cq] { cq.Run(); });
  }
  std::cout << " DONE\n";

  gcs::Client client(
      google::cloud::Options{}.set<gcs::ProjectIdOption>(options.project_id));
  // The main thread just reads the file one line at a time.
  auto is = client.ReadObject(options.bucket, options.object);
  std::string line;
  std::getline(is, line, '\n');
  int lineno = 0;
  std::vector<std::string> const headers =
      absl::StrSplit(line, options.separator);

  std::cout << "# HEADER " << line << "\n";

  std::cout << "Reading input file " << std::flush;
  auto start = std::chrono::steady_clock::now();

  std::atomic<int> apply_finished_count(0);
  auto report_progress_callback =
      [=, &apply_finished_count](
          ::google::cloud::future<::google::cloud::Status> status_future) {
        if ((apply_finished_count.fetch_add(1) + 1) %
                report_worker_progress_rate ==
            0) {
          std::cout << '+' << std::flush;
        }
        auto status = status_future.get();
        if (!status.ok()) {
          std::cerr << "Apply failed: " << status;
        }
      };
  while (std::getline(is, line, '\n')) {
    ++lineno;

    if (line.empty()) {
      break;
    }
    std::vector<std::string> const parsed =
        absl::StrSplit(line, options.separator);

    using std::chrono::milliseconds;
    auto ts = std::chrono::duration_cast<milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());

    // Create the row key by concatenating the desired columns.
    std::string row_key;
    std::string row_key_separator;
    for (auto&& index : options.keys) {
      row_key += row_key_separator;
      row_key_separator = options.keys_separator;
      if (index == -1) {
        // Magic index, use the line number
        row_key += std::to_string(lineno);
      } else {
        row_key += parsed.at(index);
      }
    }

    // Create a mutation that inserts one column per field, the name of the
    // column is derived from the header.
    cbt::SingleRowMutation mutation(row_key);
    auto field_count = std::min(headers.size(), parsed.size());
    for (std::size_t i = 0; i != field_count; ++i) {
      mutation.emplace_back(
          cbt::SetCell(options.family, headers[i], ts, parsed[i]));
    }
    auto admission_and_completion = batcher.AsyncApply(cq, std::move(mutation));
    admission_and_completion.second.then(report_progress_callback);

    if (lineno % report_reader_progress_rate == 0) {
      std::cout << '.' << std::flush;
    }

    // Wait until there is space in buffers.
    admission_and_completion.first.get();
  }
  batcher.AsyncWaitForNoPendingRequests().get();
  std::cout << " DONE\n";

  std::cout << "Waiting for worker threads " << std::flush;
  // Let the workers know that they can exit.
  cq.Shutdown();
  for (auto& thread : thread_pool) {
    thread.join();
  }
  std::cout << " DONE\n";

  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now() - start);
  std::cout << "Total running time " << elapsed.count() << "s\n";

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << "\n";
  return 1;
}

namespace {

std::string ConsumeArg(Options& options, std::vector<std::string>& argv,
                       char const* arg_name) {
  std::string const separator_option = "--separator=";
  std::string const key_option = "--key=";
  std::string const keys_separator_option = "--key-separator=";

  std::string const usage = R""(
[options] <project> <instance> <table> <family> <bucket> <object>
The options are:
    --help: produce this help.
    --separator=c: use the 'c' character instead of comma (',') to separate the
        values in the CSV file.
    --key=N: use field number N as part of the row key. The fields are numbered
        starting at one. They are concatenated in the order provided.
    --key-separator=sep: use 'sep' to separate the fields when forming the row
        key.
    project: the Google Cloud Platform project id for your table.
    instance: the Cloud Bigtable instance hosting your table.
    table: the table where you want to upload the CSV file.
    family: the column family where you want to upload the CSV file.
    bucket: the name of the GCS bucket that contains the data.
    object: the name of the GCS object that contains the data.
)"";
  while (argv.size() >= 2) {
    std::string argument(argv[1]);
    argv.erase(argv.begin() + 1);
    if (argument == "--help") {
    } else if (0 == argument.find(separator_option)) {
      options.separator = argument.substr(separator_option.size())[0];
    } else if (0 == argument.find(key_option)) {
      options.keys.push_back(std::stoi(argument.substr(key_option.size())) - 1);
    } else if (0 == argument.find(keys_separator_option)) {
      options.keys_separator = argument.substr(keys_separator_option.size());
    } else {
      return argument;
    }
  }
  std::string cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  cmd = cmd.substr(last_slash);

  std::ostringstream os;
  os << "Missing argument " << arg_name << "\n";
  os << "Usage: " << cmd << usage << "\n";
  throw std::runtime_error(os.str());
}

Options ParseArgsNoAutoRun(int argc, char const* const argv[]) {
  // Parse the command-line arguments.
  Options options;
  std::vector<std::string> args(argv, argv + argc);
  options.project_id = ConsumeArg(options, args, "project_id");
  options.instance_id = ConsumeArg(options, args, "instance_id");
  options.table_id = ConsumeArg(options, args, "table_id");
  options.family = ConsumeArg(options, args, "family");
  options.bucket = ConsumeArg(options, args, "bucket");
  options.object = ConsumeArg(options, args, "object");
  // If the user does not say, use the first column as the row key.
  if (options.keys.empty()) {
    options.keys.push_back(0);
  }
  return options;
}

/// Setup test versions of the Bigtable and Google Cloud Storage environments
/// and return options pointing to those versions.
Options AutoRun() {
  using google::cloud::internal::GetEnv;
  using google::cloud::internal::Sample;

  for (auto const& var :
       {"GOOGLE_CLOUD_PROJECT", "GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID",
        "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME"}) {
    auto const value = GetEnv(var).value_or("");
    if (!value.empty()) continue;
    std::ostringstream os;
    os << "The environment variable " << var << " is not set or empty";
    throw std::runtime_error(std::move(os).str());
  }
  auto const project_id = GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const instance_id =
      GetEnv("GOOGLE_CLOUD_CPP_BIGTABLE_TEST_INSTANCE_ID").value();
  auto const bucket_name =
      GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME").value();
  auto constexpr kTableId = "gcs2cbt-auto-run";
  auto constexpr kObjectName = "gcs2cbt-sample-data.csv";
  auto gcs_client = gcs::Client();
  auto constexpr kTestData = R"""(RowId,Header1,Header2,Header3
1,v1,v2,v3
3,v1,v2,v3
)""";
  gcs_client.InsertObject(bucket_name, kObjectName, kTestData).value();

  cbt::TableAdmin admin(
      cbt::CreateDefaultAdminClient(project_id, cbt::ClientOptions{}),
      instance_id);
  auto schema = admin.CreateTable(
      kTableId,
      cbt::TableConfig({{"fam", cbt::GcRule::MaxNumVersions(2)}}, {}));
  // Throw the error unless it is "already exists"
  if (!schema &&
      schema.status().code() != google::cloud::StatusCode::kAlreadyExists) {
    (void)schema.value();
  }

  char const* argv[] = {
      "auto-run",          "--key=1", "--separator=,", project_id.c_str(),
      instance_id.c_str(), kTableId,  "fam",           bucket_name.c_str(),
      kObjectName};
  int argc = sizeof(argv) / sizeof(argv[0]);
  return ParseArgsNoAutoRun(argc, argv);
}

Options ParseArgs(int argc, char* argv[]) {
  bool auto_run =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_AUTO_RUN_EXAMPLES")
          .value_or("") == "yes";
  if (auto_run) return AutoRun();

  return ParseArgsNoAutoRun(argc, argv);
}

}  // namespace

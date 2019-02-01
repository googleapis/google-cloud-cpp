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

#include "google/cloud/bigtable/table.h"
#include "google/cloud/storage/client.h"
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
template <typename T>
class GenericCircularBuffer {
 public:
  explicit GenericCircularBuffer(std::size_t size)
      : buffer_(size), head_(0), tail_(0), empty_(true), is_shutdown_(false) {}

  void Shutdown() {
    std::unique_lock<std::mutex> lk(mu_);
    is_shutdown_ = true;
    lk.unlock();
    cv_.notify_all();
  }

  bool Pop(T& next) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this]() { return !Empty() || is_shutdown_; });
    if (!Empty()) {
      next = std::move(buffer_[head_]);
      ++head_;
      if (head_ >= buffer_.size()) {
        head_ = 0;
      }
      empty_ = head_ == tail_;
      lk.unlock();
      cv_.notify_all();
      return true;
    }
    return !is_shutdown_;
  }

  void Push(T data) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this]() { return !Full(); });
    buffer_[tail_] = std::move(data);
    ++tail_;
    if (tail_ >= buffer_.size()) {
      tail_ = 0;
    }
    empty_ = false;
    lk.unlock();
    cv_.notify_all();
  }

 private:
  bool Empty() { return head_ == tail_ && empty_; }
  bool Full() { return head_ == tail_ && !empty_; }

 private:
  std::mutex mu_;
  std::condition_variable cv_;
  std::vector<T> buffer_;
  std::size_t head_;
  std::size_t tail_;
  bool empty_;
  bool is_shutdown_;
};

/**
 * Breakdown a CSV file line into fields.
 *
 * TODO() - handle escape sequences with backslash.
 *
 * TODO() - handle quoted fields with embedded separators.
 *
 * TODO() - make the separator configurable.
 */
std::vector<std::string> ParseLine(long lineno, std::string const& line,
                                   char separator);

using CircularBuffer = GenericCircularBuffer<cbt::BulkMutation>;

struct Options {
  char separator;
  std::vector<int> keys;
  std::string keys_separator;

  std::string ConsumeArg(int& argc, char* argv[], char const* arg_name) {
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
    while (argc >= 2) {
      std::string argument(argv[1]);
      std::copy(argv + 2, argv + argc, argv + 1);
      argc--;
      if (argument == "--help") {
      } else if (0 == argument.find(separator_option)) {
        separator = argument.substr(separator_option.size())[0];
      } else if (0 == argument.find(key_option)) {
        keys.push_back(std::stoi(argument.substr(key_option.size())) - 1);
      } else if (0 == argument.find(keys_separator_option)) {
        keys_separator = argument.substr(keys_separator_option.size());
      } else {
        return argument;
      }
    }
    std::string cmd = argv[0];
    auto last_slash = std::string(cmd).find_last_of('/');
    cmd = cmd.substr(last_slash);

    std::ostringstream os;
    os << "Missing argument " << arg_name << "\n";
    os << "Usage: " << cmd << usage << std::endl;
    throw std::runtime_error(os.str());
  }
};

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Parse the command-line arguments.
  Options options;
  std::string const project_id = options.ConsumeArg(argc, argv, "project_id");
  std::string const instance_id = options.ConsumeArg(argc, argv, "instance_id");
  std::string const table_id = options.ConsumeArg(argc, argv, "table_id");
  std::string const family = options.ConsumeArg(argc, argv, "family");
  std::string const bucket = options.ConsumeArg(argc, argv, "bucket");
  std::string const object = options.ConsumeArg(argc, argv, "object");

  // If the user does not say, use the first column as the row key.
  if (options.keys.empty()) {
    options.keys.push_back(0);
  }

  // Create a connection to Cloud Bigtable and an object to manipulate the
  // specific table used in this demo.
  cbt::Table table(cbt::CreateDefaultDataClient(
                       project_id, instance_id,
                       cbt::ClientOptions().set_connection_pool_size(
                           std::thread::hardware_concurrency())),
                   table_id);

  // How often do we print a progress marker ('.') in the reader thread.
  int const report_reader_progress_rate = 500000;
  // How often do we print a progress marker ('+') in the worker threads.
  int const report_worker_progress_rate = 5;
  // The size of the thread pool pushing data to Cloud Bigtable
  std::size_t const thread_pool_size = []() -> std::size_t {
    if (std::thread::hardware_concurrency() != 0U) {
      return std::thread::hardware_concurrency() - 1;
    }
    return 1;
  }();

  // The size of the circular buffer. It is tempting to make it larger, but each
  // element in the circular buffer can be a few MiB in size. We limit this to
  // the number of threads, once it becomes that large all threads will have
  // work to do after they finish their current work.
  int const buffer_size = static_cast<int>(thread_pool_size);

  // Create a circular buffer to communicate between the main thread that reads
  // the file and the threads that upload the parsed lines to Cloud Bigtable.
  CircularBuffer buffer(buffer_size);
  // Then create a few threads, each one of which pulls mutations out of the
  // circular buffer and then applies the mutation to the table.
  auto read_buffer = [&buffer](cbt::Table table, int report_progress_rate) {
    long count = 0;
    cbt::BulkMutation mutation;
    while (buffer.Pop(mutation)) {
      table.BulkApply(std::move(mutation));
      if (++count % report_progress_rate == 0) {
        std::cout << '+' << std::flush;
      }
    }
  };

  std::cout << "Starting " << thread_pool_size << " workers ..." << std::flush;
  std::vector<std::future<void>> workers;
  for (std::size_t i = 0; i != thread_pool_size; ++i) {
    workers.push_back(std::async(std::launch::async, read_buffer, table,
                                 report_worker_progress_rate));
  }
  std::cout << " DONE" << std::endl;

  google::cloud::StatusOr<gcs::ClientOptions> opts =
      gcs::ClientOptions::CreateDefaultClientOptions();
  if (!opts) {
    std::cerr << "Couldn't create gcs::ClientOptions, status=" << opts.status();
    return 1;
  }
  gcs::Client client(opts->set_project_id(project_id));
  // The main thread just reads the file one line at a time.
  auto is = client.ReadObject(bucket, object);
  std::string line;
  std::getline(is, line, '\n');
  int lineno = 0;
  auto headers = ParseLine(++lineno, line, options.separator);

  std::cout << "# HEADER " << line << std::endl;

  std::cout << "Reading input file " << std::flush;
  auto start = std::chrono::steady_clock::now();

  // We can only do about 100,000 Apply operations in each BulkApply (see the
  // Cloud Bigtable bigtable.proto file for details), and each SingleRowMutation
  // we create below has roughly headers.size() elements. So we want at most:
  auto bulk_apply_size = static_cast<int>(100000 / headers.size());
  cbt::BulkMutation bulk;
  int count = 0;
  while (!is.eof()) {
    ++lineno;
    std::getline(is, line, '\n');
    if (line.empty()) {
      break;
    }
    auto parsed = ParseLine(lineno, line, options.separator);

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
      mutation.emplace_back(cbt::SetCell(family, headers[i], ts, parsed[i]));
    }
    bulk.emplace_back(std::move(mutation));
    // If we have too many mutations in the bulk mutation it is time to push it
    // to the queue, where one of the running threads will pick it up.
    if (++count >= bulk_apply_size) {
      buffer.Push(std::move(bulk));
      bulk = {};
      count = 0;
    }

    if (lineno % report_reader_progress_rate == 0) {
      std::cout << '.' << std::flush;
    }
  }
  if (count != 0) {
    buffer.Push(std::move(bulk));
  }
  std::cout << " DONE" << std::endl;

  std::cout << "Waiting for worker threads " << std::flush;
  // Let the workers know that they can exit.
  buffer.Shutdown();
  int worker_count = 0;
  for (auto& task : workers) {
    // If there was an exception in any thread continue, and report any
    // exceptions raised by other threads too.
    try {
      task.get();
    } catch (std::exception const& ex) {
      std::cerr << "Exception raised by worker " << worker_count << ": "
                << ex.what() << std::endl;
    }
    ++worker_count;
    std::cout << '.' << std::flush;
  }
  std::cout << " DONE" << std::endl;

  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now() - start);
  std::cout << "Total running time " << elapsed.count() << "s" << std::endl;

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception raised: " << ex.what() << std::endl;
  return 1;
}

namespace {
std::vector<std::string> ParseLine(long lineno, std::string const& line,
                                   char separator) try {
  std::vector<std::string> result;

  // Extract the fields one at a time using a std::istringstream.
  std::istringstream tokens(line);
  while (!tokens.eof()) {
    std::string tk;
    std::getline(tokens, tk, separator);
    result.emplace_back(std::move(tk));
  }
  return result;
} catch (std::exception const& ex) {
  std::ostringstream os;
  os << ex.what() << " in line #" << lineno << " (" << line << ")";
  throw std::runtime_error(os.str());
}

}  // namespace

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
 * Shows how to upload CSV data into Cloud Bigtable.
 */

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
    cv_.wait(lk, [this]() { return not Empty() or is_shutdown_; });
    if (not Empty()) {
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
    return not is_shutdown_;
  }

  void Push(T data) {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [this]() { return not Full(); });
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
  bool Empty() { return head_ == tail_ and empty_; }
  bool Full() { return head_ == tail_ and not empty_; }

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
                                   char separator = ',');

namespace cbt = google::cloud::bigtable;
using CircularBuffer = GenericCircularBuffer<cbt::BulkMutation>;

}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Make sure we have the right number of arguments.
  if (argc != 6) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <project> <instance> <table> <family>" << std::endl;
    return 1;
  }
  std::string const project_id = argv[1];
  std::string const instance_id = argv[2];
  std::string const table_id = argv[3];
  std::string const family = argv[4];
  std::string const filename = argv[5];

  // Create a connection to Cloud Bigtable and an object to manipulate the
  // specific table used in this demo.
  cbt::Table table(cbt::CreateDefaultDataClient(project_id, instance_id,
                                                cbt::ClientOptions()),
                   table_id);

  // How often do we print a progress message.
  int const report_progress_rate = 100000;
  // The maximum size of a bulk apply.
  int const bulk_apply_size = 10000;
  // The size of the circular buffer.
  int const buffer_size = 1000;
  // The size of the thread pool pushing data to Cloud Bigtable
  auto const thread_pool_size = std::thread::hardware_concurrency() - 1;

  // Create a circular buffer to communicate between the main thread that reads
  // the file and the threads that upload the parsed lines to Cloud Bigtable.
  CircularBuffer buffer(buffer_size);
  // Then create a few threads, each one of which pulls mutations out of the
  // circular buffer and then applies the mutation to the table.
  auto read_buffer = [&buffer, &table]() {
    cbt::BulkMutation mutation;
    while (buffer.Pop(mutation)) {
      table.BulkApply(std::move(mutation));
    }
  };
  std::vector<std::future<void>> workers;
  for (int i = 0; i != thread_pool_size; ++i) {
    workers.push_back(std::async(std::launch::async, read_buffer));
  }

  // The main thread just reads the file one line at a time.
  std::ifstream is(filename);
  std::string line;
  std::getline(is, line, '\n');
  int lineno = 0;
  auto headers = ParseLine(++lineno, line);

  std::cout << "Start reading input file " << std::flush;
  auto start = std::chrono::steady_clock::now();

  cbt::BulkMutation bulk;
  int count = 0;
  while (not is.eof()) {
    ++lineno;
    std::getline(is, line, '\n');
    auto parsed = ParseLine(lineno, line);

    using std::chrono::milliseconds;
    auto ts = std::chrono::duration_cast<milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
    // Create a mutation that inserts one column per field, the name of the
    // column is derived from the header.
    // TODO(coryan) - use an option to join several fields for the key.
    cbt::SingleRowMutation mutation(std::to_string(lineno));
    auto field_count = std::min(headers.size(), parsed.size());
    for (std::size_t i = 0; i != field_count; ++i) {
      mutation.emplace_back(cbt::SetCell(family, headers[i], ts, parsed[i]));
    }
    bulk.emplace_back(std::move(mutation));
    // If we have too many mutations in the bulk mutation it is time to push it
    // to the queue, where one of the running threads will pick it up.
    if (++count > bulk_apply_size) {
      buffer.Push(std::move(bulk));
      bulk = {};
      count = 0;
    }

    if (lineno % report_progress_rate == 0) {
      std::cout << '.' << std::flush;
    }
  }
  if (count != 0) {
    buffer.Push(std::move(bulk));
  }
  // Let the workers know that they can exit if they find an
  buffer.Shutdown();

  std::cout << "***" << std::flush;

  int worker_count = 0;
  for (auto& task : workers) {
    // If there was an exception in any thread continue, and report any
    // exceptions raised by other threads too.
    try {
      task.get();
    } catch (...) {
      std::cerr << "Exception raised by worker " << worker_count << std::endl;
    }
    ++worker_count;
  }

  auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::steady_clock::now() - start);
  std::cout << " DONE in " << elapsed.count() << "s" << std::endl;

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
  while (not tokens.eof()) {
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

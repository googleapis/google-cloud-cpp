// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <google/cloud/storage/client.h>
#include <boost/program_options.hpp>
#include <crc32c/crc32c.h>
#include <functional>
#include <future>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

namespace po = boost::program_options;
namespace gcs = google::cloud::storage;

std::atomic<std::int64_t> total_object_count;

std::string random_name_portion(std::mt19937_64& gen, int n) {
  static char const alphabet[] =
      "abcdefghijklmnopqrstuvwxyz"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "0123456789"
      "/-_.~@+=";
  std::string result;
  std::generate_n(std::back_inserter(result), n, [&gen] {
    return alphabet[std::uniform_int_distribution<int>(
        0, sizeof(alphabet) - 1)(gen)];
  });
  return result;
}

std::string random_prefix(std::mt19937_64& gen, std::string const& prefix) {
  return prefix + random_name_portion(gen, 8);
}

std::string hashed_name(bool use_hash_prefix, std::string object_name) {
  if (not use_hash_prefix) return std::move(object_name);

  // Just use the last 32-bits of the hash
  auto const hash = crc32c::Crc32c(object_name);

  char buf[16];
  std::snprintf(buf, sizeof(buf), "%08x_", hash);
  return buf + object_name;
}

std::vector<std::future<void>> launch_workers(std::string const& bucket,
                                              std::string const& prefix,
                                              bool use_hash_prefix,
                                              int task_count,
                                              long object_count) {
  if (object_count == 0) return {};
  if (task_count == 0) return {};

  std::vector<std::future<void>> tasks;
  int task_id = 0;
  std::generate_n(std::back_inserter(tasks), task_count, [&] {
    int task = task_id++;
    return std::async(std::launch::async, [=]() mutable {
      std::mt19937_64 generator(std::random_device{}());
      auto client = gcs::Client::CreateDefaultClient().value();
      auto make_basename = [&generator, &prefix] {
        auto basename = prefix;
        if (not basename.empty()) basename += '/';
        basename += random_name_portion(generator, 8);
        basename += '-';
        return basename;
      };
      std::string basename;
      for (long i = 0; i != object_count; ++i) {
        if (i % 100 == 0) basename = make_basename();
        if (i % task_count != task) continue;
        auto object_name = basename + std::to_string(i);
        auto hashed = hashed_name(use_hash_prefix, std::move(object_name));
        std::ostringstream os;
        os << "Prefix: " << prefix << "\nUse Hash Prefix: " << std::boolalpha
           << use_hash_prefix << "\nHashed Name: " << hashed
           << "\nObject Index: " << i << "\nTask Id: " << task << "\n";
        client.InsertObject(bucket, std::move(hashed), std::move(os).str())
            .value();
        ++total_object_count;
      }
    });
  });
  return tasks;
}

std::vector<std::future<void>> launch_tasks(std::string const& bucket,
                                            std::string const& prefix,
                                            bool use_hash_prefix,
                                            int task_count, long object_count) {
  if (object_count == 0) return {};
  if (task_count == 0) return {};
  if (prefix.size() >= 512 or task_count == 1) {
    return launch_workers(bucket, prefix, use_hash_prefix, task_count,
                          object_count);
  }

  // Initialize a random bit source with some small amount of entropy.
  std::mt19937_64 generator(std::random_device{}());
  auto tasks = launch_tasks(bucket, random_prefix(generator, prefix),
                            use_hash_prefix, task_count / 2, object_count / 2);
  auto worker_tasks = launch_workers(bucket, prefix, use_hash_prefix,
                                     task_count - task_count / 2,
                                     object_count - object_count / 2);

  tasks.insert(tasks.end(), std::move_iterator(worker_tasks.begin()),
               std::move_iterator(worker_tasks.end()));
  return tasks;
}

int main(int argc, char* argv[]) try {
  unsigned int const default_task_count = []() -> unsigned int {
    if (std::thread::hardware_concurrency() == 0) return 16;
    return 16 * std::thread::hardware_concurrency();
  }();
  long const default_object_count = 1'000'000;

  po::options_description desc(
      "Populate a GCS Bucket with randomly named objects");
  desc.add_options()("help", "produce help message")
      //
      ("bucket", po::value<std::string>()->required(),
       "set the source bucket name")
      //
      ("object-count", po::value<long>()->default_value(default_object_count))
      //
      ("use-hash-prefix", po::value<bool>()->default_value(true))
      //
      ("task-count",
       po::value<unsigned int>()->default_value(default_task_count));

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 0;
  }
  for (auto arg : {"bucket"}) {
    if (vm.count(arg) != 1 || vm[arg].as<std::string>().empty()) {
      std::cout << "The --" << arg
                << " option must be set to a non-empty value\n"
                << desc << "\n";
      return 1;
    }
  }

  auto const bucket = vm["bucket"].as<std::string>();
  auto const use_hash_prefix = vm["use-hash-prefix"].as<bool>();
  auto const task_count = vm["task-count"].as<unsigned int>();
  auto const object_count = vm["object-count"].as<long>();

  std::mt19937_64 generator(std::random_device{}());
  auto const run_prefix = random_name_portion(generator, 16);

  std::cout << "Creating " << object_count << " randomly named objects in "
            << bucket << std::endl;
  auto tasks = launch_tasks(bucket, run_prefix, use_hash_prefix, task_count,
                            object_count);
  std::cout << "Launched " << tasks.size() << " tasks... waiting" << std::endl;

  auto report_progress =
      [upload_start = std::chrono::steady_clock::now()](std::size_t active) {
        using std::chrono::duration_cast;
        using std::chrono::milliseconds;
        auto count = total_object_count.load();
        auto elapsed = std::chrono::steady_clock::now() - upload_start;
        if (count == 0 or elapsed.count() == 0) return;
        auto rate = count * 1000 / duration_cast<milliseconds>(elapsed).count();
        std::cout << "  Uploaded " << count << " objects (" << rate
                  << " objects/s, " << active << " task(s) still active)"
                  << std::endl;
      };
  while (not tasks.empty()) {
    using namespace std::chrono_literals;
    auto& w = tasks.back();
    auto status = w.wait_for(10s);
    if (status == std::future_status::ready) {
      tasks.pop_back();
      continue;
    }
    report_progress(tasks.size());
  }
  report_progress(tasks.size());
  std::cout << "DONE (" << total_object_count.load() << ")\n";

  return 0;
} catch (std::exception const& ex) {
  std::cerr << "Standard exception caught " << ex.what() << '\n';
  return 1;
}

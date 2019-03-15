// Copyright 2018 Google LLC
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

#include "google/cloud/internal/random.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/list_objects_reader.h"
#include <chrono>
#include <fstream>
#include <future>

namespace {

using ms = std::chrono::milliseconds;


void ConfigureWorkingResolver() {
  std::ofstream("/etc/resolv.conf") << R"""(
# Created for the crazy NOSIGNAL test, sorry.
search google.com
nameserver 8.8.8.8
nameserver 8.8.4.4
)""";
}

void ConfigureBrokenResolver() {
  std::ofstream("/etc/resolv.conf") << R"""(
# Created for the crazy NOSIGNAL test, sorry.
search google.com
nameserver 71.114.67.58
)""";
}

google::cloud::Status UploadFiles(std::string const& bucket_name,
                                  std::string const& media,
                                  std::vector<std::string> const& names) {
  namespace gcs = google::cloud::storage;
  auto client = gcs::Client::CreateDefaultClient();
  if (!client) {
    return client.status();
  }

  for (auto const& object_name : names) {
    // Raise on error so the errors are reported to the thread that launched
    // this function.
    auto meta =
        client->InsertObject(bucket_name, object_name, media,
                             gcs::IfGenerationMatch(0), gcs::Fields(""));
    if (!meta) {
      std::cerr << "Error uploading " << object_name << std::endl;
    }
    std::this_thread::sleep_for(ms(25));
  }
  return google::cloud::Status();
}

google::cloud::Status DownloadFiles(int iterations,
                                    std::string const& bucket_name,
                                    std::vector<std::string> const& names) {
  namespace gcs = google::cloud::storage;

  if (names.empty()) {
    // Nothing to do, should not happen, but checking explicitly so the code is
    // more readable.
    return google::cloud::Status(google::cloud::StatusCode::kInvalidArgument,
                                 "empty object name list");
  }

  auto client = gcs::Client::CreateDefaultClient();
  if (!client) {
    return client.status();
  }

  std::ofstream("/etc/resolv.conf") << R"""(
# Created for the crazy NOSIGNAL test, sorry.
search corp.google.com prod.google.com prodz.google.com google.com nyc.corp.google.com
nameserver 71.114.67.58
)""";

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  // Recall that both ends of `std::uniform_int_distribution` are inclusive.
  std::uniform_int_distribution<std::size_t> pick_name(0, names.size() - 1);

  for (int i = 0; i != iterations; ++i) {
    auto object_name = names.at(pick_name(generator));
    try {
      auto stream = client->ReadObject(bucket_name, object_name);
      std::string actual(std::istreambuf_iterator<char>{stream}, {});
    } catch (std::exception const& ex) {
      std::cerr << "Exception raised while downloading " << object_name
                << ex.what() << std::endl;
    }
    std::this_thread::sleep_for(ms(250));
  }
  return google::cloud::Status();
}

google::cloud::Status UploadDownloadThenIdle(
    std::string const& bucket_name, std::chrono::seconds idle_duration) {
  int const thread_count = 16;
  int const objects_per_thread = 40;
  int const object_count = thread_count * objects_per_thread;
  int const download_iterations = 1000 * objects_per_thread;
  int const object_size = 4 * 1024 * 1024;
  int const line_size = 128;

  auto generator = google::cloud::internal::MakeDefaultPRNG();
  std::string const letters =
      "abcdefghijklmnopqrstuvwxyz"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "012456789";

  std::vector<std::string> object_names;
  std::generate_n(
      std::back_inserter(object_names), object_count, [&generator, &letters] {
        return "ob-" + google::cloud::internal::Sample(generator, 32, letters) +
               ".txt";
      });

  std::string const media = [&generator, &letters](int total_bytes,
                                                   int bytes_per_line) {
    std::string media;
    for (int i = 0; i != total_bytes / bytes_per_line; ++i) {
      std::string line =
          google::cloud::internal::Sample(generator, line_size - 1, letters);
      line += '\n';
      media += line;
    }

    return media;
  }(object_size, line_size);

  ConfigureWorkingResolver();

  std::vector<std::future<google::cloud::Status>> uploads;
  auto names_block_begin = object_names.begin();
  for (int i = 0; i != thread_count; ++i) {
    auto names_block_end = names_block_begin;
    std::advance(names_block_end, objects_per_thread);
    std::vector<std::string> names{names_block_begin, names_block_end};
    uploads.emplace_back(std::async(std::launch::async, UploadFiles,
                                    bucket_name, media, std::move(names)));
    names_block_begin = names_block_end;
  }

  std::cout << "Waiting for uploads " << std::flush;
  for (auto&& fut : uploads) {
    auto status = fut.get();
    if (!status.ok()) {
      std::cerr << "Failure in upload thread: " << status << "\n";
    }
    std::cout << '.' << std::flush;
  }
  std::cout << " DONE\n" << std::flush;

  // Go idle for FLAG_idle_duration.
  ConfigureBrokenResolver();
  std::this_thread::sleep_for(idle_duration);

  std::vector<std::future<google::cloud::Status>> downloads;
  for (int i = 0; i != thread_count; ++i) {
    downloads.emplace_back(std::async(std::launch::async, DownloadFiles,
                                      download_iterations, bucket_name,
                                      object_names));
  }

  std::cout << "Waiting for downloads " << std::flush;
  for (auto&& fut : downloads) {
    auto status = fut.get();
    if (!status.ok()) {
      std::cerr << "Failure in download thread: " << status << "\n";
    }
    std::cout << '.' << std::flush;
  }
  std::cout << " DONE\n" << std::flush;

  // Go idle for FLAG_idle_duration.
  ConfigureWorkingResolver();
  std::this_thread::sleep_for(idle_duration);

  auto client = google::cloud::storage::Client::CreateDefaultClient();
  if (!client) {
    return client.status();
  }

  for (auto&& name : object_names) {
    auto status = client->DeleteObject(bucket_name, name);
    if (!status.ok()) {
      std::cerr << "Error deleting " << name << " " << status << "\n";
    }
  }

  // Go idle for FLAG_idle_duration.
  std::this_thread::sleep_for(idle_duration);
  return google::cloud::Status();
}

}  // namespace

int main(int argc, char* argv[]) {
  // Make sure the arguments are valid.
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <bucket-name> <idle-time>\n";
    return 1;
  }

  auto bucket_name = std::string(argv[1]);
  auto idle_duration = std::chrono::seconds(std::stoi(argv[2]));

  auto status = UploadDownloadThenIdle(bucket_name, idle_duration);
  if (!status.ok()) {
    std::cerr << "Error running test: " << status << "\n";
    return 1;
  }

  return 0;
}

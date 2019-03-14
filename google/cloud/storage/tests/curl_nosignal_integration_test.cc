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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/list_objects_reader.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/init_google_mock.h"
#include <gmock/gmock.h>
#include <chrono>
#include <future>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::TestPermanentFailure;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;

/// Store the options captured from the command-line arguments.
std::string FLAG_bucket_name;
std::chrono::seconds FLAG_idle_duration;

class CurlNoSignalIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {};

Status UploadFiles(std::string const& media,
                 std::vector<std::string> const& names) {
  auto bucket_name = FLAG_bucket_name;
  StatusOr<Client> client = Client::CreateDefaultClient();
  if (!client) {
    return client.status();
  }

  for (auto const& object_name : names) {
    // Raise on error so the errors are reported to the thread that launched
    // this function.
    auto meta = client
                              ->InsertObject(bucket_name, object_name, media,
                                             IfGenerationMatch(0), Fields(""));
    if (!meta) {
      return meta.status();
    }
  }
  return Status();
}

Status DownloadFiles(int iterations, std::vector<std::string> const& names) {
  if (names.empty()) {
    // Nothing to do, should not happen, but checking explicitly so the code is
    // more readable.
    return Status(StatusCode::kInvalidArgument, "empty object name list");
  }

  auto bucket_name = FLAG_bucket_name;
  StatusOr<Client> client = Client::CreateDefaultClient();
  if (!client) {
    return client.status();
  }

  std::random_device rd;
  std::mt19937_64 gen(rd());
  // Recall that both ends of `std::uniform_int_distribution` are inclusive.
  std::uniform_int_distribution<std::size_t> pick_name(0, names.size() - 1);

  for (int i = 0; i != iterations; ++i) {
    auto object_name = names.at(pick_name(gen));
    auto stream = client->ReadObject(bucket_name, object_name);
    std::string actual(std::istreambuf_iterator<char>{stream}, {});
    if (!stream.status().ok()) {
      return stream.status();
    }
  }
  return Status();
}

TEST_F(CurlNoSignalIntegrationTest, UploadDownloadThenIdle) {
  auto bucket_name = FLAG_bucket_name;

  int const thread_count = 16;
  int const objects_per_thread = 40;
  int const object_count = thread_count * objects_per_thread;
  int const download_iterations = 2 * objects_per_thread;
  int const object_size = 4 * 1024 * 1024;
  int const line_size = 128;

  std::vector<std::string> object_names;
  std::generate_n(std::back_inserter(object_names), object_count,
                  [this] { return MakeRandomObjectName(); });

  std::string media = [this] {
    std::ostringstream discard;
    std::ostringstream capture;
    WriteRandomLines(capture, discard, object_size / line_size, line_size);
    return std::move(capture).str();
  }();

  std::vector<std::future<Status>> uploads;
  auto names_block_begin = object_names.begin();
  for (int i = 0; i != thread_count; ++i) {
    auto names_block_end = names_block_begin;
    std::advance(names_block_end, objects_per_thread);
    std::vector<std::string> names{names_block_begin, names_block_end};
    uploads.emplace_back(
        std::async(std::launch::async, UploadFiles, media, std::move(names)));
    names_block_begin = names_block_end;
  }
  EXPECT_EQ(names_block_begin, object_names.end());

  std::cout << "Waiting for uploads " << std::flush;
  for (auto&& fut : uploads) {
    ASSERT_STATUS_OK(fut.get());
    std::cout << '.' << std::flush;
  }
  std::cout << " DONE\n" << std::flush;

  // Go idle for FLAG_idle_duration.
  std::this_thread::sleep_for(FLAG_idle_duration);

  std::vector<std::future<Status>> downloads;
  for (int i = 0; i != thread_count; ++i) {
    downloads.emplace_back(std::async(std::launch::async, DownloadFiles,
                                      download_iterations, object_names));
  }

  std::cout << "Waiting for downloads " << std::flush;
  for (auto&& fut : downloads) {
    ASSERT_STATUS_OK(fut.get());
    std::cout << '.' << std::flush;
  }
  std::cout << " DONE\n" << std::flush;

  // Go idle for FLAG_idle_duration.
  std::this_thread::sleep_for(FLAG_idle_duration);

  StatusOr<Client> client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  for (auto&& name : object_names) {
    auto status = client->DeleteObject(FLAG_bucket_name, name);
    EXPECT_STATUS_OK(status);
  }

  // Go idle for FLAG_idle_duration.
  std::this_thread::sleep_for(FLAG_idle_duration);
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  google::cloud::testing_util::InitGoogleMock(argc, argv);

  // Make sure the arguments are valid.
  if (argc != 3) {
    std::string const cmd = argv[0];
    auto last_slash = std::string(argv[0]).find_last_of('/');
    std::cerr << "Usage: " << cmd.substr(last_slash + 1)
              << " <bucket-name> <idle-time>\n";
    return 1;
  }

  google::cloud::storage::FLAG_bucket_name = argv[1];
  google::cloud::storage::FLAG_idle_duration =
      std::chrono::seconds(std::stoi(argv[2]));

  return RUN_ALL_TESTS();
}

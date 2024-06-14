// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/status.h"
#include "google/cloud/storagecontrol/v2/storage_control_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/example_driver.h"
#include <chrono>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

namespace {

void CreateFolder(google::cloud::storagecontrol_v2::StorageControlClient client,
                  std::vector<std::string> const& argv) {
  // [START storage_control_create_folder]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name, std::string const& folder_id) {
    auto const parent = std::string{"projects/_/buckets/"} + bucket_name;
    auto folder = client.CreateFolder(
        parent, google::storage::control::v2::Folder{}, folder_id);
    if (!folder) throw std::move(folder).status();

    std::cout << "Created folder: " << folder->name() << "\n";
  }
  // [END storage_control_create_folder]
  (std::move(client), argv.at(0), argv.at(1));
}

void DeleteFolder(google::cloud::storagecontrol_v2::StorageControlClient client,
                  std::vector<std::string> const& argv) {
  // [START storage_control_delete_folder]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name, std::string const& folder_id) {
    auto const name = std::string{"projects/_/buckets/"} + bucket_name +
                      std::string{"/folders/"} + folder_id;
    auto status = client.DeleteFolder(name);
    if (!status.ok()) throw std::move(status);

    std::cout << "Deleted folder: " << folder_id << "\n";
  }
  // [END storage_control_delete_folder]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetFolder(google::cloud::storagecontrol_v2::StorageControlClient client,
               std::vector<std::string> const& argv) {
  // [START storage_control_get_folder]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name, std::string const& folder_id) {
    auto const name = std::string{"projects/_/buckets/"} + bucket_name +
                      std::string{"/folders/"} + folder_id;
    auto folder = client.GetFolder(name);
    if (!folder) throw std::move(folder).status();

    std::cout << "Got folder: " << folder->name() << "\n";
  }
  // [END storage_control_get_folder]
  (std::move(client), argv.at(0), argv.at(1));
}

void ListFolders(google::cloud::storagecontrol_v2::StorageControlClient client,
                 std::vector<std::string> const& argv) {
  // [START storage_control_list_folders]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name) {
    auto const parent = std::string{"projects/_/buckets/"} + bucket_name;
    for (auto folder : client.ListFolders(parent)) {
      if (!folder) throw std::move(folder).status();
      std::cout << folder->name() << "\n";
    }

    std::cout << bucket_name << std::endl;
  }
  // [END storage_control_list_folders]
  (std::move(client), argv.at(0));
}

void RenameFolder(google::cloud::storagecontrol_v2::StorageControlClient client,
                  std::vector<std::string> const& argv) {
  // [START storage_control_rename_folder]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name, std::string const& source_folder_id,
     std::string const& dest_folder_id) {
    auto name = std::string{"projects/_/buckets/"} + bucket_name +
                std::string{"/folders/"} + source_folder_id;
    // Start a rename operation and block until it completes. Real applications
    // may want to setup a callback, wait on a coroutine, or poll until it
    // completes.
    auto renamed = client.RenameFolder(name, dest_folder_id).get();
    if (!renamed) throw std::move(renamed).status();

    std::cout << "Renamed: " << source_folder_id << " to: " << dest_folder_id
              << "\n";
  }
  // [END storage_control_rename_folder]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = google::cloud::testing_util;
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet(
      {"GOOGLE_CLOUD_CPP_STORAGE_TEST_FOLDER_BUCKET_NAME"});
  auto const bucket_name =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_STORAGE_TEST_FOLDER_BUCKET_NAME")
          .value();

  auto client = storagecontrol::StorageControlClient(
      storagecontrol::MakeStorageControlConnection());
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const prefix = "storage-control-samples";
  auto const folder_id = prefix + std::string{"-"} +
                         google::cloud::internal::Sample(
                             generator, 32, "abcdefghijklmnopqrstuvwxyz");
  auto const dest_folder_id = prefix + std::string{"-"} +
                              google::cloud::internal::Sample(
                                  generator, 32, "abcdefghijklmnopqrstuvwxyz");
  auto const create_time_limit =
      std::chrono::system_clock::now() - std::chrono::hours(48);
  // This is the only example that cleans up stale folders. The examples run in
  // parallel (within a build and across the builds), having multiple examples
  // doing the same cleanup is probably more trouble than it is worth.
  [](google::cloud::storagecontrol_v2::StorageControlClient client,
    std::string const& bucket_name, std::string const& prefix,
    std::chrono::system_clock::time_point created_time_limit) -> google::cloud::Status {
    std::cout << "\nRemoving stale folders for examples" << std::endl;
    std::regex re(prefix + R"re(-[a-z]{32})re");
    auto const parent = std::string{"projects/_/buckets/"} + bucket_name;
    for (auto folder : client.ListFolders(parent)) {
      if (!folder) return std::move(folder).status();
      if (!std::regex_match(folder->name(), re)) continue;
      auto const create_time =
          google::cloud::internal::ToChronoTimePoint(folder->create_time());
      if (create_time > created_time_limit) continue;
      (void)client.DeleteFolder(folder->name());
    }
    return {};
  }
  (std::move(client), bucket_name, prefix, create_time_limit);
  
  std::cout << "\nRunning CreateFolder() example" << std::endl;
  CreateFolder(client, {bucket_name, folder_id});

  std::cout << "\nRunning GetFolder() example" << std::endl;
  GetFolder(client, {bucket_name, folder_id});

  std::cout << "\nRunning ListFolders() example" << std::endl;
  ListFolders(client, {bucket_name});

  std::cout << "\nRunning RenameFolder() example" << std::endl;
  RenameFolder(client, {bucket_name, folder_id, dest_folder_id});

  std::cout << "\nRunning DeleteFolder() example" << std::endl;
  DeleteFolder(client, {bucket_name, dest_folder_id});
}

}  // namespace

int main(int argc, char* argv[]) {
  using google::cloud::testing_util::Example;
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  using ClientCommand =
      std::function<void(google::cloud::storagecontrol_v2::StorageControlClient,
                         std::vector<std::string> argv)>;

  auto make_entry = [](std::string name,
                       std::vector<std::string> const& arg_names,
                       ClientCommand const& command) {
    auto adapter = [=](std::vector<std::string> argv) {
      if ((argv.size() == 1 && argv[0] == "--help") ||
          argv.size() != arg_names.size()) {
        std::string usage = name;
        for (auto const& a : arg_names) usage += " <" + a + ">";
        throw google::cloud::testing_util::Usage{std::move(usage)};
      }
      auto client = storagecontrol::StorageControlClient(
          storagecontrol::MakeStorageControlConnection());
      ;
      command(client, std::move(argv));
    };
    return google::cloud::testing_util::Commands::value_type(std::move(name),
                                                             adapter);
  };

  Example example({
      make_entry("create-folder", {"bucket-name", "folder-id"}, CreateFolder),
      make_entry("delete-folder", {"bucket-name", "folder-id"}, DeleteFolder),
      make_entry("get-folder", {"bucket-name", "folder-id"}, GetFolder),
      make_entry("list-folders", {"bucket-name"}, ListFolders),
      make_entry("rename-folder",
                 {"bucket-name", "source-folder-id", "dest-folder-id"},
                 RenameFolder),
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}

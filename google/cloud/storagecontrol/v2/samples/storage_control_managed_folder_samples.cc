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

#include "google/cloud/storagecontrol/v2/storage_control_client.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/time_utils.h"
#include "google/cloud/testing_util/example_driver.h"
#include <google/storage/control/v2/storage_control.pb.h>
#include <chrono>
#include <iostream>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace {

void RemoveStaleManagedFolders(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::string const& bucket_name, std::string const& prefix,
    std::chrono::system_clock::time_point created_time_limit) {
  std::regex re(prefix + R"re(-[a-z]{32})re");
  auto const parent = std::string{"projects/_/buckets/"} + bucket_name;
  for (auto managed_folder : client.ListManagedFolders(parent)) {
    if (!managed_folder) throw std::move(managed_folder).status();
    if (!std::regex_match(managed_folder->name(), re)) continue;
    auto const create_time = google::cloud::internal::ToChronoTimePoint(
        managed_folder->create_time());
    if (create_time > created_time_limit) continue;
    (void)client.DeleteManagedFolder(managed_folder->name());
  }
}

void CreateManagedFolder(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_managed_folder_create]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name, std::string const& managed_folder_id) {
    auto const parent = std::string{"projects/_/buckets/"} + bucket_name;
    auto managed_folder = client.CreateManagedFolder(
        parent, google::storage::control::v2::ManagedFolder{},
        managed_folder_id);
    if (!managed_folder) throw std::move(managed_folder).status();

    std::cout << "Created managed folder: " << managed_folder->name() << "\n";
  }
  // [END storage_control_managed_folder_create]
  (std::move(client), argv.at(0), argv.at(1));
}

void DeleteManagedFolder(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_managed_folder_delete]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name, std::string const& managed_folder_id) {
    auto const name = std::string{"projects/_/buckets/"} + bucket_name +
                      "/managedFolders/" + managed_folder_id;
    auto status = client.DeleteManagedFolder(name);
    if (!status.ok()) throw std::move(status);

    std::cout << "Deleted managed folder: " << managed_folder_id << "\n";
  }
  // [END storage_control_managed_folder_delete]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetManagedFolder(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_managed_folder_get]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name, std::string const& managed_folder_id) {
    auto const name = std::string{"projects/_/buckets/"} + bucket_name +
                      "/managedFolders/" + managed_folder_id;
    auto managed_folder = client.GetManagedFolder(name);
    if (!managed_folder) throw std::move(managed_folder).status();

    std::cout << "Got managed folder: " << managed_folder->name() << "\n";
  }
  // [END storage_control_managed_folder_get]
  (std::move(client), argv.at(0), argv.at(1));
}

void ListManagedFolders(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_managed_folder_list]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name) {
    auto const parent = std::string{"projects/_/buckets/"} + bucket_name;
    for (auto managed_folder : client.ListManagedFolders(parent)) {
      if (!managed_folder) throw std::move(managed_folder).status();
      std::cout << managed_folder->name() << "\n";
    }

    std::cout << bucket_name << std::endl;
  }
  // [END storage_control_managed_folder_list]
  (std::move(client), argv.at(0));
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
  auto const prefix = std::string{"storage-control-samples"};
  auto const managed_folder_id =
      prefix + "-" +
      google::cloud::internal::Sample(generator, 32,
                                      "abcdefghijklmnopqrstuvwxyz");
  auto const create_time_limit =
      std::chrono::system_clock::now() - std::chrono::hours(48);
  // This is the only example that cleans up stale managed folders. The examples
  // run in parallel (within a build and across the builds), having multiple
  // examples doing the same cleanup is probably more trouble than it is worth.
  std::cout << "\nRemoving stale managed folders for examples" << std::endl;
  RemoveStaleManagedFolders(client, bucket_name, prefix, create_time_limit);

  std::cout << "\nRunning CreateManagedFolder() example" << std::endl;
  CreateManagedFolder(client, {bucket_name, managed_folder_id});

  std::cout << "\nRunning GetManagedFolder() example" << std::endl;
  GetManagedFolder(client, {bucket_name, managed_folder_id});

  std::cout << "\nRunning ListManagedFolders() example" << std::endl;
  ListManagedFolders(client, {bucket_name});

  std::cout << "\nRunning DeleteManagedFolder() example" << std::endl;
  DeleteManagedFolder(client, {bucket_name, managed_folder_id});
}

}  // namespace

int main(int argc, char* argv[]) {  // NOLINT(bugprone-exception-escape)
  using google::cloud::testing_util::Example;
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  using ClientCommand = std::function<void(storagecontrol::StorageControlClient,
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
      command(client, std::move(argv));
    };
    return google::cloud::testing_util::Commands::value_type(std::move(name),
                                                             adapter);
  };

  Example example({
      make_entry("create-managed-folder", {"bucket-name", "managed-folder-id"},
                 CreateManagedFolder),
      make_entry("delete-managed-folder", {"bucket-name", "managed-folder-id"},
                 DeleteManagedFolder),
      make_entry("get-managed-folder", {"bucket-name", "managed-folder-id"},
                 GetManagedFolder),
      make_entry("list-managed-folders", {"bucket-name"}, ListManagedFolders),
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}

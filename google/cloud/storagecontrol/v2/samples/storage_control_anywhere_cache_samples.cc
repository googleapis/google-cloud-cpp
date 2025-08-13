// Copyright 2025 Google LLC
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
#include "google/cloud/testing_util/example_driver.h"
#include "google/storage/control/v2/storage_control.pb.h"
#include <string>
#include <utility>
#include <vector>

namespace {

void CreateAnywhereCache(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_create_anywhere_cache]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name, std::string const& cache_name,
     std::string const& zone_name) {
    google::storage::control::v2::AnywhereCache cache;
    cache.set_name(cache_name);
    cache.set_zone(zone_name);

    google::storage::control::v2::CreateAnywhereCacheRequest request;
    request.set_parent(std::string{"projects/_/buckets/"} + bucket_name);
    *request.mutable_anywhere_cache() = cache;

    // Start a create operation and block until it completes. Real applications
    // may want to setup a callback, wait on a coroutine, or poll until it
    // completes.
    auto anywhere_cache = client.CreateAnywhereCache(request).get();
    if (!anywhere_cache) throw std::move(anywhere_cache).status();
    std::cout << "Created anywhere cache: " << anywhere_cache->name() << "\n";
  }
  // [END storage_control_create_anywhere_cache]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void GetAnywhereCache(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_get_anywhere_cache]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& cache_name) {
    auto anywhere_cache = client.GetAnywhereCache(cache_name);
    if (!anywhere_cache) throw std::move(anywhere_cache).status();
    std::cout << "Got anywhere cache: " << anywhere_cache->name() << "\n";
  }
  // [END storage_control_get_anywhere_cache]
  (std::move(client), argv.at(0));
}

void ListAnywhereCaches(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_list_anywhere_caches]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name) {
    auto const parent = std::string{"projects/_/buckets/"} + bucket_name;
    for (auto anywhere_cache : client.ListAnywhereCaches(parent)) {
      if (!anywhere_cache) throw std::move(anywhere_cache).status();
      std::cout << anywhere_cache->name() << "\n";
    }
  }
  // [END storage_control_list_anywhere_caches]
  (std::move(client), argv.at(0));
}

void UpdateAnywhereCache(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_update_anywhere_cache]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client, std::string const& cache_name,
     std::string const& admission_policy) {
    google::storage::control::v2::AnywhereCache cache;
    google::protobuf::FieldMask field_mask;
    field_mask.add_paths("admission_policy");
    cache.set_name(cache_name);
    cache.set_admission_policy(admission_policy);
    // Start an update operation and block until it completes. Real applications
    // may want to setup a callback, wait on a coroutine, or poll until it
    // completes.
    auto anywhere_cache = client.UpdateAnywhereCache(cache, field_mask).get();
    if (!anywhere_cache) throw std::move(anywhere_cache).status();
    std::cout << "Updated anywhere cache: " << anywhere_cache->name() << "\n";
  }
  // [END storage_control_update_anywhere_cache]
  (std::move(client), argv.at(0), argv.at(1));
}

void PauseAnywhereCache(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_pause_anywhere_cache]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& cache_name) {
    auto anywhere_cache = client.PauseAnywhereCache(cache_name);
    if (!anywhere_cache) throw std::move(anywhere_cache).status();
    std::cout << "Paused anywhere cache: " << anywhere_cache->name() << "\n";
  }
  // [END storage_control_pause_anywhere_cache]
  (std::move(client), argv.at(0));
}

void ResumeAnywhereCache(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_resume_anywhere_cache]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& cache_name) {
    auto anywhere_cache = client.ResumeAnywhereCache(cache_name);
    if (!anywhere_cache) throw std::move(anywhere_cache).status();
    std::cout << "Resumed anywhere cache: " << anywhere_cache->name() << "\n";
  }
  // [END storage_control_resume_anywhere_cache]
  (std::move(client), argv.at(0));
}

void DisableAnywhereCache(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_disable_anywhere_cache]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& cache_name) {
    auto anywhere_cache = client.DisableAnywhereCache(cache_name);
    if (!anywhere_cache) throw std::move(anywhere_cache).status();
    std::cout << "Disabled anywhere cache: " << anywhere_cache->name() << "\n";
  }
  // [END storage_control_disable_anywhere_cache]
  (std::move(client), argv.at(0));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = google::cloud::testing_util;
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet(
      {"GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME",
       "GOOGLE_CLOUD_CPP_TEST_ZONE"});
  auto const bucket_name = google::cloud::internal::GetEnv(
                               "GOOGLE_CLOUD_CPP_STORAGE_TEST_BUCKET_NAME")
                               .value();
  auto const zone_name =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_TEST_ZONE").value();
  auto client = storagecontrol::StorageControlClient(
      storagecontrol::MakeStorageControlConnection());

  auto const cache_name =
      "projects/_/buckets/" + bucket_name + "/anywhereCaches/" + zone_name;

  std::cout << "\nRunning CreateAnywhereCache() example" << std::endl;
  CreateAnywhereCache(client, {bucket_name, cache_name, zone_name});

  std::cout << "\nRunning GetAnywhereCache() example" << std::endl;
  GetAnywhereCache(client, {cache_name});

  std::cout << "\nRunning ListAnywhereCaches() example" << std::endl;
  ListAnywhereCaches(client, {bucket_name});

  std::cout << "\nRunning UpdateAnywhereCache() example" << std::endl;
  UpdateAnywhereCache(client, {cache_name, "admit-on-second-miss"});

  std::cout << "\nRunning PauseAnywhereCache() example" << std::endl;
  PauseAnywhereCache(client, {cache_name});

  std::cout << "\nRunning ResumeAnywhereCache() example" << std::endl;
  ResumeAnywhereCache(client, {cache_name});

  std::cout << "\nRunning DisableAnywhereCache() example" << std::endl;
  DisableAnywhereCache(client, {cache_name});
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
      make_entry("create-anywhere-cache",
                 {"bucket-name", "cache-name", "zone-name"},
                 CreateAnywhereCache),
      make_entry("get-anywhere-cache", {"cache-name"}, GetAnywhereCache),
      make_entry("list-anywhere-caches", {"bucket-name"}, ListAnywhereCaches),
      make_entry("update-anywhere-cache", {"cache-name", "admission-policy"},
                 UpdateAnywhereCache),
      make_entry("pause-anywhere-cache", {"cache-name"}, PauseAnywhereCache),
      make_entry("resume-anywhere-cache", {"cache-name"}, ResumeAnywhereCache),
      make_entry("disable-anywhere-cache", {"cache-name"},
                 DisableAnywhereCache),
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}

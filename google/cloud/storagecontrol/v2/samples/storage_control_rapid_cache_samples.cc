// Copyright 2026 Google LLC
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
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/field_mask.pb.h>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

void CreateRapidCache(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_create_rapid_cache]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name, std::string const& cache_id,
     std::string const& zone_name) {
    google::storage::control::v2::RapidCache cache;
    cache.set_name("projects/_/buckets/" + bucket_name + "/rapidCaches/" +
                   cache_id);
    cache.set_zone(zone_name);
    cache.set_cache_type("rapid-cache-ultra");
    cache.mutable_ttl()->set_seconds(86400);  // 24 hours

    auto const parent = "projects/_/buckets/" + bucket_name;
    auto operation = client.CreateRapidCache(parent, cache).get();
    if (!operation) throw std::move(operation).status();
    std::cout << "Created rapid cache: " << operation->name() << "\n";
    std::cout << "State: " << operation->state() << "\n";
  }
  // [END storage_control_create_rapid_cache]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void GetRapidCache(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_get_rapid_cache]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& cache_name) {
    auto cache = client.GetRapidCache(cache_name);
    if (!cache) throw std::move(cache).status();
    std::cout << "Got rapid cache: " << cache->name() << "\n";
    std::cout << "State: " << cache->state() << "\n";
  }
  // [END storage_control_get_rapid_cache]
  (std::move(client), argv.at(0));
}

void ListRapidCaches(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_list_rapid_caches]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name) {
    auto const parent = std::string{"projects/_/buckets/"} + bucket_name;
    for (auto cache : client.ListRapidCaches(parent)) {
      if (!cache) throw std::move(cache).status();
      std::cout << cache->name() << "\n";
    }
  }
  // [END storage_control_list_rapid_caches]
  (std::move(client), argv.at(0));
}

void UpdateRapidCache(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_update_rapid_cache]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& cache_name) {
    google::storage::control::v2::RapidCache cache;
    google::protobuf::FieldMask field_mask;
    field_mask.add_paths("ttl");
    cache.set_name(cache_name);
    cache.mutable_ttl()->set_seconds(172800);  // 48 hours

    auto operation = client.UpdateRapidCache(cache, field_mask).get();
    if (!operation) throw std::move(operation).status();
    std::cout << "Updated rapid cache: " << operation->name() << "\n";
  }
  // [END storage_control_update_rapid_cache]
  (std::move(client), argv.at(0));
}

void DisableRapidCache(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_disable_rapid_cache]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& cache_name) {
    auto operation = client.DisableRapidCache(cache_name).get();
    if (!operation) throw std::move(operation).status();
    std::cout << "Disabled rapid cache: " << operation->name() << "\n";
  }
  // [END storage_control_disable_rapid_cache]
  (std::move(client), argv.at(0));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = google::cloud::testing_util;
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  if (!argv.empty()) throw examples::Usage{"auto"};

  // Creating a rapid cache is a billable operation that needs a regional,
  // hierarchical-namespace bucket, so these examples only run when a dedicated
  // bucket and zone are configured.
  auto const bucket_name =
      google::cloud::internal::GetEnv(
          "GOOGLE_CLOUD_CPP_STORAGE_TEST_RAPID_BUCKET_NAME")
          .value_or("");
  auto const zone_name = google::cloud::internal::GetEnv(
                             "GOOGLE_CLOUD_CPP_STORAGE_TEST_RAPID_ZONE")
                             .value_or("");
  if (bucket_name.empty() || zone_name.empty()) {
    std::cout << "Set GOOGLE_CLOUD_CPP_STORAGE_TEST_RAPID_BUCKET_NAME and "
                 "GOOGLE_CLOUD_CPP_STORAGE_TEST_RAPID_ZONE to run the rapid "
                 "cache examples. Skipping.\n";
    return;
  }

  auto client = storagecontrol::StorageControlClient(
      storagecontrol::MakeStorageControlConnection());

  // The service uses the zone name as the rapid cache id.
  auto const cache_name =
      "projects/_/buckets/" + bucket_name + "/rapidCaches/" + zone_name;

  std::cout << "\nRunning CreateRapidCache() example" << std::endl;
  CreateRapidCache(client, {bucket_name, zone_name, zone_name});

  std::cout << "\nRunning GetRapidCache() example" << std::endl;
  GetRapidCache(client, {cache_name});

  std::cout << "\nRunning ListRapidCaches() example" << std::endl;
  ListRapidCaches(client, {bucket_name});

  std::cout << "\nRunning UpdateRapidCache() example" << std::endl;
  UpdateRapidCache(client, {cache_name});

  std::cout << "\nRunning DisableRapidCache() example" << std::endl;
  DisableRapidCache(client, {cache_name});
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
      make_entry("create-rapid-cache", {"bucket-name", "cache-id", "zone-name"},
                 CreateRapidCache),
      make_entry("get-rapid-cache", {"cache-name"}, GetRapidCache),
      make_entry("list-rapid-caches", {"bucket-name"}, ListRapidCaches),
      make_entry("update-rapid-cache", {"cache-name"}, UpdateRapidCache),
      make_entry("disable-rapid-cache", {"cache-name"}, DisableRapidCache),
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}

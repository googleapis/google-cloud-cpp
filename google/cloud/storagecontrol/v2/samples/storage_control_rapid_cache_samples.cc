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
#include "google/cloud/no_await_tag.h"
#include "google/cloud/testing_util/example_driver.h"
#include "google/storage/control/v2/storage_control.pb.h"
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

void CreateRapidCache(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_create_rapid_cache]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name, std::string const& zone_name) {
    // The zone must be in the same region as the bucket. Rapid Cache Ultra
    // requires a bucket with hierarchical namespace enabled.
    google::storage::control::v2::RapidCache cache;
    cache.set_zone(zone_name);
    cache.set_cache_type("rapid-cache-ultra");

    auto const parent = std::string{"projects/_/buckets/"} + bucket_name;
    // Creating a Rapid Cache is a long-running operation. This example blocks
    // until it completes. Real applications may want to set up a callback, wait
    // on a coroutine, or poll until it completes.
    auto rapid_cache = client.CreateRapidCache(parent, cache).get();
    if (!rapid_cache) throw std::move(rapid_cache).status();
    std::cout << "Created Rapid Cache: " << rapid_cache->name() << "\n";
  }
  // [END storage_control_create_rapid_cache]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetRapidCache(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_get_rapid_cache]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name, std::string const& cache_id) {
    auto const name = std::string{"projects/_/buckets/"} + bucket_name +
                      "/rapidCaches/" + cache_id;
    auto rapid_cache = client.GetRapidCache(name);
    if (!rapid_cache) throw std::move(rapid_cache).status();
    std::cout << "Got Rapid Cache: " << rapid_cache->name() << "\n"
              << "Zone: " << rapid_cache->zone() << "\n"
              << "State: " << rapid_cache->state() << "\n";
  }
  // [END storage_control_get_rapid_cache]
  (std::move(client), argv.at(0), argv.at(1));
}

void ListRapidCaches(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_list_rapid_caches]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name) {
    auto const parent = std::string{"projects/_/buckets/"} + bucket_name;
    // The returned range fetches additional pages as needed.
    for (auto rapid_cache : client.ListRapidCaches(parent)) {
      if (!rapid_cache) throw std::move(rapid_cache).status();
      std::cout << rapid_cache->name() << "\n";
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
     std::string const& bucket_name, std::string const& cache_id) {
    google::storage::control::v2::RapidCache cache;
    cache.set_name(std::string{"projects/_/buckets/"} + bucket_name +
                   "/rapidCaches/" + cache_id);
    cache.mutable_ttl()->set_seconds(172800);
    // Only the fields listed in the update mask are changed.
    google::protobuf::FieldMask field_mask;
    field_mask.add_paths("ttl");

    // Updating a Rapid Cache is a long-running operation. This example blocks
    // until it completes. Real applications may want to set up a callback, wait
    // on a coroutine, or poll until it completes.
    auto rapid_cache = client.UpdateRapidCache(cache, field_mask).get();
    if (!rapid_cache) throw std::move(rapid_cache).status();
    std::cout << "Updated Rapid Cache: " << rapid_cache->name() << "\n"
              << "TTL: " << rapid_cache->ttl().seconds() << " seconds\n";
  }
  // [END storage_control_update_rapid_cache]
  (std::move(client), argv.at(0), argv.at(1));
}

void DisableRapidCache(
    google::cloud::storagecontrol_v2::StorageControlClient client,
    std::vector<std::string> const& argv) {
  // [START storage_control_disable_rapid_cache]
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  [](storagecontrol::StorageControlClient client,
     std::string const& bucket_name, std::string const& cache_id) {
    auto const name = std::string{"projects/_/buckets/"} + bucket_name +
                      "/rapidCaches/" + cache_id;
    // Disabling a Rapid Cache starts a long-running operation. The cache moves
    // to the "disabled" state within seconds, but the operation only completes
    // once the cache has been fully removed, which can take more than 30
    // minutes. This example starts the operation without waiting for it to
    // complete; use GetRapidCache() to check the cache state.
    auto operation =
        client.DisableRapidCache(google::cloud::NoAwaitTag{}, name);
    if (!operation) throw std::move(operation).status();
    std::cout << "Disabling Rapid Cache: " << name << "\n"
              << "Operation: " << operation->name() << "\n";
  }
  // [END storage_control_disable_rapid_cache]
  (std::move(client), argv.at(0), argv.at(1));
}

void AutoRun(std::vector<std::string> const& argv) {
  namespace examples = google::cloud::testing_util;
  namespace storagecontrol = google::cloud::storagecontrol_v2;
  if (!argv.empty()) throw examples::Usage{"auto"};
  // Rapid Cache Ultra requires a regional bucket with hierarchical namespace
  // enabled, in the same region as the cache zone. The shared test buckets do
  // not meet these requirements, so the examples only run when a suitable
  // bucket is configured. A disabled Rapid Cache takes 30+ minutes to be
  // removed, so back-to-back runs need to wait, or use a different zone.
  auto const bucket_name = google::cloud::internal::GetEnv(
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_RAPID_CACHE_BUCKET_NAME");
  if (!bucket_name.has_value() || bucket_name->empty()) {
    std::cout << "Skipping Rapid Cache examples: "
              << "GOOGLE_CLOUD_CPP_STORAGE_TEST_RAPID_CACHE_BUCKET_NAME is "
              << "not set\n";
    return;
  }
  auto zone_name = google::cloud::internal::GetEnv(
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_RAPID_CACHE_ZONE");
  if (!zone_name) {
    zone_name = google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_TEST_ZONE")
                    .value_or("us-central1-a");
  }
  auto client = storagecontrol::StorageControlClient(
      storagecontrol::MakeStorageControlConnection());

  std::cout << "\nRunning CreateRapidCache() example" << std::endl;
  CreateRapidCache(client, {*bucket_name, *zone_name});

  // The cache ID is assigned by the service. Find it using the zone, ignoring
  // any caches disabled by previous runs. If this lookup fails the new cache
  // cannot be disabled here, and must be disabled manually.
  auto const parent = std::string{"projects/_/buckets/"} + *bucket_name;
  std::string cache_id;
  for (auto cache : client.ListRapidCaches(parent)) {
    if (!cache) throw std::move(cache).status();
    if (cache->zone() != *zone_name || cache->state() == "disabled") continue;
    cache_id = cache->name().substr(cache->name().rfind('/') + 1);
    break;
  }
  if (cache_id.empty()) {
    throw std::runtime_error("Cannot find the Rapid Cache in " + *zone_name);
  }

  try {
    std::cout << "\nRunning GetRapidCache() example" << std::endl;
    GetRapidCache(client, {*bucket_name, cache_id});

    std::cout << "\nRunning ListRapidCaches() example" << std::endl;
    ListRapidCaches(client, {*bucket_name});

    std::cout << "\nRunning UpdateRapidCache() example" << std::endl;
    UpdateRapidCache(client, {*bucket_name, cache_id});
  } catch (...) {
    // Best-effort cleanup, the original error is more interesting.
    try {
      DisableRapidCache(client, {*bucket_name, cache_id});
    } catch (...) {
    }
    throw;
  }

  std::cout << "\nRunning DisableRapidCache() example" << std::endl;
  DisableRapidCache(client, {*bucket_name, cache_id});

  // The cache moves to the "disabled" state shortly after the request.
  auto const name = parent + "/rapidCaches/" + cache_id;
  for (int i = 0; i != 12; ++i) {
    auto cache = client.GetRapidCache(name);
    if (!cache) throw std::move(cache).status();
    if (cache->state() == "disabled") {
      std::cout << "Rapid Cache " << name << " is disabled\n";
      return;
    }
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }
  throw std::runtime_error("Rapid Cache " + name + " was not disabled");
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
      make_entry("create-rapid-cache", {"bucket-name", "zone-name"},
                 CreateRapidCache),
      make_entry("get-rapid-cache", {"bucket-name", "cache-id"}, GetRapidCache),
      make_entry("list-rapid-caches", {"bucket-name"}, ListRapidCaches),
      make_entry("update-rapid-cache", {"bucket-name", "cache-id"},
                 UpdateRapidCache),
      make_entry("disable-rapid-cache", {"bucket-name", "cache-id"},
                 DisableRapidCache),
      {"auto", AutoRun},
  });
  return example.Run(argc, argv);
}

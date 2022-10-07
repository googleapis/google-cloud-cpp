// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/internal/getenv.h"
#include <functional>
#include <iostream>
#include <thread>

namespace {

void GetAutoclass(google::cloud::storage::Client client,
                  std::vector<std::string> const& argv) {
  //! [get-autoclass] [START storage_get_autoclass]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name) {
    auto metadata = client.GetBucketMetadata(bucket_name);
    if (!metadata) throw google::cloud::Status(std::move(metadata).status());

    if (!metadata->has_autoclass()) {
      std::cout << "The bucket " << metadata->name() << " does not have an"
                << " autoclass configuration.\n";
      return;
    }

    std::cout << "Autoclass is "
              << (metadata->autoclass().enabled ? "enabled" : "disabled")
              << " for bucket " << metadata->name() << ". "
              << " The bucket's full autoclass configuration is "
              << metadata->autoclass() << "\n";
  }
  //! [get-autoclass] [END storage_get_autoclass]
  (std::move(client), argv.at(0));
}

void SetAutoclass(google::cloud::storage::Client client,
                  std::vector<std::string> const& argv) {
  using ::google::cloud::storage::examples::Usage;
  if (argv.at(1) != "true" && argv.at(1) != "false") {
    throw Usage{"enabled must be either 'true' or 'false'"};
  }
  auto const enabled = argv.at(1) == "true";
  //! [set-autoclass] [START storage_set_autoclass]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name, bool enabled) {
    auto metadata = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().SetAutoclass(
                         gcs::BucketAutoclass{enabled}));
    if (!metadata) throw google::cloud::Status(std::move(metadata).status());

    std::cout << "The autoclass configuration for bucket " << bucket_name
              << " was successfully updated.";
    if (!metadata->has_autoclass()) {
      std::cout << " The bucket no longer has an autoclass configuration.\n";
      return;
    }
    std::cout << " The new configuration is " << metadata->autoclass() << "\n";
  }
  //! [set-autoclass] [END storage_set_autoclass]
  (std::move(client), argv.at(0), enabled);
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const bucket_name_enabled = examples::MakeRandomBucketName(generator);
  auto const bucket_name_disabled = examples::MakeRandomBucketName(generator);
  auto const object_name =
      examples::MakeRandomObjectName(generator, "object-") + ".txt";
  auto client = gcs::Client();

  std::cout << "\nCreating buckets to run the example:"
            << "\nEnabled Autoclass: " << bucket_name_enabled
            << "\nDisabled Autoclass: " << bucket_name_disabled << std::endl;
  // In GCS a single project cannot create or delete buckets more often than
  // once every two seconds. We will pause until that time before deleting the
  // bucket.
  auto constexpr kBucketPeriod = std::chrono::seconds(2);
  auto pause = std::chrono::steady_clock::now() + kBucketPeriod;
  (void)client
      .CreateBucketForProject(
          bucket_name_enabled, project_id,
          gcs::BucketMetadata{}.set_autoclass(gcs::BucketAutoclass{true}))
      .value();
  if (!examples::UsingEmulator()) std::this_thread::sleep_until(pause);
  pause = std::chrono::steady_clock::now() + kBucketPeriod;
  (void)client
      .CreateBucketForProject(
          bucket_name_disabled, project_id,
          gcs::BucketMetadata{}.set_autoclass(gcs::BucketAutoclass{false}))
      .value();

  std::cout << "\nRunning GetAutoclass() example [enabled]" << std::endl;
  GetAutoclass(client, {bucket_name_enabled});

  std::cout << "\nRunning GetAutoclass() example [disabled]" << std::endl;
  GetAutoclass(client, {bucket_name_disabled});

  std::cout << "\nRunning SetAutoclass() example" << std::endl;
  SetAutoclass(client, {bucket_name_enabled, "false"});

  if (!examples::UsingEmulator()) std::this_thread::sleep_until(pause);
  (void)examples::RemoveBucketAndContents(client, bucket_name_enabled);
  (void)examples::RemoveBucketAndContents(client, bucket_name_disabled);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  examples::Example example({
      examples::CreateCommandEntry("get-autoclass", {"<bucket-name>"},
                                   GetAutoclass),
      examples::CreateCommandEntry(
          "set-autoclass", {"<bucket-name>", "<enabled>"}, SetAutoclass),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

// Copyright 2020 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/examples/storage_examples_common.h"
#include "google/cloud/internal/getenv.h"
#include <functional>
#include <iostream>
#include <thread>

namespace {

void GetDefaultEventBasedHold(google::cloud::storage::Client client,
                              std::vector<std::string> const& argv) {
  //! [get default event based hold]
  // [START storage_get_default_event_based_hold]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    std::cout << "The default event-based hold for objects in bucket "
              << bucket_metadata->name() << " is "
              << (bucket_metadata->default_event_based_hold() ? "enabled"
                                                              : "disabled")
              << "\n";
  }
  // [END storage_get_default_event_based_hold]
  //! [get default event based hold]
  (std::move(client), argv.at(0));
}

void EnableDefaultEventBasedHold(google::cloud::storage::Client client,
                                 std::vector<std::string> const& argv) {
  //! [enable default event based hold]
  // [START storage_enable_default_event_based_hold]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) throw std::runtime_error(original.status().message());
    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetDefaultEventBasedHold(true),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      throw std::runtime_error(patched_metadata.status().message());
    }

    std::cout << "The default event-based hold for objects in bucket "
              << bucket_name << " is "
              << (patched_metadata->default_event_based_hold() ? "enabled"
                                                               : "disabled")
              << "\n";
  }
  // [END storage_enable_default_event_based_hold]
  //! [enable default event based hold]
  (std::move(client), argv.at(0));
}

void DisableDefaultEventBasedHold(google::cloud::storage::Client client,
                                  std::vector<std::string> const& argv) {
  //! [disable default event based hold]
  // [START storage_disable_default_event_based_hold]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) throw std::runtime_error(original.status().message());
    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetDefaultEventBasedHold(false),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      throw std::runtime_error(patched_metadata.status().message());
    }

    std::cout << "The default event-based hold for objects in bucket "
              << bucket_name << " is "
              << (patched_metadata->default_event_based_hold() ? "enabled"
                                                               : "disabled")
              << "\n";
  }
  // [END storage_disable_default_event_based_hold]
  //! [disable default event based hold]
  (std::move(client), argv.at(0));
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
  auto const bucket_name = examples::MakeRandomBucketName(generator);
  auto client = gcs::Client();

  std::cout << "\nCreating bucket to run the examples" << std::endl;
  (void)client.CreateBucketForProject(bucket_name, project_id,
                                      gcs::BucketMetadata{});
  // In GCS a single project cannot create or delete buckets more often than
  // once every two seconds. We will pause until that time before deleting the
  // bucket.
  auto pause = std::chrono::steady_clock::now() + std::chrono::seconds(2);

  std::cout << "\nRunning GetDefaultEventBasedHold() example" << std::endl;
  GetDefaultEventBasedHold(client, {bucket_name});

  std::cout << "\nRunning EnableDefaultEventBasedHold() example" << std::endl;
  EnableDefaultEventBasedHold(client, {bucket_name});

  std::cout << "\nRunning DisableDefaultEventBasedHold() example" << std::endl;
  DisableDefaultEventBasedHold(client, {bucket_name});

  std::cout << "\nCleaning up" << std::endl;
  if (!examples::UsingEmulator()) std::this_thread::sleep_until(pause);
  (void)examples::RemoveBucketAndContents(client, bucket_name);
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  auto make_entry = [](std::string const& name,
                       std::vector<std::string> arg_names,
                       examples::ClientCommand const& cmd) {
    arg_names.insert(arg_names.begin(), "<bucket-name>");
    return examples::CreateCommandEntry(name, std::move(arg_names), cmd);
  };

  examples::Example example({
      make_entry("get-default-event-based-hold", {}, GetDefaultEventBasedHold),
      make_entry("enable-default-event-based-hold", {},
                 EnableDefaultEventBasedHold),
      make_entry("disable-default-event-based-hold", {},
                 DisableDefaultEventBasedHold),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

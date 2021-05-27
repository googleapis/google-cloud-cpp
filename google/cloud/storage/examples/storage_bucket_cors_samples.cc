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

void SetCorsConfiguration(google::cloud::storage::Client client,
                          std::vector<std::string> const& argv) {
  //! [cors configuration] [START storage_cors_configuration]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& origin) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) throw std::runtime_error(original.status().message());
    std::vector<gcs::CorsEntry> cors_configuration;
    cors_configuration.emplace_back(
        gcs::CorsEntry{3600, {"GET"}, {origin}, {"Content-Type"}});

    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetCors(cors_configuration),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      throw std::runtime_error(patched_metadata.status().message());
    }

    if (patched_metadata->cors().empty()) {
      std::cout << "Cors configuration is not set for bucket "
                << patched_metadata->name() << "\n";
      return;
    }

    std::cout << "Cors configuration successfully set for bucket "
              << patched_metadata->name() << "\nNew cors configuration: ";
    for (auto const& cors_entry : patched_metadata->cors()) {
      std::cout << "\n  " << cors_entry << "\n";
    }
  }
  //! [cors configuration] [END storage_cors_configuration]
  (std::move(client), argv.at(0), argv.at(1));
}

void RemoveCorsConfiguration(google::cloud::storage::Client client,
                             std::vector<std::string> const& argv) {
  // [START storage_remove_cors_configuration]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);
    if (!original) throw std::runtime_error(original.status().message());

    StatusOr<gcs::BucketMetadata> patched = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().ResetCors(),
        gcs::IfMetagenerationMatch(original->metageneration()));
    if (!patched) throw std::runtime_error(patched.status().message());

    std::cout << "Cors configuration successfully removed for bucket "
              << patched->name() << "\n";
  }
  // [END storage_remove_cors_configuration]
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

  std::cout << "\nCreating bucket to run the example (" << bucket_name << ")"
            << std::endl;
  (void)client
      .CreateBucketForProject(bucket_name, project_id, gcs::BucketMetadata{})
      .value();
  // In GCS a single project cannot create or delete buckets more often than
  // once every two seconds. We will pause until that time before deleting the
  // bucket.
  auto pause = std::chrono::steady_clock::now() + std::chrono::seconds(2);

  std::cout << "\nRunning the SetCorsConfiguration() example" << std::endl;
  SetCorsConfiguration(client, {bucket_name, "http://origin1.example.com"});

  std::cout << "\nRunning the RemoveCorsConfiguration() example" << std::endl;
  RemoveCorsConfiguration(client, {bucket_name});

  if (!examples::UsingEmulator()) std::this_thread::sleep_until(pause);
  (void)examples::RemoveBucketAndContents(client, bucket_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  examples::Example example({
      examples::CreateCommandEntry("set-cors-configuration",
                                   {"<bucket-name>", "<config>"},
                                   SetCorsConfiguration),
      examples::CreateCommandEntry("remove-cors-configuration",
                                   {"<bucket-name>"}, RemoveCorsConfiguration),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

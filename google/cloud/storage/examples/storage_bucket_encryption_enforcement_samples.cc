// Copyright 2026 Google LLC
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
#include "google/cloud/internal/format_time_point.h"
#include <iostream>
#include <thread>
#include <vector>

namespace {

void GetBucketEncryptionEnforcementConfig(
    google::cloud::storage::Client client,
    std::vector<std::string> const& argv) {
  //! [get bucket encryption enforcement config] [START storage_get_bucket_encryption_enforcement_config]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> metadata =
        client.GetBucketMetadata(bucket_name);
    if (!metadata) throw std::move(metadata).status();

    std::cout << "Bucket Name: " << metadata->name() << "\n";

    if (!metadata->has_encryption()) {
      std::cout << "  GMEK Enforcement: NOT SET (Default)\n"
                << "  CMEK Enforcement: NOT SET (Default)\n"
                << "  CSEK Enforcement: NOT SET (Default)\n";
      return;
    }

    auto const& encryption = metadata->encryption();

    auto format_config = [](auto const& config) {
      if (config.restriction_mode.empty()) return std::string("NOT SET (Default)");
      return "Mode: " + config.restriction_mode + ", Effective Time: " +
             google::cloud::internal::FormatRfc3339(config.effective_time);
    };

    std::cout << "  GMEK Enforcement: "
              << format_config(encryption.google_managed_encryption_enforcement_config) << "\n"
              << "  CMEK Enforcement: "
              << format_config(encryption.customer_managed_encryption_enforcement_config) << "\n"
              << "  CSEK Enforcement: "
              << format_config(encryption.customer_supplied_encryption_enforcement_config) << "\n";
  }
  //! [get bucket encryption enforcement config] [END storage_get_bucket_encryption_enforcement_config]
  (std::move(client), argv.at(0));
}

void SetBucketEncryptionEnforcementConfig(
    google::cloud::storage::Client client,
    std::vector<std::string> const& argv) {
  //! [set bucket encryption enforcement config] [START storage_set_bucket_encryption_enforcement_config]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    gcs::BucketEncryption encryption;
    encryption.google_managed_encryption_enforcement_config.restriction_mode =
        "NOT_RESTRICTED";
    encryption.customer_managed_encryption_enforcement_config.restriction_mode =
        "FULLY_RESTRICTED";
    encryption.customer_supplied_encryption_enforcement_config.restriction_mode =
        "FULLY_RESTRICTED";

    StatusOr<gcs::BucketMetadata> updated = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().SetEncryption(encryption));
    if (!updated) throw std::move(updated).status();

    std::cout << "Successfully set encryption enforcement config on bucket "
              << updated->name() << ".\n";
  }
  //! [set bucket encryption enforcement config] [END storage_set_bucket_encryption_enforcement_config]
  (std::move(client), argv.at(0));
}

void RemoveAllEncryptionEnforcementConfig(
    google::cloud::storage::Client client,
    std::vector<std::string> const& argv) {
  //! [remove all encryption enforcement config] [START storage_remove_all_encryption_enforcement_config]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    // To remove the enforcement policy, the fields must be explicitly cleared.
    // Here we clear the entire encryption configuration.
    StatusOr<gcs::BucketMetadata> updated = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().ResetEncryption());
    if (!updated) throw std::move(updated).status();

    std::cout << "Successfully removed encryption enforcement config for bucket "
              << updated->name() << ".\n";
  }
  //! [remove all encryption enforcement config] [END storage_remove_all_encryption_enforcement_config]
  (std::move(client), argv.at(0));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({"GOOGLE_CLOUD_PROJECT"});
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const bucket_name = examples::MakeRandomBucketName(generator);
  auto client = gcs::Client();

  std::cout << "\nCreating bucket to run the example (" << bucket_name << ")"
            << std::endl;
  (void)client
      .CreateBucketForProject(bucket_name, project_id, gcs::BucketMetadata{},
                              examples::CreateBucketOptions())
      .value();
  // In GCS a single project cannot create or delete buckets more often than
  // once every two seconds. We will pause until that time before deleting the
  // bucket.
  auto pause = std::chrono::steady_clock::now() + std::chrono::seconds(2);

  std::cout << "\nRunning the SetBucketEncryptionEnforcementConfig() example"
            << std::endl;
  SetBucketEncryptionEnforcementConfig(client, {bucket_name});

  std::cout << "\nRunning the GetBucketEncryptionEnforcementConfig() example [1]"
            << std::endl;
  GetBucketEncryptionEnforcementConfig(client, {bucket_name});

  std::cout << "\nRunning the RemoveAllEncryptionEnforcementConfig() example"
            << std::endl;
  RemoveAllEncryptionEnforcementConfig(client, {bucket_name});

  std::cout << "\nRunning the GetBucketEncryptionEnforcementConfig() example [2]"
            << std::endl;
  GetBucketEncryptionEnforcementConfig(client, {bucket_name});

  if (!examples::UsingEmulator()) std::this_thread::sleep_until(pause);
  (void)examples::RemoveBucketAndContents(client, bucket_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  auto make_entry = [](std::string const& name,
                       std::vector<std::string> arg_names,
                       examples::ClientCommand const& cmd) {
    return examples::CreateCommandEntry(name, std::move(arg_names), cmd);
  };
  examples::Example example({
      make_entry("get-bucket-encryption-enforcement-config", {"<bucket-name>"},
                 GetBucketEncryptionEnforcementConfig),
      make_entry("set-bucket-encryption-enforcement-config",
                 {"<bucket-name>"},
                 SetBucketEncryptionEnforcementConfig),
      make_entry("remove-all-encryption-enforcement-config", {"<bucket-name>"},
                 RemoveAllEncryptionEnforcementConfig),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

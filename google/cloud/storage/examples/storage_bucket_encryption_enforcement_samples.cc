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
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/getenv.h"
#include <iostream>
#include <thread>
#include <vector>

namespace {

void GetBucketEncryptionEnforcementConfig(
    google::cloud::storage::Client client,
    std::vector<std::string> const& argv) {
  //! [get bucket encryption enforcement config] [START
  //! storage_get_bucket_encryption_enforcement_config]
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
      if (config.restriction_mode.empty())
        return std::string("NOT SET (Default)");
      return "Mode: " + config.restriction_mode + ", Effective Time: " +
             google::cloud::internal::FormatRfc3339(config.effective_time);
    };

    std::cout << "  GMEK Enforcement: "
              << format_config(
                     encryption.google_managed_encryption_enforcement_config)
              << "\n"
              << "  CMEK Enforcement: "
              << format_config(
                     encryption.customer_managed_encryption_enforcement_config)
              << "\n"
              << "  CSEK Enforcement: "
              << format_config(
                     encryption.customer_supplied_encryption_enforcement_config)
              << "\n";
  }
  //! [get bucket encryption enforcement config] [END
  //! storage_get_bucket_encryption_enforcement_config]
  (std::move(client), argv.at(0));
}

void SetBucketEncryptionEnforcementConfig(
    google::cloud::storage::Client client,
    std::vector<std::string> const& argv) {
  //! [set bucket encryption enforcement config] [START
  //! storage_set_bucket_encryption_enforcement_config]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& project_id,
     std::string const& bucket_name) {
    auto create_bucket = [&](std::string const& name,
                             gcs::BucketEncryption encryption) {
      StatusOr<gcs::BucketMetadata> bucket = client.CreateBucketForProject(
          name, project_id, gcs::BucketMetadata().set_encryption(encryption));
      if (!bucket) throw std::move(bucket).status();
      return bucket;
    };

    // Example 1: Enforce GMEK Only
    gcs::BucketEncryption gmek_encryption;
    gmek_encryption.google_managed_encryption_enforcement_config
        .restriction_mode = "NotRestricted";
    gmek_encryption.customer_managed_encryption_enforcement_config
        .restriction_mode = "FullyRestricted";
    gmek_encryption.customer_supplied_encryption_enforcement_config
        .restriction_mode = "FullyRestricted";
    std::cout << "Bucket "
              << create_bucket("g-" + bucket_name, gmek_encryption)->name()
              << " created with GMEK-only enforcement policy.\n";

    // In GCS, a single project cannot create or delete buckets more often than
    // once every two seconds. We pause to avoid rate limiting.
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Example 2: Enforce CMEK Only
    gcs::BucketEncryption cmek_encryption;
    cmek_encryption.google_managed_encryption_enforcement_config
        .restriction_mode = "FullyRestricted";
    cmek_encryption.customer_managed_encryption_enforcement_config
        .restriction_mode = "NotRestricted";
    cmek_encryption.customer_supplied_encryption_enforcement_config
        .restriction_mode = "FullyRestricted";
    std::cout << "Bucket "
              << create_bucket("c-" + bucket_name, cmek_encryption)->name()
              << " created with CMEK-only enforcement policy.\n";

    // In GCS, a single project cannot create or delete buckets more often than
    // once every two seconds. We pause to avoid rate limiting.
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Example 3: Restrict CSEK (Ransomware Protection)
    gcs::BucketEncryption csek_encryption;
    csek_encryption.customer_supplied_encryption_enforcement_config
        .restriction_mode = "FullyRestricted";
    std::cout << "Bucket "
              << create_bucket("rc-" + bucket_name, csek_encryption)->name()
              << " created with a policy to restrict CSEK.\n";
  }
  //! [set bucket encryption enforcement config] [END
  //! storage_set_bucket_encryption_enforcement_config]
  (std::move(client), argv.at(0), argv.at(1));
}

void UpdateBucketEncryptionEnforcementConfig(
    google::cloud::storage::Client client,
    std::vector<std::string> const& argv) {
  //! [update bucket encryption enforcement config] [START
  //! storage_update_bucket_encryption_enforcement_config]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);
    if (!original) throw std::move(original).status();

    gcs::BucketMetadata updated_metadata = *original;
    gcs::BucketEncryption encryption;
    if (original->has_encryption()) {
      encryption = original->encryption();
    }

    // 1. Update a specific type (e.g., change GMEK to FullyRestricted)
    encryption.google_managed_encryption_enforcement_config.restriction_mode =
        "FullyRestricted";
    // 2. Remove a specific type (e.g., remove CMEK enforcement)
    encryption.customer_managed_encryption_enforcement_config.restriction_mode =
        "NotRestricted";
    encryption.customer_supplied_encryption_enforcement_config
        .restriction_mode = "FullyRestricted";

    updated_metadata.set_encryption(encryption);

    StatusOr<gcs::BucketMetadata> updated =
        client.PatchBucket(bucket_name, *original, updated_metadata);
    if (!updated) throw std::move(updated).status();

    std::cout << "Encryption enforcement policy updated for bucket "
              << updated->name() << "\n"
              << "GMEK is now fully restricted, and CMEK enforcement has been "
                 "removed.\n";
  }
  //! [update bucket encryption enforcement config] [END
  //! storage_update_bucket_encryption_enforcement_config]
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
  // Shorten bucket name to allow for prefixes without exceeding max length
  auto const bucket_name =
      examples::MakeRandomBucketName(generator).substr(0, 50);
  auto client = gcs::Client(examples::CreateBucketOptions());

  auto constexpr kBucketPeriod = std::chrono::seconds(2);

  // Clean up any potentially leaked buckets from a previous run before creating
  // them.
  (void)examples::RemoveBucketAndContents(client, "g-" + bucket_name);
  if (!examples::UsingEmulator()) std::this_thread::sleep_for(kBucketPeriod);
  (void)examples::RemoveBucketAndContents(client, "c-" + bucket_name);
  if (!examples::UsingEmulator()) std::this_thread::sleep_for(kBucketPeriod);
  (void)examples::RemoveBucketAndContents(client, "rc-" + bucket_name);
  if (!examples::UsingEmulator()) std::this_thread::sleep_for(kBucketPeriod);

  std::cout << "\nRunning the SetBucketEncryptionEnforcementConfig() example"
            << std::endl;
  SetBucketEncryptionEnforcementConfig(client, {project_id, bucket_name});

  std::cout
      << "\nRunning the GetBucketEncryptionEnforcementConfig() example [1]"
      << std::endl;
  GetBucketEncryptionEnforcementConfig(client, {"c-" + bucket_name});

  std::cout << "\nRunning the UpdateBucketEncryptionEnforcementConfig() example"
            << std::endl;
  UpdateBucketEncryptionEnforcementConfig(client, {"c-" + bucket_name});

  std::cout
      << "\nRunning the GetBucketEncryptionEnforcementConfig() example [2]"
      << std::endl;
  GetBucketEncryptionEnforcementConfig(client, {"c-" + bucket_name});

  // In GCS a single project cannot create or delete buckets more often than
  // once every two seconds. We will pause until that time before deleting the
  // buckets.
  auto pause = std::chrono::steady_clock::now() + kBucketPeriod;
  if (!examples::UsingEmulator()) std::this_thread::sleep_until(pause);

  (void)examples::RemoveBucketAndContents(client, "g-" + bucket_name);
  if (!examples::UsingEmulator()) std::this_thread::sleep_for(kBucketPeriod);
  (void)examples::RemoveBucketAndContents(client, "c-" + bucket_name);
  if (!examples::UsingEmulator()) std::this_thread::sleep_for(kBucketPeriod);
  (void)examples::RemoveBucketAndContents(client, "rc-" + bucket_name);
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
                 {"<project-id>", "<bucket-name>"},
                 SetBucketEncryptionEnforcementConfig),
      make_entry("update-bucket-encryption-enforcement-config",
                 {"<bucket-name>"}, UpdateBucketEncryptionEnforcementConfig),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

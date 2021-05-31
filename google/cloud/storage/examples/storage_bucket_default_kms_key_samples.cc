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

void AddBucketDefaultKmsKey(google::cloud::storage::Client client,
                            std::vector<std::string> const& argv) {
  //! [add bucket kms key] [START storage_set_bucket_default_kms_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& key_name) {
    StatusOr<gcs::BucketMetadata> updated = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().SetEncryption(
                         gcs::BucketEncryption{key_name}));
    if (!updated) throw std::runtime_error(updated.status().message());

    if (!updated->has_encryption()) {
      std::cerr << "The change to set the encryption attribute on bucket "
                << updated->name()
                << " was successful, but the encryption is not set."
                << "This is unexpected, maybe a concurrent change?\n";
      return;
    }

    std::cout << "Successfully set default KMS key on bucket "
              << updated->name() << " to "
              << updated->encryption().default_kms_key_name << "."
              << "\nFull metadata: " << *updated << "\n";
  }
  //! [add bucket kms key] [END storage_set_bucket_default_kms_key]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetBucketDefaultKmsKey(google::cloud::storage::Client client,
                            std::vector<std::string> const& argv) {
  //! [get bucket default kms key] [START storage_bucket_get_default_kms_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> metadata =
        client.GetBucketMetadata(bucket_name);
    if (!metadata) throw std::runtime_error(metadata.status().message());

    if (!metadata->has_encryption()) {
      std::cout << "The bucket " << metadata->name()
                << " does not have a default KMS key set.\n";
      return;
    }

    std::cout << "The default KMS key for bucket " << metadata->name()
              << " is: " << metadata->encryption().default_kms_key_name << "\n";
  }
  //! [get bucket default kms key] [END storage_bucket_get_default_kms_key]
  (std::move(client), argv.at(0));
}

void RemoveBucketDefaultKmsKey(google::cloud::storage::Client client,
                               std::vector<std::string> const& argv) {
  //! [remove bucket default kms key]
  // [START storage_bucket_delete_default_kms_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> updated = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().ResetEncryption());
    if (!updated) throw std::runtime_error(updated.status().message());

    std::cout << "Successfully removed default KMS key on bucket "
              << updated->name() << "\n";
  }
  // [END storage_bucket_delete_default_kms_key]
  //! [remove bucket default kms key]
  (std::move(client), argv.at(0));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
      "GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto const cmek_key =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_STORAGE_TEST_CMEK_KEY")
          .value();
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

  std::cout << "\nRunning the GetBucketDefaultKmsKey() example [1]"
            << std::endl;
  GetBucketDefaultKmsKey(client, {bucket_name});

  std::cout << "\nRunning the AddBucketDefaultKmsKey() example" << std::endl;
  AddBucketDefaultKmsKey(client, {bucket_name, cmek_key});

  std::cout << "\nRunning the GetBucketDefaultKmsKey() example [2]"
            << std::endl;
  GetBucketDefaultKmsKey(client, {bucket_name});

  std::cout << "\nRunning the RemoveBucketDefaultKmsKey() example" << std::endl;
  RemoveBucketDefaultKmsKey(client, {bucket_name});

  if (!examples::UsingEmulator()) std::this_thread::sleep_until(pause);
  (void)examples::RemoveBucketAndContents(client, bucket_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  auto make_entry = [](std::string const& name,
                       std::vector<std::string> arg_names,
                       examples::ClientCommand const& cmd) {
    arg_names.insert(arg_names.begin(), "<bucket-name>");
    return examples::CreateCommandEntry(name, std::move(arg_names), cmd);
  };
  examples::Example example({
      make_entry("add-bucket-default-kms-key", {"<kms-key-name>"},
                 AddBucketDefaultKmsKey),
      make_entry("get-bucket-default-kms-key", {}, GetBucketDefaultKmsKey),
      make_entry("remove-bucket-default-kms-key", {},
                 RemoveBucketDefaultKmsKey),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

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

void GetBucketLifecycleManagement(google::cloud::storage::Client client,
                                  std::vector<std::string> const& argv) {
  // [START storage_view_lifecycle_management_configuration]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> updated_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!updated_metadata) {
      throw std::runtime_error(updated_metadata.status().message());
    }

    if (!updated_metadata->has_lifecycle() ||
        updated_metadata->lifecycle().rule.empty()) {
      std::cout << "Bucket lifecycle management is not enabled for bucket "
                << updated_metadata->name() << ".\n";
      return;
    }
    std::cout << "Bucket lifecycle management is enabled for bucket "
              << updated_metadata->name() << ".\n";
    std::cout << "The bucket lifecycle rules are";
    for (auto const& kv : updated_metadata->lifecycle().rule) {
      std::cout << "\n " << kv.condition() << ", " << kv.action();
    }
    std::cout << "\n";
  }
  // [END storage_view_lifecycle_management_configuration]
  (std::move(client), argv.at(0));
}

void EnableBucketLifecycleManagement(google::cloud::storage::Client client,
                                     std::vector<std::string> const& argv) {
  //! [enable_bucket_lifecycle_management]
  // [START storage_enable_bucket_lifecycle_management]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    gcs::BucketLifecycle bucket_lifecycle_rules = gcs::BucketLifecycle{
        {gcs::LifecycleRule(gcs::LifecycleRule::ConditionConjunction(
                                gcs::LifecycleRule::MaxAge(30),
                                gcs::LifecycleRule::IsLive(true)),
                            gcs::LifecycleRule::Delete())}};

    StatusOr<gcs::BucketMetadata> updated_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetLifecycle(bucket_lifecycle_rules));

    if (!updated_metadata) {
      throw std::runtime_error(updated_metadata.status().message());
    }

    if (!updated_metadata->has_lifecycle() ||
        updated_metadata->lifecycle().rule.empty()) {
      std::cout << "Bucket lifecycle management is not enabled for bucket "
                << updated_metadata->name() << ".\n";
      return;
    }
    std::cout << "Successfully enabled bucket lifecycle management for bucket "
              << updated_metadata->name() << ".\n";
    std::cout << "The bucket lifecycle rules are";
    for (auto const& kv : updated_metadata->lifecycle().rule) {
      std::cout << "\n " << kv.condition() << ", " << kv.action();
    }
    std::cout << "\n";
  }
  // [END storage_enable_bucket_lifecycle_management]
  //! [storage_enable_bucket_lifecycle_management]
  (std::move(client), argv.at(0));
}

void DisableBucketLifecycleManagement(google::cloud::storage::Client client,
                                      std::vector<std::string> const& argv) {
  //! [disable_bucket_lifecycle_management]
  // [START storage_disable_bucket_lifecycle_management]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> updated_metadata = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().ResetLifecycle());

    if (!updated_metadata) {
      throw std::runtime_error(updated_metadata.status().message());
    }

    std::cout << "Successfully disabled bucket lifecycle management for bucket "
              << updated_metadata->name() << ".\n";
  }
  // [END storage_disable_bucket_lifecycle_management]
  //! [storage_disable_bucket_lifecycle_management]
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
                                      gcs::BucketMetadata());
  // In GCS a single project cannot create or delete buckets more often than
  // once every two seconds. We will pause until that time before deleting the
  // bucket.
  auto pause = std::chrono::steady_clock::now() + std::chrono::seconds(2);

  std::cout << "\nRunning EnableBucketLifecycleManagement() example"
            << std::endl;
  EnableBucketLifecycleManagement(client, {bucket_name});

  std::cout << "\nRunning GetBucketLifecycleManagement() example" << std::endl;
  GetBucketLifecycleManagement(client, {bucket_name});

  std::cout << "\nRunning DisableBucketLifecycleManagement() example"
            << std::endl;
  DisableBucketLifecycleManagement(client, {bucket_name});

  std::cout << "\nRunning GetBucketLifecycleManagement() example" << std::endl;
  GetBucketLifecycleManagement(client, {bucket_name});

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
      make_entry("get-bucket-lifecycle-management", {},
                 GetBucketLifecycleManagement),
      make_entry("enable-bucket-lifecycle-management", {},
                 EnableBucketLifecycleManagement),
      make_entry("disable-bucket-lifecycle-management", {},
                 DisableBucketLifecycleManagement),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

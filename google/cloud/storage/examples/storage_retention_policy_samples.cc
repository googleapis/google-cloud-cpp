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

void GetRetentionPolicy(google::cloud::storage::Client client,
                        std::vector<std::string> const& argv) {
  //! [get retention policy]
  // [START storage_get_retention_policy]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    if (!bucket_metadata->has_retention_policy()) {
      std::cout << "The bucket " << bucket_metadata->name()
                << " does not have a retention policy set.\n";
      return;
    }

    std::cout << "The bucket " << bucket_metadata->name()
              << " retention policy is set to "
              << bucket_metadata->retention_policy() << "\n";
  }
  // [END storage_get_retention_policy]
  //! [get retention policy]
  (std::move(client), argv.at(0));
}

void SetRetentionPolicy(google::cloud::storage::Client client,
                        std::vector<std::string> const& argv) {
  //! [set retention policy]
  // [START storage_set_retention_policy]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::chrono::seconds period) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) throw std::runtime_error(original.status().message());
    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetRetentionPolicy(period),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      throw std::runtime_error(patched_metadata.status().message());
    }

    if (!patched_metadata->has_retention_policy()) {
      std::cout << "The bucket " << patched_metadata->name()
                << " does not have a retention policy set.\n";
      return;
    }

    std::cout << "The bucket " << patched_metadata->name()
              << " retention policy is set to "
              << patched_metadata->retention_policy() << "\n";
  }
  // [END storage_set_retention_policy]
  //! [set retention policy]
  (std::move(client), argv.at(0), std::chrono::seconds(std::stol(argv.at(1))));
}

void RemoveRetentionPolicy(google::cloud::storage::Client client,
                           std::vector<std::string> const& argv) {
  //! [remove retention policy]
  // [START storage_remove_retention_policy]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) throw std::runtime_error(original.status().message());
    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().ResetRetentionPolicy(),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      throw std::runtime_error(patched_metadata.status().message());
    }

    if (!patched_metadata->has_retention_policy()) {
      std::cout << "The bucket " << patched_metadata->name()
                << " does not have a retention policy set.\n";
      return;
    }

    std::cout << "The bucket " << patched_metadata->name()
              << " retention policy is set to "
              << patched_metadata->retention_policy()
              << ". This is unexpected, maybe a concurrent change by another"
              << " application?\n";
  }
  // [END storage_remove_retention_policy]
  //! [remove retention policy]
  (std::move(client), argv.at(0));
}

void LockRetentionPolicy(google::cloud::storage::Client client,
                         std::vector<std::string> const& argv) {
  //! [lock retention policy]
  // [START storage_lock_retention_policy]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) throw std::runtime_error(original.status().message());
    StatusOr<gcs::BucketMetadata> updated_metadata =
        client.LockBucketRetentionPolicy(bucket_name,
                                         original->metageneration());

    if (!updated_metadata) {
      throw std::runtime_error(updated_metadata.status().message());
    }

    if (!updated_metadata->has_retention_policy()) {
      std::cerr << "The bucket " << updated_metadata->name()
                << " does not have a retention policy, even though the"
                << " operation to set it was successful.\n"
                << "This is unexpected, and may indicate that another"
                << " application has modified the bucket concurrently.\n";
      return;
    }

    std::cout << "Retention policy successfully locked for bucket "
              << updated_metadata->name() << "\nNew retention policy is: "
              << updated_metadata->retention_policy()
              << "\nFull metadata: " << *updated_metadata << "\n";
  }
  // [END storage_lock_retention_policy]
  //! [lock retention policy]
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
  auto const bucket_name =
      examples::MakeRandomBucketName(generator, "cloud-cpp-test-examples-");
  auto client = gcs::Client::CreateDefaultClient().value();

  std::cout << "\nCreating bucket to run the examples" << std::endl;
  (void)client.CreateBucketForProject(bucket_name, project_id,
                                      gcs::BucketMetadata{});
  // In GCS a single project cannot create or delete buckets more often than
  // once every two seconds. We will pause until that time before deleting the
  // bucket.
  auto pause = std::chrono::steady_clock::now() + std::chrono::seconds(2);

  std::cout << "\nRunning GetRetentionPolicy() example" << std::endl;
  GetRetentionPolicy(client, {bucket_name});

  std::cout << "\nRunning SetRetentionPolicy() example" << std::endl;
  SetRetentionPolicy(client, {bucket_name, "30"});

  std::cout << "\nRunning RemoveRetentionPolicy() example" << std::endl;
  RemoveRetentionPolicy(client, {bucket_name});

  std::cout << "\nRunning SetRetentionPolicy() example" << std::endl;
  SetRetentionPolicy(client, {bucket_name, "30"});

  std::cout << "\nRunning LockRetentionPolicy() example" << std::endl;
  LockRetentionPolicy(client, {bucket_name});

  std::cout << "\nCleaning up" << std::endl;
  if (!examples::UsingTestbench()) std::this_thread::sleep_until(pause);
  (void)client.DeleteBucket(bucket_name);
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
      make_entry("get-retention-policy", {}, GetRetentionPolicy),
      make_entry("set-retention-policy", {"<period>"}, SetRetentionPolicy),
      make_entry("remove-retention-policy", {}, RemoveRetentionPolicy),
      make_entry("lock-retention-policy", {}, LockRetentionPolicy),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

// Copyright 2024 Google LLC
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
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

void CreateBucketWithSoftDelete(google::cloud::storage::Client client,
                                std::vector<std::string> const& argv) {
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& project_id) {
    auto bucket = client.CreateBucket(
        bucket_name,
        gcs::BucketMetadata{}.set_soft_delete_policy(
            std::chrono::seconds(30 * std::chrono::hours(24))),
        gcs::OverrideDefaultProject(project_id));
    if (!bucket) throw std::move(bucket).status();

    if (!bucket->has_soft_delete_policy()) {
      throw std::runtime_error("missing soft-delete policy in new bucket");
    }
    std::cout << "Successfully created bucket " << bucket_name
              << " with soft-delete policy: " << bucket->soft_delete_policy()
              << "\n";
  }(std::move(client), argv.at(0), argv.at(1));
}

void SetBucketSoftDeletePolicy(google::cloud::storage::Client client,
                               std::vector<std::string> const& argv) {
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    auto bucket = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().SetSoftDeletePolicy(
                         std::chrono::seconds(45 * std::chrono::hours(24))));
    if (!bucket) throw std::move(bucket).status();

    if (!bucket->has_soft_delete_policy()) {
      throw std::runtime_error("missing soft-delete policy in updated bucket");
    }
    std::cout << "Successfully updated bucket " << bucket_name
              << " the updated soft-delete policy is "
              << bucket->soft_delete_policy() << "\n";
  }(std::move(client), argv.at(0));
}

void ResetBucketSoftDeletePolicy(google::cloud::storage::Client client,
                                 std::vector<std::string> const& argv) {
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    auto bucket = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().ResetSoftDeletePolicy());
    if (!bucket) throw std::move(bucket).status();

    if (!bucket->has_soft_delete_policy()) {
      std::cout << "Successfully reset soft-delete policy on bucket "
                << bucket_name << "\n";
      return;
    }
    std::cout << "Updated bucket " << bucket_name
              << " still has a soft-delete policy: "
              << bucket->soft_delete_policy() << "\n";
  }(std::move(client), argv.at(0));
}

void GetBucketSoftDeletePolicy(google::cloud::storage::Client client,
                               std::vector<std::string> const& argv) {
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    auto bucket = client.GetBucketMetadata(bucket_name);
    if (!bucket) throw std::move(bucket).status();

    if (!bucket->has_soft_delete_policy()) {
      std::cout << "Bucket " << bucket->name()
                << " does not have the soft-delete policy enabled\n";
      return;
    }
    std::cout << "Bucket " << bucket->name()
              << " has a soft-delete policy set: "
              << bucket->soft_delete_policy() << "\n";
  }(std::move(client), argv.at(0));
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

  std::cout << "Running the CreateBucketWithSoftDelete() example" << std::endl;
  CreateBucketWithSoftDelete(client, {bucket_name, project_id});

  // In GCS a single project cannot create or delete buckets more often than
  // once every two seconds. We will pause until that time before deleting the
  // bucket.
  auto pause = std::chrono::steady_clock::now() + std::chrono::seconds(2);

  std::cout << "\nRunning the GetBucketSoftDeletePolicy() example" << std::endl;
  GetBucketSoftDeletePolicy(client, {bucket_name});

  std::cout << "\nRunning the SetBucketSoftDeletePolicy() example" << std::endl;
  SetBucketSoftDeletePolicy(client, {bucket_name});

  std::cout << "\nRunning the ResetBucketSoftDeletePolicy() example"
            << std::endl;
  ResetBucketSoftDeletePolicy(client, {bucket_name});

  std::cout << "\nRunning the GetBucketSoftDeletePolicy() example [2]"
            << std::endl;
  GetBucketSoftDeletePolicy(client, {bucket_name});

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
      make_entry("create-bucket-with-soft-delete", {"<project-id>"},
                 CreateBucketWithSoftDelete),
      make_entry("get-bucket-soft-delete", {}, GetBucketSoftDeletePolicy),
      make_entry("set-bucket-soft-delete-policy", {},
                 SetBucketSoftDeletePolicy),
      make_entry("reset-bucket-soft-delete-policy", {},
                 ResetBucketSoftDeletePolicy),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

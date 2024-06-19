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
#include <thread>

namespace {

void CreateBucketWithObjectRetention(google::cloud::storage::Client client,
                                     std::vector<std::string> const& argv) {
  //! [START storage_create_bucket_with_object_retention]
  //! [create-bucket-with-object-retention]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& project_id) {
    auto bucket = client.CreateBucket(bucket_name, gcs::BucketMetadata{},
                                      gcs::EnableObjectRetention(true),
                                      gcs::OverrideDefaultProject(project_id));
    if (!bucket) throw std::move(bucket).status();

    if (!bucket->has_object_retention()) {
      throw std::runtime_error("missing object retention in new bucket");
    }
    std::cout << "Successfully created bucket " << bucket_name
              << " with object retention: " << bucket->object_retention()
              << "\n";
  }
  //! [create-bucket-with-object-retention]
  //! [END storage_create_bucket_with_object_retention]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetBucketObjectRetention(google::cloud::storage::Client client,
                              std::vector<std::string> const& argv) {
  //! [get-bucket-object-retention]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    auto bucket = client.GetBucketMetadata(bucket_name);
    if (!bucket) throw std::move(bucket).status();

    if (!bucket->has_object_retention()) {
      std::cout << "Bucket " << bucket->name()
                << " does not have object retention enabled\n";
      return;
    }
    std::cout << "Bucket " << bucket->name()
              << " has a object retention enabled: "
              << bucket->object_retention() << "\n";
  }
  //! [get-bucket-object-retention]
  (std::move(client), argv.at(0));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  if (examples::UsingEmulator()) return;
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const bucket_name = examples::MakeRandomBucketName(generator);

  auto client = gcs::Client();

  std::cout << "Running the CreateBucketWithSoftDelete() example" << std::endl;
  CreateBucketWithObjectRetention(client, {bucket_name, project_id});

  // In GCS a single project cannot create or delete buckets more often than
  // once every two seconds. We will pause until that time before deleting the
  // bucket.
  auto pause = std::chrono::steady_clock::now() + std::chrono::seconds(2);

  std::cout << "\nRunning the GetBucketObjectRetention() example" << std::endl;
  GetBucketObjectRetention(client, {bucket_name});

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
      make_entry("create-bucket-with-object-retention", {"<project-id>"},
                 CreateBucketWithObjectRetention),
      make_entry("get-bucket-object-retention", {}, GetBucketObjectRetention),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

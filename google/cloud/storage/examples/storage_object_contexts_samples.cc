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
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

void SetObjectContexts(google::cloud::storage::Client client,
                       std::vector<std::string> const& argv) {
  //! [START storage_set_object_contexts] [set-object-contexts]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    auto original = client.GetObjectMetadata(bucket_name, object_name);
    if (!original) throw std::move(original).status();

    // `SetContext()` inserts or updates a single key, and `ResetContext()`
    // removes a single key. Keys not mentioned in the patch are unchanged.
    // The metageneration precondition avoids overwriting concurrent changes.
    auto updated = client.PatchObject(
        bucket_name, object_name,
        gcs::ObjectMetadataPatchBuilder()
            .SetContext("key1", "newValue1")
            .ResetContext("key2")
            .SetContext("key3", "value3"),
        gcs::IfMetagenerationMatch(original->metageneration()));
    if (!updated) throw std::move(updated).status();

    std::cout << "Updated contexts for " << updated->name() << ":\n";
    if (!updated->has_contexts()) return;
    for (auto const& [key, payload] : updated->contexts().custom()) {
      std::cout << key << " = " << payload.value << "\n";
    }
  }
  //! [END storage_set_object_contexts] [set-object-contexts]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetObjectContexts(google::cloud::storage::Client client,
                       std::vector<std::string> const& argv) {
  //! [START storage_get_object_contexts] [get-object-contexts]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    auto metadata = client.GetObjectMetadata(bucket_name, object_name);
    if (!metadata) throw std::move(metadata).status();

    if (!metadata->has_contexts() || metadata->contexts().custom().empty()) {
      std::cout << "No contexts found for " << metadata->name() << "\n";
      return;
    }
    std::cout << "Contexts for " << metadata->name() << ":\n";
    for (auto const& [key, payload] : metadata->contexts().custom()) {
      std::cout << key << " = " << payload.value << "\n";
    }
  }
  //! [END storage_get_object_contexts] [get-object-contexts]
  (std::move(client), argv.at(0), argv.at(1));
}

void DeleteObjectContexts(google::cloud::storage::Client client,
                          std::vector<std::string> const& argv) {
  //! [START storage_delete_object_contexts] [delete-object-contexts]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    auto original = client.GetObjectMetadata(bucket_name, object_name);
    if (!original) throw std::move(original).status();

    // `ResetContexts()` removes all the custom contexts in the object.
    auto updated = client.PatchObject(
        bucket_name, object_name,
        gcs::ObjectMetadataPatchBuilder().ResetContexts(),
        gcs::IfMetagenerationMatch(original->metageneration()));
    if (!updated) throw std::move(updated).status();

    std::cout << "Deleted all contexts for " << updated->name() << "\n";
  }
  //! [END storage_delete_object_contexts] [delete-object-contexts]
  (std::move(client), argv.at(0), argv.at(1));
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
  auto client = gcs::Client();

  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());

  auto const bucket_name = examples::MakeRandomBucketName(generator);
  std::cout << "\nCreating bucket to run the example (" << bucket_name << ")"
            << std::endl;
  (void)client
      .CreateBucket(bucket_name, gcs::BucketMetadata{},
                    gcs::OverrideDefaultProject(project_id),
                    examples::CreateBucketOptions())
      .value();

  auto const object_name = examples::MakeRandomObjectName(generator, "object-");
  std::cout << "\nInserting object" << std::endl;
  (void)client
      .InsertObject(bucket_name, object_name,
                    "The quick brown fox jumps over the lazy dog",
                    gcs::IfGenerationMatch(0))
      .value();

  std::cout << "\nRunning GetObjectContexts() example [1]" << std::endl;
  GetObjectContexts(client, {bucket_name, object_name});

  std::cout << "\nRunning SetObjectContexts() example" << std::endl;
  SetObjectContexts(client, {bucket_name, object_name});

  std::cout << "\nRunning GetObjectContexts() example [2]" << std::endl;
  GetObjectContexts(client, {bucket_name, object_name});

  std::cout << "\nRunning DeleteObjectContexts() example" << std::endl;
  DeleteObjectContexts(client, {bucket_name, object_name});

  std::cout << "\nRunning GetObjectContexts() example [3]" << std::endl;
  GetObjectContexts(client, {bucket_name, object_name});

  std::cout << "\nCleaning up" << std::endl;
  (void)examples::RemoveBucketAndContents(client, bucket_name);
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  auto make_entry = [](std::string const& name,
                       examples::ClientCommand const& cmd) {
    return examples::CreateCommandEntry(
        name, {"<bucket-name>", "<object-name>"}, cmd);
  };
  examples::Example example({
      make_entry("set-object-contexts", SetObjectContexts),
      make_entry("get-object-contexts", GetObjectContexts),
      make_entry("delete-object-contexts", DeleteObjectContexts),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

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
#include <iostream>

namespace {

void InsertObjectWithRetention(google::cloud::storage::Client client,
                               std::vector<std::string> const& argv) {
  //! [insert-object-with-retention]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    auto constexpr kData = "The quick brown fox jumps over the lazy dog";
    auto const until =
        std::chrono::system_clock::now() + std::chrono::hours(48);
    auto insert = client.InsertObject(
        bucket_name, object_name, kData,
        gcs::WithObjectMetadata(gcs::ObjectMetadata{}.set_retention(
            gcs::ObjectRetention{gcs::ObjectRetentionUnlocked(), until})));
    if (!insert) throw std::move(insert).status();

    std::cout << "Object successfully created: " << *insert << "\n";
  }
  //! [insert-object-with-retention]
  (std::move(client), argv.at(0), argv.at(1));
}

void WriteObjectWithRetention(google::cloud::storage::Client client,
                              std::vector<std::string> const& argv) {
  //! [write-object-with-retention]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    auto constexpr kData = "The quick brown fox jumps over the lazy dog";
    auto const until =
        std::chrono::system_clock::now() + std::chrono::hours(48);
    auto os = client.WriteObject(
        bucket_name, object_name,
        gcs::WithObjectMetadata(gcs::ObjectMetadata{}.set_retention(
            gcs::ObjectRetention{gcs::ObjectRetentionUnlocked(), until})));
    os << kData;
    os.Close();
    auto insert = os.metadata();
    if (!insert) throw std::move(insert).status();

    std::cout << "Object successfully created: " << *insert << "\n";
  }
  //! [write-object-with-retention]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetObjectRetention(google::cloud::storage::Client client,
                        std::vector<std::string> const& argv) {
  //! [get-object-retention]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    auto metadata = client.GetObjectMetadata(bucket_name, object_name);
    if (!metadata) throw std::move(metadata).status();

    if (!metadata->has_retention()) {
      std::cout << "The object " << metadata->name() << " in bucket "
                << metadata->bucket()
                << " does not have a retention configuration\n";
      return;
    }
    std::cout << "The retention configuration for object " << metadata->name()
              << " in bucket " << metadata->bucket() << " is "
              << metadata->retention() << "\n";
  }
  //! [get-object-retention]
  (std::move(client), argv.at(0), argv.at(1));
}

void PatchObjectRetention(google::cloud::storage::Client client,
                          std::vector<std::string> const& argv) {
  //! [START storage_set_object_retention_policy] [patch-object-retention]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    auto original = client.GetObjectMetadata(bucket_name, object_name);
    if (!original) throw std::move(original).status();

    auto const until =
        std::chrono::system_clock::now() + std::chrono::hours(24);
    auto updated = client.PatchObject(
        bucket_name, object_name,
        gcs::ObjectMetadataPatchBuilder().SetRetention(
            gcs::ObjectRetention{gcs::ObjectRetentionUnlocked(), until}),
        gcs::OverrideUnlockedRetention(true),
        gcs::IfMetagenerationMatch(original->metageneration()));
    if (!updated) throw std::move(updated).status();

    std::cout << "Successfully updated object retention configuration: "
              << *updated << "\n";
  }
  //! [END storage_set_object_retention_policy] [patch-object-retention]
  (std::move(client), argv.at(0), argv.at(1));
}

void ResetObjectRetention(google::cloud::storage::Client client,
                          std::vector<std::string> const& argv) {
  //! [reset-object-retention]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    auto original = client.GetObjectMetadata(bucket_name, object_name);
    if (!original) throw std::move(original).status();

    auto updated = client.PatchObject(
        bucket_name, object_name,
        gcs::ObjectMetadataPatchBuilder().ResetRetention(),
        gcs::OverrideUnlockedRetention(true),
        gcs::IfMetagenerationMatch(original->metageneration()));
    if (!updated) throw std::move(updated).status();

    std::cout << "Successfully updated object retention configuration: "
              << *updated << "\n";
  }
  //! [reset-object-retention]
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
  auto bucket = client
                    .CreateBucket(bucket_name, gcs::BucketMetadata{},
                                  gcs::EnableObjectRetention(true),
                                  gcs::OverrideDefaultProject(project_id),
                                  examples::CreateBucketOptions())
                    .value();

  auto const name1 = examples::MakeRandomObjectName(generator, "object-");
  auto const name2 = examples::MakeRandomObjectName(generator, "object-");
  auto const name3 = examples::MakeRandomObjectName(generator, "object-");

  std::cout << "Running InsertObjectWithRetention() example" << std::endl;
  InsertObjectWithRetention(client, {bucket_name, name1});

  std::cout << "\nRunning GetObjectRetention() example [1]" << std::endl;
  GetObjectRetention(client, {bucket_name, name1});

  std::cout << "\nRunning WriteObjectWithRetention() example" << std::endl;
  WriteObjectWithRetention(client, {bucket_name, name2});

  std::cout << "\nRunning GetObjectRetention() example [2]" << std::endl;
  GetObjectRetention(client, {bucket_name, name2});

  std::cout << "\nRunning PatchObjectRetention() [1]" << std::endl;
  PatchObjectRetention(client, {bucket_name, name2});

  std::cout << "\nInserting object" << std::endl;
  auto const o1 =
      client
          .InsertObject(bucket_name, name3,
                        "The quick brown fox jumps over the lazy dog",
                        gcs::IfGenerationMatch(0))
          .value();

  std::cout << "\nRunning GetObjectRetention() example [3]" << std::endl;
  GetObjectRetention(client, {bucket_name, name3});

  std::cout << "\nRunning PatchObjectRetention() [2]" << std::endl;
  PatchObjectRetention(client, {bucket_name, name3});

  std::cout << "\nRunning ResetObjectRetention() [1]" << std::endl;
  ResetObjectRetention(client, {bucket_name, name1});

  std::cout << "\nRunning ResetObjectRetention() [2]" << std::endl;
  ResetObjectRetention(client, {bucket_name, name2});

  std::cout << "\nRunning ResetObjectRetention() [3]" << std::endl;
  ResetObjectRetention(client, {bucket_name, name3});

  std::cout << "\nCleaning up" << std::endl;
  for (auto const& name : {name1, name2, name3}) {
    std::cout << "GetObjectMetadata [" << name << "]" << std::endl;
    auto current = client.GetObjectMetadata(bucket_name, name);
    if (!current) continue;
    std::cout << "DeleteObject [" << name << "]" << std::endl;
    auto status = client.DeleteObject(current->bucket(), current->name(),
                                      gcs::Generation(current->generation()));
    if (!status.ok()) std::cout << "Status=" << status << "\n";
  }
  std::cout << "\nDeleteBucket" << std::endl;
  auto status = client.DeleteBucket(bucket_name);
  if (!status.ok()) std::cout << "Status=" << status << "\n";
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
      make_entry("insert-object-with-retention", InsertObjectWithRetention),
      make_entry("write-object-with-retention", WriteObjectWithRetention),
      make_entry("get-object-retention", GetObjectRetention),
      make_entry("patch-object-retention", PatchObjectRetention),
      make_entry("reset-object-retention", ResetObjectRetention),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

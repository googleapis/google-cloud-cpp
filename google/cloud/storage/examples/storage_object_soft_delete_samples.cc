// Copyright 2025 Google LLC
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
#include <vector>

namespace {

void ListSoftDeletedObjects(google::cloud::storage::Client client,
                            std::vector<std::string> const& argv) {
  //! [list-soft-deleted objects]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name) {
    std::cout << "Listing soft-deleted objects in bucket: " << bucket_name
              << "\n";
    for (auto const& object_metadata :
         client.ListObjects(bucket_name, gcs::Versions(true))) {
      if (!object_metadata) throw std::move(object_metadata).status();
      if (object_metadata->time_deleted() !=
          std::chrono::time_point<std::chrono::system_clock>{}) {
        std::cout << "Soft-deleted object: " << object_metadata->name() << "\n";
      }
    }
  }
  //! [list-soft-deleted objects]
  (std::move(client), argv.at(0));
}

std::vector<int64_t> ListSoftDeletedObjectVersions(
    google::cloud::storage::Client client,
    std::vector<std::string> const& argv) {
  //! [list-soft-deleted-object-versions]
  namespace gcs = ::google::cloud::storage;
  return [](gcs::Client client, std::string const& bucket_name,
            std::string const& object_name) {
    std::vector<int64_t> object_generations;
    for (auto const& object_metadata :
         client.ListObjects(bucket_name, gcs::Versions(true))) {
      if (!object_metadata) throw std::move(object_metadata).status();
      if (object_metadata->name() == object_name &&
          object_metadata->time_deleted() !=
              std::chrono::time_point<std::chrono::system_clock>{}) {
        std::cout << "Version" << (object_generations.size() + 1)
                  << " of the soft-deleted object " << object_name << ": "
                  << object_metadata->generation() << "\n";
        object_generations.push_back(object_metadata->generation());
      }
    }
    if (object_generations.empty()) {
      std::cout << "No soft-deleted object found with name: " << object_name
                << "\n";
    }
    return object_generations;
  }
  //! [list-soft-deleted-object-versions]
  (std::move(client), argv.at(0), argv.at(1));
}

void RestoreSoftDeletedObject(google::cloud::storage::Client client,
                              std::vector<std::string> const& argv) {
  //! [restore-soft-deleted-object]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::int64_t generation) {
    auto object_metadata = client.GetObjectMetadata(bucket_name, object_name);
    if (object_metadata.ok()) {
      std::cout << "Skipping restoration since object already exists: "
                << object_name << "\n";
      return;
    }

    auto restored_object =
        client.RewriteObject(bucket_name, object_name, bucket_name, object_name,
                             gcs::SourceGeneration(generation));
    auto metadata = restored_object.Result();
    if (!metadata.ok()) throw std::move(metadata).status();
    std::cout << "Object successfully restored: " << object_name
              << " (generation: " << generation << ")\n";
  }
  //! [restore-soft-deleted-object]
  (std::move(client), argv.at(0), argv.at(1), std::stoll(argv.at(2)));
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
  std::cout << "\nCreating bucket to run the example (bucket name: "
            << bucket_name << ")" << std::endl;
  auto client = gcs::Client();
  auto bucket =
      client
          .CreateBucket(
              bucket_name,
              gcs::BucketMetadata().set_versioning(gcs::BucketVersioning{true}),
              gcs::OverrideDefaultProject(project_id),
              examples::CreateBucketOptions())
          .value();
  auto object_name = examples::MakeRandomObjectName(generator, "object-");
  std::cout << "Inserting object: " << object_name << std::endl;
  client.InsertObject(bucket_name, object_name, "Test data for object");
  std::cout << "Deleting object: " << object_name << std::endl;
  client.DeleteObject(bucket_name, object_name);

  std::cout << "\nRunning the ListSoftDeletedObjects() example" << std::endl;
  ListSoftDeletedObjects(client, {bucket_name});

  std::cout << "\nRunning the ListSoftDeletedObjectVersions() example"
            << std::endl;
  auto object_generations =
      ListSoftDeletedObjectVersions(client, {bucket_name, object_name});

  std::cout << "\nRunning the RestoreSoftDeletedObject() example" << std::endl;
  RestoreSoftDeletedObject(client, {bucket_name, object_name,
                                    std::to_string(object_generations[0])});

  std::cout << "\nCleaning up" << std::endl;
  auto updated_bucket_metadata = client.UpdateBucket(
      bucket_name, gcs::BucketMetadata().set_soft_delete_policy(
                       gcs::BucketSoftDeletePolicy{std::chrono::seconds(0)}));
  if (!updated_bucket_metadata.ok()) {
    std::cout << "Bucket update failed with status: "
              << updated_bucket_metadata.status() << "\n";
  }
  auto obj_delete_status = client.DeleteObject(
      bucket_name, object_name, gcs::Generation(object_generations[0]));
  if (!obj_delete_status.ok()) {
    std::cout << "Soft-deleted object copy deletion failed with status: "
              << obj_delete_status << "\n";
  } else {
    std::cout << "Soft-deleted object copy deleted successfully.\n";
  }
  auto object_metadata = client.GetObjectMetadata(bucket_name, object_name);
  if (object_metadata) {
    auto obj_delete_status = client.DeleteObject(bucket_name, object_name);
    if (!obj_delete_status.ok()) {
      std::cout << "Live object deletion failed with status: "
                << obj_delete_status << "\n";
    } else {
      std::cout << "Live object deleted successfully.\n";
    }
  } else {
    std::cout << "No live object not found for cleanup.\n";
  }
  auto bucket_delete_status = client.DeleteBucket(bucket_name);
  if (!bucket_delete_status.ok()) {
    std::cout << "Bucket deletion failed with status: " << bucket_delete_status
              << "\n";
  } else {
    std::cout << "Bucket deleted successfully.\n";
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace examples = ::google::cloud::storage::examples;
  auto make_entry = [](std::string const& name,
                       std::vector<std::string> const& arg_names,
                       examples::ClientCommand const& cmd) {
    return examples::CreateCommandEntry(name, arg_names, cmd);
  };
  examples::Example example({
      make_entry("list-soft-deleted-objects", {"<bucket-name>"},
                 ListSoftDeletedObjects),
      make_entry("list-soft-deleted-object-versions",
                 {"<bucket-name>", "<object-name>"},
                 ListSoftDeletedObjectVersions),
      make_entry("restore-soft-deleted-object",
                 {"<bucket-name>", "<object-name>", "<generation>"},
                 RestoreSoftDeletedObject),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

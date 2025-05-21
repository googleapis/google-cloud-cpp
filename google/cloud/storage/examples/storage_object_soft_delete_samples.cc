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
  //! [storage_list_soft_deleted_objects]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name) {
    std::cout << "Listing soft-deleted objects in the bucket: " << bucket_name
              << "\n";
    int object_count = 0;
    for (auto&& object_metadata :
         client.ListObjects(bucket_name, gcs::SoftDeleted(true))) {
      if (!object_metadata) throw std::move(object_metadata).status();
      std::cout << "Soft-deleted object" << ++object_count << ": "
                << object_metadata->name() << "\n";
    }
  }
  //! [storage_list_soft_deleted_objects]
  (std::move(client), argv.at(0));
}

void ListSoftDeletedObjectVersions(google::cloud::storage::Client client,
                                   std::vector<std::string> const& argv) {
  //! [storage_list_soft_deleted_object_versions]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name) {
    int version_count = 0;
    for (auto&& object_metadata :
         client.ListObjects(bucket_name, gcs::SoftDeleted(true),
                            gcs::MatchGlob(object_name))) {
      if (!object_metadata) throw std::move(object_metadata).status();
      std::cout << "Version" << ++version_count
                << " of the soft-deleted object " << object_name << ": "
                << object_metadata->generation() << "\n";
    }
  }
  //! [storage_list_soft_deleted_object_versions]
  (std::move(client), argv.at(0), argv.at(1));
}

void RestoreSoftDeletedObject(google::cloud::storage::Client client,
                              std::vector<std::string> const& argv) {
  //! [storage_restore_object]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& object_name, std::int64_t generation) {
    // Restore will override an already existing live object
    // with the same name.
    auto object_metadata =
        client.RestoreObject(bucket_name, object_name, generation);
    if (!object_metadata.ok()) throw std::move(object_metadata).status();
    std::cout << "Object successfully restored: " << object_metadata->name()
              << " (generation: " << generation << ")\n";
  }
  //! [storage_restore_object]
  (std::move(client), argv.at(0), argv.at(1), std::stoll(argv.at(2)));
}

void RunAll(std::vector<std::string> const& argv) {
  namespace examples = ::google::cloud::storage::examples;
  namespace gcs = ::google::cloud::storage;

  if (!argv.empty()) throw examples::Usage{"auto"};
  std::cout << "\nSetup" << std::endl;
  examples::CheckEnvironmentVariablesAreSet({
      "GOOGLE_CLOUD_PROJECT",
  });
  auto const project_id =
      google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value();
  auto client = gcs::Client();
  auto generator = google::cloud::internal::DefaultPRNG(std::random_device{}());
  auto const bucket_name = examples::MakeRandomBucketName(generator);
  std::cout << "Creating a bucket having soft-delete enabled: " << bucket_name
            << std::endl;
  auto bucket = client.CreateBucket(
      bucket_name,
      gcs::BucketMetadata{}.set_soft_delete_policy(
          std::chrono::seconds(10 * std::chrono::hours(24))),
      gcs::OverrideDefaultProject(project_id));
  auto object_name = examples::MakeRandomObjectName(generator, "object-");
  std::cout << "Inserting an object: " << object_name << std::endl;
  auto insert_obj_metadata =
      client.InsertObject(bucket_name, object_name, "Test data for object");
  if (!insert_obj_metadata.ok()) throw std::move(insert_obj_metadata).status();
  std::cout << "Deleting the object: " << object_name << std::endl;
  auto obj_delete_status = client.DeleteObject(bucket_name, object_name);
  if (!obj_delete_status.ok()) throw std::move(obj_delete_status);

  std::cout << "\nRunning the ListSoftDeletedObjects() example" << std::endl;
  ListSoftDeletedObjects(client, {bucket_name});

  std::cout << "\nRunning the ListSoftDeletedObjectVersions() example"
            << std::endl;
  ListSoftDeletedObjectVersions(client, {bucket_name, object_name});

  auto objects = client.ListObjects(bucket_name, gcs::SoftDeleted(true),
                                    gcs::MatchGlob(object_name));
  auto object_metadata = objects.begin();
  if (!*object_metadata) throw std::move(*object_metadata).status();
  std::int64_t generation = (*object_metadata)->generation();

  std::cout << "\nRunning the RestoreSoftDeletedObject() example" << std::endl;
  RestoreSoftDeletedObject(
      client, {bucket_name, object_name, std::to_string(generation)});

  std::cout << "\nCleanup" << std::endl;
  auto object_delete_status = client.DeleteObject(bucket_name, object_name);
  if (!object_delete_status.ok()) throw std::move(object_delete_status);
  std::cout << "Object deleted successfully.\n";
  auto bucket_delete_status = client.DeleteBucket(bucket_name);
  if (!bucket_delete_status.ok()) throw std::move(bucket_delete_status);
  std::cout << "Bucket deleted successfully.\n";
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

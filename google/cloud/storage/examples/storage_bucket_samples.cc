// Copyright 2018 Google LLC
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
#include <functional>
#include <iostream>
#include <map>
#include <sstream>

namespace {
struct Usage {
  std::string msg;
};

char const* ConsumeArg(int& argc, char* argv[]) {
  if (argc < 2) {
    return nullptr;
  }
  char const* result = argv[1];
  std::copy(argv + 2, argv + argc, argv + 1);
  argc--;
  return result;
}

std::string command_usage;

void PrintUsage(int argc, char* argv[], std::string const& msg) {
  std::string const cmd = argv[0];
  auto last_slash = std::string(cmd).find_last_of('/');
  auto program = cmd.substr(last_slash + 1);
  std::cerr << msg << "\nUsage: " << program << " <command> [arguments]\n\n"
            << "Commands:\n"
            << command_usage << "\n";
}

void ListBuckets(google::cloud::storage::Client client, int& argc,
                 char* argv[]) {
  if (argc != 1) {
    throw Usage{"list-buckets"};
  }
  //! [list buckets] [START storage_list_buckets]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client) {
    int count = 0;
    gcs::ListBucketsReader bucket_list = client.ListBuckets();
    for (auto&& bucket_metadata : bucket_list) {
      if (!bucket_metadata) {
        throw std::runtime_error(bucket_metadata.status().message());
      }

      std::cout << bucket_metadata->name() << "\n";
      ++count;
    }

    if (count == 0) {
      std::cout << "No buckets in default project\n";
    }
  }
  //! [list buckets] [END storage_list_buckets]
  (std::move(client));
}

void ListBucketsForProject(google::cloud::storage::Client client, int& argc,
                           char* argv[]) {
  if (argc != 2) {
    throw Usage{"list-buckets-for-project <project-id>"};
  }
  auto project_id = ConsumeArg(argc, argv);
  //! [list buckets for project]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string project_id) {
    int count = 0;
    for (auto&& bucket_metadata : client.ListBucketsForProject(project_id)) {
      if (!bucket_metadata) {
        throw std::runtime_error(bucket_metadata.status().message());
      }

      std::cout << bucket_metadata->name() << "\n";
      ++count;
    }

    if (count == 0) {
      std::cout << "No buckets in project " << project_id << "\n";
    }
  }
  //! [list buckets for project]
  (std::move(client), project_id);
}

void CreateBucket(google::cloud::storage::Client client, int& argc,
                  char* argv[]) {
  if (argc != 2) {
    throw Usage{"create-bucket <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [create bucket] [START storage_create_bucket]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.CreateBucket(bucket_name, gcs::BucketMetadata());

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    std::cout << "Bucket " << bucket_metadata->name() << " created."
              << "\nFull Metadata: " << *bucket_metadata << "\n";
  }
  //! [create bucket] [END storage_create_bucket]
  (std::move(client), bucket_name);
}

void CreateBucketForProject(google::cloud::storage::Client client, int& argc,
                            char* argv[]) {
  if (argc != 3) {
    throw Usage{"create-bucket-for-project <bucket-name> <project-id>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto project_id = ConsumeArg(argc, argv);
  //! [create bucket for project]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string project_id) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.CreateBucketForProject(bucket_name, project_id,
                                      gcs::BucketMetadata());

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    std::cout << "Bucket " << bucket_metadata->name() << " created for project "
              << project_id << " [" << bucket_metadata->project_number() << "]"
              << "\nFull Metadata: " << *bucket_metadata << "\n";
  }
  //! [create bucket for project]
  (std::move(client), bucket_name, project_id);
}

void CreateBucketWithStorageClassLocation(google::cloud::storage::Client client,
                                          int& argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "create-bucket-with-storage-class-location <bucket-name> "
        "<storage-class> <location>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto storage_class = ConsumeArg(argc, argv);
  auto location = ConsumeArg(argc, argv);
  //! [create bucket class location]
  // [START storage_create_bucket_class_location]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string storage_class,
     std::string location) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.CreateBucket(bucket_name, gcs::BucketMetadata()
                                             .set_storage_class(storage_class)
                                             .set_location(location));

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    std::cout << "Bucket " << bucket_metadata->name() << " created."
              << "\nFull Metadata: " << *bucket_metadata << "\n";
  }
  // [END storage_create_bucket_class_location]
  //! [create bucket class location]
  (std::move(client), bucket_name, storage_class, location);
}

void GetBucketMetadata(google::cloud::storage::Client client, int& argc,
                       char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-bucket-metadata <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [get bucket metadata]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    std::cout << "The metadata for bucket " << bucket_metadata->name() << " is "
              << *bucket_metadata << "\n";
  }
  //! [get bucket metadata]
  (std::move(client), bucket_name);
}

void DeleteBucket(google::cloud::storage::Client client, int& argc,
                  char* argv[]) {
  if (argc != 2) {
    throw Usage{"delete-bucket <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [delete bucket] [START storage_delete_bucket]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name) {
    google::cloud::Status status = client.DeleteBucket(bucket_name);

    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }

    std::cout << "The bucket " << bucket_name << " was deleted successfully.\n";
  }
  //! [delete bucket] [END storage_delete_bucket]
  (std::move(client), bucket_name);
}

void ChangeDefaultStorageClass(google::cloud::storage::Client client, int& argc,
                               char* argv[]) {
  if (argc != 3) {
    throw Usage{"change-default-storage-class <bucket-name> <new-class>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto storage_class = ConsumeArg(argc, argv);
  //! [update bucket]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string storage_class) {
    StatusOr<gcs::BucketMetadata> meta = client.GetBucketMetadata(bucket_name);

    if (!meta) {
      throw std::runtime_error(meta.status().message());
    }

    meta->set_storage_class(storage_class);
    StatusOr<gcs::BucketMetadata> updated_meta =
        client.UpdateBucket(bucket_name, *meta);

    if (!updated_meta) {
      throw std::runtime_error(updated_meta.status().message());
    }

    std::cout << "Updated the storage class in " << updated_meta->name()
              << " to " << updated_meta->storage_class() << "."
              << "\nFull metadata:" << *updated_meta << "\n";
  }
  //! [update bucket]
  (std::move(client), bucket_name, storage_class);
}

void PatchBucketStorageClass(google::cloud::storage::Client client, int& argc,
                             char* argv[]) {
  if (argc != 3) {
    throw Usage{"patch-bucket-storage-class <bucket-name> <storage-class>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto storage_class = ConsumeArg(argc, argv);
  //! [patch bucket storage class] [START storage_change_default_storage_class]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string storage_class) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      throw std::runtime_error(original.status().message());
    }

    gcs::BucketMetadata desired = *original;
    desired.set_storage_class(storage_class);

    StatusOr<gcs::BucketMetadata> patched =
        client.PatchBucket(bucket_name, *original, desired);

    if (!patched) {
      throw std::runtime_error(patched.status().message());
    }

    std::cout << "Storage class for bucket " << patched->name()
              << " has been patched to " << patched->storage_class() << "."
              << "\nFull metadata: " << *patched << "\n";
  }
  //! [patch bucket storage class] [END storage_change_default_storage_class]
  (std::move(client), bucket_name, storage_class);
}

void PatchBucketStorageClassWithBuilder(google::cloud::storage::Client client,
                                        int& argc, char* argv[]) {
  if (argc != 3) {
    throw Usage{
        "patch-bucket-storage-classwith-builder <bucket-name> <storage-class>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto storage_class = ConsumeArg(argc, argv);
  //! [patch bucket storage class with builder]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string storage_class) {
    StatusOr<gcs::BucketMetadata> patched = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetStorageClass(storage_class));

    if (!patched) {
      throw std::runtime_error(patched.status().message());
    }

    std::cout << "Storage class for bucket " << patched->name()
              << " has been patched to " << patched->storage_class() << "."
              << "\nFull metadata: " << *patched << "\n";
  }
  //! [patch bucket storage class with builder]
  (std::move(client), bucket_name, storage_class);
}

void GetBucketClassAndLocation(google::cloud::storage::Client client, int& argc,
                               char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-bucket-class-and-location <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  // [START storage_get_bucket_class_and_location]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    std::cout << "Bucket " << bucket_metadata->name()
              << " default storage class is "
              << bucket_metadata->storage_class() << ", and the location is "
              << bucket_metadata->location() << '\n';
  }
  // [END storage_get_bucket_class_and_location]
  (std::move(client), bucket_name);
}

void AddBucketDefaultKmsKey(google::cloud::storage::Client client, int& argc,
                            char* argv[]) {
  if (argc != 3) {
    throw Usage{"add-bucket-default-kms-key <bucket-name> <key-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto key_name = ConsumeArg(argc, argv);
  //! [add bucket kms key] [START storage_set_bucket_default_kms_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string key_name) {
    StatusOr<gcs::BucketMetadata> updated_metadata = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().SetEncryption(
                         gcs::BucketEncryption{key_name}));

    if (!updated_metadata) {
      throw std::runtime_error(updated_metadata.status().message());
    }

    if (!updated_metadata->has_encryption()) {
      std::cerr << "The change to set the encryption attribute on bucket "
                << updated_metadata->name()
                << " was sucessful, but the encryption is not set."
                << "This is unexpected, maybe a concurrent change?\n";
      return;
    }

    std::cout << "Successfully set default KMS key on bucket  "
              << updated_metadata->name() << " to "
              << updated_metadata->encryption().default_kms_key_name << "."
              << "\nFull metadata: " << *updated_metadata << "\n";
  }
  //! [add bucket kms key] [END storage_set_bucket_default_kms_key]
  (std::move(client), bucket_name, key_name);
}

void GetBucketDefaultKmsKey(google::cloud::storage::Client client, int& argc,
                            char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-bucket-default-kms-key <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [get bucket default kms key] [START storage_bucket_get_default_kms_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> meta = client.GetBucketMetadata(bucket_name);

    if (!meta) {
      throw std::runtime_error(meta.status().message());
    }

    if (!meta->has_encryption()) {
      std::cout << "The bucket " << meta->name()
                << " does not have a default KMS key set.\n";
      return;
    }

    std::cout << "The default KMS key for bucket " << meta->name()
              << " is: " << meta->encryption().default_kms_key_name << "\n";
  }
  //! [get bucket default kms key] [END storage_bucket_get_default_kms_key]
  (std::move(client), bucket_name);
}

void RemoveBucketDefaultKmsKey(google::cloud::storage::Client client, int& argc,
                               char* argv[]) {
  if (argc != 2) {
    throw Usage{"remove-bucket-default-kms-key <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [remove bucket default kms key]
  // [START storage_bucket_delete_default_kms_key]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> updated_metadata = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().ResetEncryption());

    if (!updated_metadata) {
      throw std::runtime_error(updated_metadata.status().message());
    }

    std::cout << "Successfully removed default KMS key on bucket "
              << updated_metadata->name() << "\n";
  }
  // [END storage_bucket_delete_default_kms_key]
  //! [remove bucket default kms key]
  (std::move(client), bucket_name);
}

void EnableBucketPolicyOnly(google::cloud::storage::Client client, int& argc,
                            char* argv[]) {
  if (argc != 2) {
    throw Usage{"enable-bucket-policy-only <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [enable bucket policy only]
  // [START storage_enable_bucket_policy_only]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    gcs::BucketIamConfiguration configuration;
    configuration.bucket_policy_only = gcs::BucketPolicyOnly{true};
    StatusOr<gcs::BucketMetadata> updated_metadata = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().SetIamConfiguration(
                         std::move(configuration)));

    if (!updated_metadata) {
      throw std::runtime_error(updated_metadata.status().message());
    }

    std::cout << "Successfully enabled Bucket Policy Only on bucket "
              << updated_metadata->name() << "\n";
  }
  // [END storage_enable_bucket_policy_only]
  //! [enable bucket policy only]
  (std::move(client), bucket_name);
}

void DisableBucketPolicyOnly(google::cloud::storage::Client client, int& argc,
                             char* argv[]) {
  if (argc != 2) {
    throw Usage{"disable-bucket-policy-only <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [disable bucket policy only]
  // [START storage_disable_bucket_policy_only]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    gcs::BucketIamConfiguration configuration;
    configuration.bucket_policy_only = gcs::BucketPolicyOnly{false};
    StatusOr<gcs::BucketMetadata> updated_metadata = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().SetIamConfiguration(
                         std::move(configuration)));

    if (!updated_metadata) {
      throw std::runtime_error(updated_metadata.status().message());
    }

    std::cout << "Successfully disabled Bucket Policy Only on bucket "
              << updated_metadata->name() << "\n";
  }
  // [END storage_disable_bucket_policy_only]
  //! [disable bucket policy only]
  (std::move(client), bucket_name);
}

void GetBucketPolicyOnly(google::cloud::storage::Client client, int& argc,
                         char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-bucket-policy-only <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [get bucket policy only]
  // [START storage_get_bucket_policy_only]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    if (bucket_metadata->has_iam_configuration() &&
        bucket_metadata->iam_configuration().bucket_policy_only.has_value()) {
      gcs::BucketPolicyOnly bucket_policy_only =
          *bucket_metadata->iam_configuration().bucket_policy_only;

      std::cout << "Bucket Policy Only is enabled for "
                << bucket_metadata->name() << "\n";
      std::cout << "Bucket will be locked on " << bucket_policy_only << "\n";
    } else {
      std::cout << "Bucket Policy Only is not enabled for "
                << bucket_metadata->name() << "\n";
    }
  }
  // [END storage_get_bucket_policy_only]
  //! [get bucket policy only]
  (std::move(client), bucket_name);
}

void AddBucketLabel(google::cloud::storage::Client client, int& argc,
                    char* argv[]) {
  if (argc != 4) {
    throw Usage{"add-bucket-label <bucket-name> <label-key> <label-value>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto label_key = ConsumeArg(argc, argv);
  auto label_value = ConsumeArg(argc, argv);
  //! [add bucket label] [START storage_add_bucket_label]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string label_key,
     std::string label_value) {
    StatusOr<gcs::BucketMetadata> updated_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetLabel(label_key, label_value));

    if (!updated_metadata) {
      throw std::runtime_error(updated_metadata.status().message());
    }

    std::cout << "Successfully set label " << label_key << " to " << label_value
              << " on bucket  " << updated_metadata->name() << ".";
    std::cout << " The bucket labels are now:";
    for (auto const& kv : updated_metadata->labels()) {
      std::cout << "\n  " << kv.first << ": " << kv.second;
    }
    std::cout << "\n";
  }
  //! [add bucket label] [END storage_add_bucket_label]
  (std::move(client), bucket_name, label_key, label_value);
}

void GetBucketLabels(google::cloud::storage::Client client, int& argc,
                     char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-bucket-labels <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [get bucket labels] [START storage_get_bucket_labels]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name, gcs::Fields("labels"));

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    if (bucket_metadata->labels().empty()) {
      std::cout << "The bucket " << bucket_name << " has no labels set.\n";
      return;
    }

    std::cout << "The labels for bucket " << bucket_name << " are:";
    for (auto const& kv : bucket_metadata->labels()) {
      std::cout << "\n  " << kv.first << ": " << kv.second;
    }
    std::cout << "\n";
  }
  //! [get bucket label] [END storage_get_bucket_labels]
  (std::move(client), bucket_name);
}

void RemoveBucketLabel(google::cloud::storage::Client client, int& argc,
                       char* argv[]) {
  if (argc != 3) {
    throw Usage{"remove-bucket-label <bucket-name> <label-key>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto label_key = ConsumeArg(argc, argv);
  //! [remove bucket label] [START storage_remove_bucket_label]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string label_key) {
    StatusOr<gcs::BucketMetadata> updated_metadata = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().ResetLabel(label_key));

    if (!updated_metadata) {
      throw std::runtime_error(updated_metadata.status().message());
    }

    std::cout << "Successfully reset label " << label_key << " on bucket  "
              << updated_metadata->name() << ".";
    if (updated_metadata->labels().empty()) {
      std::cout << " The bucket now has no labels.\n";
      return;
    }
    std::cout << " The bucket labels are now:";
    for (auto const& kv : updated_metadata->labels()) {
      std::cout << "\n  " << kv.first << ": " << kv.second;
    }
    std::cout << "\n";
  }
  //! [remove bucket label] [END storage_remove_bucket_label]
  (std::move(client), bucket_name, label_key);
}

void GetBucketLifecycleManagement(google::cloud::storage::Client client,
                                  int& argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-bucket-lifecycle-management <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  // [START storage_view_lifecycle_management_configuration]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
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
  (std::move(client), bucket_name);
}

void EnableBucketLifecycleManagement(google::cloud::storage::Client client,
                                     int& argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"enable-bucket-lifecycle-management <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [enable_bucket_lifecycle_management]
  // [START storage_enable_bucket_lifecycle_management]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
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
  (std::move(client), bucket_name);
}

void DisableBucketLifecycleManagement(google::cloud::storage::Client client,
                                      int& argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"disable-bucket-lifecycle-management <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [disable_bucket_lifecycle_management]
  // [START storage_disable_bucket_lifecycle_management]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
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
  (std::move(client), bucket_name);
}

void GetBilling(google::cloud::storage::Client client, int& argc,
                char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-billing <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [get billing] [START storage_get_requester_pays_status]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    if (!bucket_metadata->has_billing()) {
      std::cout
          << "The bucket " << bucket_metadata->name() << " does not have a"
          << " billing configuration. The default applies, i.e., the project"
          << " that owns the bucket pays for the requests.\n";
      return;
    }

    if (bucket_metadata->billing().requester_pays) {
      std::cout
          << "The bucket " << bucket_metadata->name()
          << " is configured to charge the calling project for the requests.\n";
    } else {
      std::cout << "The bucket " << bucket_metadata->name()
                << " is configured to charge the project that owns the bucket "
                   "for the requests.\n";
    }
  }
  //! [get billing] [END storage_get_requester_pays_status]
  (std::move(client), bucket_name);
}

void EnableRequesterPays(google::cloud::storage::Client client, int& argc,
                         char* argv[]) {
  if (argc != 2) {
    throw Usage{"enable-requester-pays <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [enable requester pays] [START storage_enable_requester_pays]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetBilling(gcs::BucketBilling{true}));

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    std::cout << "Billing configuration for bucket " << bucket_metadata->name()
              << " is updated. The bucket now";
    if (!bucket_metadata->has_billing()) {
      std::cout << " has no billing configuration.\n";
    } else if (bucket_metadata->billing().requester_pays) {
      std::cout << " is configured to charge the caller for requests\n";
    } else {
      std::cout << " is configured to charge the project that owns the bucket"
                << " for requests.\n";
    }
  }
  //! [enable requester pays] [END storage_enable_requester_pays]
  (std::move(client), bucket_name);
}

void DisableRequesterPays(google::cloud::storage::Client client, int& argc,
                          char* argv[]) {
  if (argc != 3) {
    throw Usage{"disable-requester-pays <bucket-name> <project-id>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto project_id = ConsumeArg(argc, argv);
  //! [disable requester pays] [START storage_disable_requester_pays]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string project_id) {
    StatusOr<gcs::BucketMetadata> bucket_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetBilling(gcs::BucketBilling{false}),
        gcs::UserProject(project_id));

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    std::cout << "Billing configuration for bucket " << bucket_name
              << " is updated. The bucket now";
    if (!bucket_metadata->has_billing()) {
      std::cout << " has no billing configuration.\n";
    } else if (bucket_metadata->billing().requester_pays) {
      std::cout << " is configured to charge the caller for requests\n";
    } else {
      std::cout << " is configured to charge the project that owns the bucket"
                << " for requests.\n";
    }
  }
  //! [disable requester pays] [END storage_disable_requester_pays]
  (std::move(client), bucket_name, project_id);
}

void WriteObjectRequesterPays(google::cloud::storage::Client client, int& argc,
                              char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "write-object-requester-pays <bucket-name> <object-name>"
        " <billed-project>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto billed_project = ConsumeArg(argc, argv);
  //! [write object requester pays]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string billed_project) {
    std::string const text = "Lorem ipsum dolor sit amet";

    gcs::ObjectWriteStream stream = client.WriteObject(
        bucket_name, object_name, gcs::UserProject(billed_project));

    for (int lineno = 0; lineno != 10; ++lineno) {
      // Add 1 to the counter, because it is conventional to number lines
      // starting at 1.
      stream << (lineno + 1) << ": I will write better examples\n";
    }

    stream.Close();

    StatusOr<gcs::ObjectMetadata> metadata = std::move(stream).metadata();
    if (!metadata) {
      throw std::runtime_error(metadata.status().message());
    }
    std::cout << "Successfully wrote to object " << metadata->name()
              << " its size is: " << metadata->size()
              << "\nFull metadata: " << *metadata << "\n";
  }
  //! [write object requester pays]
  (std::move(client), bucket_name, object_name, billed_project);
}

void ReadObjectRequesterPays(google::cloud::storage::Client client, int& argc,
                             char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "read-object-requester-pays <bucket-name> <object-name>"
        " <billed-project>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto billed_project = ConsumeArg(argc, argv);
  //! [read object requester pays]
  // [START storage_download_file_requester_pays]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string billed_project) {
    gcs::ObjectReadStream stream = client.ReadObject(
        bucket_name, object_name, gcs::UserProject(billed_project));

    std::string line;
    while (std::getline(stream, line, '\n')) {
      std::cout << line << "\n";
    }
  }
  // [END storage_download_file_requester_pays]
  //! [read object requester pays]
  (std::move(client), bucket_name, object_name, billed_project);
}

void DeleteObjectRequesterPays(google::cloud::storage::Client client, int& argc,
                               char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "delete-object-requester-pays <bucket-name> <object-name>"
        " <billed-project>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto object_name = ConsumeArg(argc, argv);
  auto billed_project = ConsumeArg(argc, argv);
  //! [delete object requester pays]
  namespace gcs = google::cloud::storage;
  [](gcs::Client client, std::string bucket_name, std::string object_name,
     std::string billed_project) {
    google::cloud::Status status = client.DeleteObject(
        bucket_name, object_name, gcs::UserProject(billed_project));
    if (!status.ok()) {
      throw std::runtime_error(status.message());
    }
  }
  //! [read object requester pays]
  (std::move(client), bucket_name, object_name, billed_project);
}

void GetDefaultEventBasedHold(google::cloud::storage::Client client, int& argc,
                              char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-default-event-based-hold <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [get default event based hold]
  // [START storage_get_default_event_based_hold]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    std::cout << "The default event-based hold for objects in bucket "
              << bucket_metadata->name() << " is "
              << (bucket_metadata->default_event_based_hold() ? "enabled"
                                                              : "disabled")
              << "\n";
  }
  // [END storage_get_default_event_based_hold]
  //! [get default event based hold]
  (std::move(client), bucket_name);
}

void EnableDefaultEventBasedHold(google::cloud::storage::Client client,
                                 int& argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"enable-default-event-based-hold <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [enable default event based hold]
  // [START storage_enable_default_event_based_hold]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      throw std::runtime_error(original.status().message());
    }

    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetDefaultEventBasedHold(true),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      throw std::runtime_error(patched_metadata.status().message());
    }

    std::cout << "The default event-based hold for objects in bucket "
              << bucket_name << " is "
              << (patched_metadata->default_event_based_hold() ? "enabled"
                                                               : "disabled")
              << "\n";
  }
  // [END storage_enable_default_event_based_hold]
  //! [enable default event based hold]
  (std::move(client), bucket_name);
}

void DisableDefaultEventBasedHold(google::cloud::storage::Client client,
                                  int& argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"disable-default-event-based-hold <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [disable default event based hold]
  // [START storage_disable_default_event_based_hold]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      throw std::runtime_error(original.status().message());
    }

    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetDefaultEventBasedHold(false),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      throw std::runtime_error(patched_metadata.status().message());
    }

    std::cout << "The default event-based hold for objects in bucket "
              << bucket_name << " is "
              << (patched_metadata->default_event_based_hold() ? "enabled"
                                                               : "disabled")
              << "\n";
  }
  // [END storage_disable_default_event_based_hold]
  //! [disable default event based hold]
  (std::move(client), bucket_name);
}

void GetObjectVersioning(google::cloud::storage::Client client, int& argc,
                         char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-object-versioning <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [view_versioning_status] [START storage_view_versioning_status]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    if (bucket_metadata->versioning().has_value()) {
      std::cout << "Object versioning for bucket " << bucket_name << " is "
                << (bucket_metadata->versioning()->enabled ? "enabled"
                                                           : "disabled")
                << "\n";
    } else {
      std::cout << "Object versioning for bucket " << bucket_name
                << " is disabled.\n";
    }
  }
  //! [view_versioning_status] [END storage_view_versioning_status]
  (std::move(client), bucket_name);
}

void EnableObjectVersioning(google::cloud::storage::Client client, int& argc,
                            char* argv[]) {
  if (argc != 2) {
    throw Usage{"enable-object-versioning <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [enable versioning] [START storage_enable_versioning]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      throw std::runtime_error(original.status().message());
    }

    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetVersioning(
            gcs::BucketVersioning{true}),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      throw std::runtime_error(patched_metadata.status().message());
    }

    if (patched_metadata->versioning().has_value()) {
      std::cout << "Object versioning for bucket " << bucket_name << " is "
                << (patched_metadata->versioning()->enabled ? "enabled"
                                                            : "disabled")
                << "\n";
    } else {
      std::cout << "Object versioning for bucket " << bucket_name
                << " is disabled.\n";
    }
  }
  //! [enable versioning] [END storage_enable_versioning]
  (std::move(client), bucket_name);
}

void DisableObjectVersioning(google::cloud::storage::Client client, int& argc,
                             char* argv[]) {
  if (argc != 2) {
    throw Usage{"disable-object-versioning <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [disable versioning] [START storage_disable_versioning]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      throw std::runtime_error(original.status().message());
    }

    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetVersioning(
            gcs::BucketVersioning{false}),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      throw std::runtime_error(patched_metadata.status().message());
    }

    if (patched_metadata->versioning().has_value()) {
      std::cout << "Object versioning for bucket " << bucket_name << " is "
                << (patched_metadata->versioning()->enabled ? "enabled"
                                                            : "disabled")
                << "\n";
    } else {
      std::cout << "Object versioning for bucket " << bucket_name
                << " is disabled.\n";
    }
  }
  //! [disable versioning] [END storage_disable_versioning]
  (std::move(client), bucket_name);
}

void GetRetentionPolicy(google::cloud::storage::Client client, int& argc,
                        char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-retention-policy <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [get retention policy]
  // [START storage_get_retention_policy]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
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
  (std::move(client), bucket_name);
}

void SetRetentionPolicy(google::cloud::storage::Client client, int& argc,
                        char* argv[]) {
  if (argc != 3) {
    throw Usage{"set-retention-policy <bucket-name> <period>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto period = std::stol(ConsumeArg(argc, argv));
  //! [set retention policy]
  // [START storage_set_retention_policy]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::chrono::seconds period) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      throw std::runtime_error(original.status().message());
    }

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
  (std::move(client), bucket_name, std::chrono::seconds(period));
}

void RemoveRetentionPolicy(google::cloud::storage::Client client, int& argc,
                           char* argv[]) {
  if (argc != 2) {
    throw Usage{"remove-retention-policy <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [remove retention policy]
  // [START storage_remove_retention_policy]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      throw std::runtime_error(original.status().message());
    }

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
  (std::move(client), bucket_name);
}

void LockRetentionPolicy(google::cloud::storage::Client client, int& argc,
                         char* argv[]) {
  if (argc != 2) {
    throw Usage{"lock-retention-policy <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [lock retention policy]
  // [START storage_lock_retention_policy]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      throw std::runtime_error(original.status().message());
    }

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
  (std::move(client), bucket_name);
}

void GetStaticWebsiteConfiguration(google::cloud::storage::Client client,
                                   int& argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-static-website-configuration <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [print bucket website configuration]
  // [START storage_print_bucket_website_configuration]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!bucket_metadata) {
      throw std::runtime_error(bucket_metadata.status().message());
    }

    if (!bucket_metadata->has_website()) {
      std::cout << "Static website configuration is not set for bucket "
                << bucket_metadata->name() << "\n";
      return;
    }

    std::cout << "Static website configuration set for bucket "
              << bucket_metadata->name() << "\nThe main page suffix is: "
              << bucket_metadata->website().main_page_suffix
              << "\nThe not found page is: "
              << bucket_metadata->website().not_found_page << "\n";
  }
  // [END storage_print_bucket_website_configuration]
  //! [print bucket website configuration]
  (std::move(client), bucket_name);
}

void SetStaticWebsiteConfiguration(google::cloud::storage::Client client,
                                   int& argc, char* argv[]) {
  if (argc != 4) {
    throw Usage{
        "set-static-website-configuration <bucket-name> <main-page-suffix> "
        "<not-found-page>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto main_page_suffix = ConsumeArg(argc, argv);
  auto not_found_page = ConsumeArg(argc, argv);
  //! [define bucket website configuration]
  // [START storage_define_bucket_website_configuration]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string main_page_suffix,
     std::string not_found_page) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      throw std::runtime_error(original.status().message());
    }

    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetWebsite(
            gcs::BucketWebsite{main_page_suffix, not_found_page}),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      throw std::runtime_error(patched_metadata.status().message());
    }

    if (!patched_metadata->has_website()) {
      std::cout << "Static website configuration is not set for bucket "
                << patched_metadata->name() << "\n";
      return;
    }

    std::cout << "Static website configuration successfully set for bucket "
              << patched_metadata->name() << "\nNew main page suffix is: "
              << patched_metadata->website().main_page_suffix
              << "\nNew not found page is: "
              << patched_metadata->website().not_found_page << "\n";
  }
  // [END storage_define_bucket_website_configuration]
  //! [define bucket website configuration]
  (std::move(client), bucket_name, main_page_suffix, not_found_page);
}

void RemoveStaticWebsiteConfiguration(google::cloud::storage::Client client,
                                      int& argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"remove-static-website-configuration <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [remove bucket website configuration]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      throw std::runtime_error(original.status().message());
    }

    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().ResetWebsite(),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      throw std::runtime_error(patched_metadata.status().message());
    }

    if (!patched_metadata->has_website()) {
      std::cout << "Static website configuration removed for bucket "
                << patched_metadata->name() << "\n";
      return;
    }

    std::cout << "Static website configuration is set for bucket "
              << patched_metadata->name()
              << "\nThis is unexpected, and may indicate that another"
              << " application has modified the bucket concurrently.\n";
  }
  //! [remove bucket website configuration]
  (std::move(client), bucket_name);
}

void SetCorsConfiguration(google::cloud::storage::Client client, int& argc,
                          char* argv[]) {
  if (argc != 3) {
    throw Usage{"set-cors-configuration <bucket-name> <origin>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  auto origin = ConsumeArg(argc, argv);
  //! [cors configuration] [START storage_cors_configuration]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string origin) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      throw std::runtime_error(original.status().message());
    }

    std::vector<gcs::CorsEntry> cors_configuration;
    cors_configuration.emplace_back(
        gcs::CorsEntry{3600, {"GET"}, {origin}, {"Content-Type"}});

    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetCors(cors_configuration),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      throw std::runtime_error(patched_metadata.status().message());
    }

    if (patched_metadata->cors().empty()) {
      std::cout << "Cors configuration is not set for bucket "
                << patched_metadata->name() << "\n";
      return;
    }

    std::cout << "Cors configuration successfully set for bucket "
              << patched_metadata->name() << "\nNew cors configuration: ";
    for (auto const& cors_entry : patched_metadata->cors()) {
      std::cout << "\n  " << cors_entry;
    }
  }
  //! [cors configuration] [END storage_cors_configuration]
  (std::move(client), bucket_name, origin);
}

void CreateSignedPolicyDocument(google::cloud::storage::Client client,
                                int& argc, char* argv[]) {
  if (argc != 1) {
    throw Usage{"create-signed-policy-document"};
  }
  //! [create signed policy document]
  namespace gcs = google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client) {
    StatusOr<gcs::PolicyDocumentResult> signed_document =
        client.CreateSignedPolicyDocument(gcs::PolicyDocument{
            std::chrono::system_clock::now() + std::chrono::minutes(15),
            {
                gcs::PolicyDocumentCondition::StartsWith("key", ""),
                gcs::PolicyDocumentCondition::ExactMatchObject(
                    "acl", "bucket-owner-read"),
                gcs::PolicyDocumentCondition::ExactMatchObject("bucket",
                                                               "travel-maps"),
                gcs::PolicyDocumentCondition::ExactMatch("Content-Type",
                                                         "image/jpeg"),
                gcs::PolicyDocumentCondition::ContentLengthRange(0, 1000000),
            }});

    if (!signed_document) {
      throw std::runtime_error(signed_document.status().message());
    }

    std::cout << "The signed document is: " << *signed_document << "\n\n"
              << "You can use this with an HTML form.\n";
  }
  //! [create signed policy document]
  (std::move(client));
}
}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Create a client to communicate with Google Cloud Storage.
  //! [create client]
  google::cloud::StatusOr<google::cloud::storage::Client> client =
      google::cloud::storage::Client::CreateDefaultClient();
  if (!client) {
    std::cerr << "Failed to create Storage Client, status=" << client.status()
              << "\n";
    return 1;
  }
  //! [create client]

  using CommandType =
      std::function<void(google::cloud::storage::Client, int&, char*[])>;
  std::map<std::string, CommandType> commands = {
      {"list-buckets", &ListBuckets},
      {"list-buckets-for-project", &ListBucketsForProject},
      {"create-bucket", &CreateBucket},
      {"create-bucket-for-project", &CreateBucketForProject},
      {"create-bucket-with-storage-class-location",
       &CreateBucketWithStorageClassLocation},
      {"get-bucket-metadata", &GetBucketMetadata},
      {"delete-bucket", &DeleteBucket},
      {"change-default-storage-class", &ChangeDefaultStorageClass},
      {"patch-bucket-storage-class", &PatchBucketStorageClass},
      {"patch-bucket-storage-class-with-builder",
       &PatchBucketStorageClassWithBuilder},
      {"get-bucket-class-and-location", &GetBucketClassAndLocation},
      {"add-bucket-default-kms-key", &AddBucketDefaultKmsKey},
      {"get-bucket-default-kms-key", &GetBucketDefaultKmsKey},
      {"remove-bucket-default-kms-key", &RemoveBucketDefaultKmsKey},
      {"enable-bucket-policy-only", &EnableBucketPolicyOnly},
      {"disable-bucket-policy-only", &DisableBucketPolicyOnly},
      {"get-bucket-policy-only", &GetBucketPolicyOnly},
      {"add-bucket-label", &AddBucketLabel},
      {"get-bucket-labels", &GetBucketLabels},
      {"remove-bucket-label", &RemoveBucketLabel},
      {"get-bucket-lifecycle-management", &GetBucketLifecycleManagement},
      {"enable-bucket-lifecycle-management", &EnableBucketLifecycleManagement},
      {"disable-bucket-lifecycle-management",
       &DisableBucketLifecycleManagement},
      {"get-billing", &GetBilling},
      {"enable-requester-pays", &EnableRequesterPays},
      {"disable-requester-pays", &DisableRequesterPays},
      {"write-object-requester-pays", &WriteObjectRequesterPays},
      {"read-object-requester-pays", &ReadObjectRequesterPays},
      {"delete-object-requester-pays", &DeleteObjectRequesterPays},
      {"get-default-event-based-hold", &GetDefaultEventBasedHold},
      {"enable-default-event-based-hold", &EnableDefaultEventBasedHold},
      {"disable-default-event-based-hold", &DisableDefaultEventBasedHold},
      {"get-object-versioning", &GetObjectVersioning},
      {"enable-object-versioning", &EnableObjectVersioning},
      {"disable-object-versioning", &DisableObjectVersioning},
      {"get-retention-policy", &GetRetentionPolicy},
      {"set-retention-policy", &SetRetentionPolicy},
      {"remove-retention-policy", &RemoveRetentionPolicy},
      {"lock-retention-policy", &LockRetentionPolicy},
      {"get-static-website-configuration", &GetStaticWebsiteConfiguration},
      {"set-static-website-configuration", &SetStaticWebsiteConfiguration},
      {"remove-static-website-configuration",
       &RemoveStaticWebsiteConfiguration},
      {"set-cors-configuration", &SetCorsConfiguration},
      {"create-signed-policy-document", &CreateSignedPolicyDocument},
  };
  for (auto&& kv : commands) {
    try {
      int fake_argc = 0;
      kv.second(*client, fake_argc, argv);
    } catch (Usage const& u) {
      command_usage += "    ";
      command_usage += u.msg;
      command_usage += "\n";
    } catch (...) {
      // ignore other exceptions.
    }
  }

  if (argc < 2) {
    PrintUsage(argc, argv, "Missing command");
    return 1;
  }

  std::string const command = ConsumeArg(argc, argv);
  auto it = commands.find(command);
  if (commands.end() == it) {
    PrintUsage(argc, argv, "Unknown command: " + command);
    return 1;
  }

  it->second(*client, argc, argv);

  return 0;
} catch (Usage const& ex) {
  PrintUsage(argc, argv, ex.msg);
  return 1;
} catch (std::exception const& ex) {
  std::cerr << "Standard C++ exception raised: " << ex.what() << "\n";
  return 1;
}

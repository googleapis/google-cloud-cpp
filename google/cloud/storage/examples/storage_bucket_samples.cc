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
            << command_usage << std::endl;
}

void ListBuckets(google::cloud::storage::Client client, int& argc,
                 char* argv[]) {
  if (argc != 1) {
    throw Usage{"list-buckets"};
  }
  //! [list buckets] [START storage_list_buckets]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client) {
    int count = 0;
    gcs::ListBucketsReader bucket_list = client.ListBuckets();
    for (auto&& bucket_metadata : bucket_list) {
      if (!bucket_metadata) {
        std::cerr << "Error reading bucket list for default project"
                  << ", status=" << bucket_metadata.status() << std::endl;
        return;
      }

      std::cout << bucket_metadata->name() << std::endl;
      ++count;
    }

    if (count == 0) {
      std::cout << "No buckets in default project" << std::endl;
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
        std::cerr << "Error reading bucket list for project " << project_id
                  << ", status=" << bucket_metadata.status() << std::endl;
        return;
      }

      std::cout << bucket_metadata->name() << std::endl;
      ++count;
    }

    if (count == 0) {
      std::cout << "No buckets in project " << project_id << std::endl;
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.CreateBucket(bucket_name, gcs::BucketMetadata());

    if (!bucket_metadata) {
      std::cerr << "Could not create bucket " << bucket_name << std::endl;
      return;
    }

    std::cout << "Bucket " << bucket_metadata->name() << " created."
              << "\nFull Metadata: " << *bucket_metadata << std::endl;
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string project_id) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.CreateBucketForProject(bucket_name, project_id,
                                      gcs::BucketMetadata());

    if (!bucket_metadata) {
      std::cerr << "Could not create bucket " << bucket_name << " in project "
                << project_id << std::endl;
      return;
    }

    std::cout << "Bucket " << bucket_metadata->name() << " created for project "
              << project_id << " [" << bucket_metadata->project_number() << "]"
              << "\nFull Metadata: " << *bucket_metadata << std::endl;
  }
  //! [create bucket for project]
  (std::move(client), bucket_name, project_id);
}

void GetBucketMetadata(google::cloud::storage::Client client, int& argc,
                       char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-bucket-metadata <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [get bucket metadata] [START storage_get_bucket_metadata]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!bucket_metadata) {
      std::cerr << "Error while getting metadata for bucket " << bucket_name
                << ", status=" << bucket_metadata.status() << std::endl;
      return;
    }
    std::cout << "The metadata for bucket " << bucket_metadata->name() << " is "
              << *bucket_metadata << std::endl;
  }
  //! [get bucket metadata] [END storage_get_bucket_metadata]
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
      std::cerr << "Error while deleting bucket " << bucket_name
                << ", status=" << status << std::endl;
      return;
    }

    std::cout << "The bucket " << bucket_name << " was deleted successfully."
              << std::endl;
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string storage_class) {
    StatusOr<gcs::BucketMetadata> meta = client.GetBucketMetadata(bucket_name);

    if (!meta) {
      std::cerr << "Error while getting metadata for bucket " << bucket_name
                << ", status=" << meta.status() << std::endl;
      return;
    }

    meta->set_storage_class(storage_class);
    StatusOr<gcs::BucketMetadata> updated_meta =
        client.UpdateBucket(bucket_name, *meta);

    if (!updated_meta) {
      std::cerr << "Error updating the metadata for bucket " << meta->name()
                << ", status=" << meta.status() << std::endl;
      return;
    }

    std::cout << "Updated the storage class in " << updated_meta->name()
              << " to " << updated_meta->storage_class() << "."
              << "\nFull metadata:" << *updated_meta << std::endl;
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string storage_class) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      std::cerr << "Error while getting metadata for bucket " << bucket_name
                << ", status=" << original.status() << std::endl;
      return;
    }

    gcs::BucketMetadata desired = *original;
    desired.set_storage_class(storage_class);

    StatusOr<gcs::BucketMetadata> patched =
        client.PatchBucket(bucket_name, *original, desired);

    if (!patched) {
      std::cerr << "Error patching the metadata for bucket " << original->name()
                << ", status=" << patched.status() << std::endl;
      return;
    }

    std::cout << "Storage class for bucket " << patched->name()
              << " has been patched to " << patched->storage_class() << "."
              << "\nFull metadata: " << *patched << std::endl;
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string storage_class) {
    StatusOr<gcs::BucketMetadata> patched = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetStorageClass(storage_class));

    if (!patched) {
      std::cerr << "Error patching the metadata for bucket " << bucket_name
                << ", status=" << patched.status() << std::endl;
      return;
    }

    std::cout << "Storage class for bucket " << patched->name()
              << " has been patched to " << patched->storage_class() << "."
              << "\nFull metadata: " << *patched << std::endl;
  }
  //! [patch bucket storage class with builder]
  (std::move(client), bucket_name, storage_class);
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string key_name) {
    StatusOr<gcs::BucketMetadata> updated_metadata = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().SetEncryption(
                         gcs::BucketEncryption{key_name}));

    if (!updated_metadata) {
      std::cerr << "Error updating the metadata for bucket " << bucket_name
                << ", status=" << updated_metadata.status() << std::endl;
      return;
    }

    if (!updated_metadata->has_encryption()) {
      std::cerr << "The change to set the encryption attribute on bucket "
                << updated_metadata->name()
                << " was sucessful, but the encryption is not set."
                << "This is unexpected, maybe a concurrent change?"
                << std::endl;
      return;
    }

    std::cout << "Successfully set default KMS key on bucket  "
              << updated_metadata->name() << " to "
              << updated_metadata->encryption().default_kms_key_name << "."
              << "\nFull metadata: " << *updated_metadata << std::endl;
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
  //! [get bucket default kms key] [START storage_get_bucket_default_kms_key]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> meta = client.GetBucketMetadata(bucket_name);

    if (!meta) {
      std::cerr << "Error reading the metadata for bucket " << bucket_name
                << ", status=" << meta.status() << std::endl;
      return;
    }

    if (!meta->has_encryption()) {
      std::cout << "The bucket " << meta->name()
                << " does not have a default KMS key set." << std::endl;
      return;
    }

    std::cout << "The default KMS key for bucket " << meta->name()
              << " is: " << meta->encryption().default_kms_key_name
              << std::endl;
  }
  //! [get bucket default kms key] [END storage_get_bucket_default_kms_key]
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> updated_metadata = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().ResetEncryption());

    if (!updated_metadata) {
      std::cerr << "Error resetting the default encryption key for bucket "
                << bucket_name << ", status=" << updated_metadata.status()
                << std::endl;
      return;
    }

    std::cout << "Successfully removed default KMS key on bucket "
              << updated_metadata->name() << std::endl;
  }
  // [END storage_bucket_delete_default_kms_key]
  //! [remove bucket default kms key]
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string label_key,
     std::string label_value) {
    StatusOr<gcs::BucketMetadata> updated_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetLabel(label_key, label_value));

    if (!updated_metadata) {
      std::cerr << "Error setting a label on bucket " << bucket_name
                << ", status=" << updated_metadata.status() << std::endl;
      return;
    }

    std::cout << "Successfully set label " << label_key << " to " << label_value
              << " on bucket  " << updated_metadata->name() << ".";
    std::cout << " The bucket labels are now:";
    for (auto const& kv : updated_metadata->labels()) {
      std::cout << "\n  " << kv.first << ": " << kv.second;
    }
    std::cout << std::endl;
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name, gcs::Fields("labels"));

    if (!bucket_metadata) {
      std::cerr << "Error reading labels on bucket " << bucket_name
                << ", status=" << bucket_metadata.status() << std::endl;
      return;
    }

    if (bucket_metadata->labels().empty()) {
      std::cout << "The bucket " << bucket_name << " has no labels set."
                << std::endl;
      return;
    }

    std::cout << "The labels for bucket " << bucket_name << " are:";
    for (auto const& kv : bucket_metadata->labels()) {
      std::cout << "\n  " << kv.first << ": " << kv.second;
    }
    std::cout << std::endl;
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string label_key) {
    StatusOr<gcs::BucketMetadata> updated_metadata = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().ResetLabel(label_key));

    if (!updated_metadata) {
      std::cerr << "Error resetting label " << label_key << " on bucket "
                << bucket_name << ", status=" << updated_metadata.status()
                << std::endl;
      return;
    }

    std::cout << "Successfully reset label " << label_key << " on bucket  "
              << updated_metadata->name() << ".";
    if (updated_metadata->labels().empty()) {
      std::cout << " The bucket now has no labels." << std::endl;
      return;
    }
    std::cout << " The bucket labels are now:";
    for (auto const& kv : updated_metadata->labels()) {
      std::cout << "\n  " << kv.first << ": " << kv.second;
    }
    std::cout << std::endl;
  }
  //! [remove bucket label] [END storage_remove_bucket_label]
  (std::move(client), bucket_name, label_key);
}

void GetBilling(google::cloud::storage::Client client, int& argc,
                char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-billing <bucket-name>"};
  }
  auto bucket_name = ConsumeArg(argc, argv);
  //! [get billing] [START storage_get_requester_pays_status]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!bucket_metadata) {
      std::cerr << "Error reading metadata for bucket " << bucket_name
                << ", status=" << bucket_metadata.status() << std::endl;
      return;
    }

    if (!bucket_metadata->has_billing()) {
      std::cout
          << "The bucket " << bucket_metadata->name() << " does not have a"
          << " billing configuration. The default applies, i.e., the project"
          << " that owns the bucket pays for the requests." << std::endl;
      return;
    }

    if (bucket_metadata->billing().requester_pays) {
      std::cout
          << "The bucket " << bucket_metadata->name()
          << " is configured to charge the calling project for the requests."
          << std::endl;
    } else {
      std::cout << "The bucket " << bucket_metadata->name()
                << " is configured to charge the project that owns the bucket "
                   "for the requests."
                << std::endl;
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetBilling(gcs::BucketBilling{true}));

    if (!bucket_metadata) {
      std::cerr << "Error setting the billing configuration for bucket "
                << bucket_name << ", status=" << bucket_metadata.status()
                << std::endl;
      return;
    }

    std::cout << "Billing configuration for bucket " << bucket_metadata->name()
              << " is updated. The bucket now";
    if (!bucket_metadata->has_billing()) {
      std::cout << " has no billing configuration." << std::endl;
    } else if (bucket_metadata->billing().requester_pays) {
      std::cout << " is configured to charge the caller for requests"
                << std::endl;
    } else {
      std::cout << " is configured to charge the project that owns the bucket"
                << " for requests." << std::endl;
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::string project_id) {
    StatusOr<gcs::BucketMetadata> bucket_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetBilling(gcs::BucketBilling{false}),
        gcs::UserProject(project_id));

    if (!bucket_metadata) {
      std::cerr << "Error disabling billing configuration for bucket "
                << bucket_name << ", status=" << bucket_metadata.status()
                << std::endl;
      return;
    }

    std::cout << "Billing configuration for bucket " << bucket_name
              << " is updated. The bucket now";
    if (!bucket_metadata->has_billing()) {
      std::cout << " has no billing configuration." << std::endl;
    } else if (bucket_metadata->billing().requester_pays) {
      std::cout << " is configured to charge the caller for requests"
                << std::endl;
    } else {
      std::cout << " is configured to charge the project that owns the bucket"
                << " for requests." << std::endl;
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
  using google::cloud::StatusOr;
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
      std::cerr << "Error while writing to object " << object_name
                << " in bucket " << bucket_name
                << ", status=" << metadata.status() << std::endl;
      return;
    }
    std::cout << "Successfully wrote to object " << metadata->name()
              << " its size is: " << metadata->size()
              << "\nFull metadata: " << *metadata << std::endl;
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

void GetServiceAccount(google::cloud::storage::Client client, int& argc,
                       char* argv[]) {
  if (argc != 1) {
    throw Usage{"get-service-account"};
  }
  //! [get service account] [START storage_get_service_account]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client) {
    StatusOr<gcs::ServiceAccount> service_account_details =
        client.GetServiceAccount();

    if (!service_account_details) {
      std::cerr << "Error getting service account details, status="
                << service_account_details.status() << std::endl;
      return;
    }
    std::cout << "The service account details are " << *service_account_details
              << std::endl;
  }
  //! [get service account] [END storage_get_service_account]
  (std::move(client));
}

void GetServiceAccountForProject(google::cloud::storage::Client client,
                                 int& argc, char* argv[]) {
  if (argc != 2) {
    throw Usage{"get-service-account-for-project <project-id>"};
  }
  auto project_id = ConsumeArg(argc, argv);
  //! [get service account for project]
  // [START storage_get_service_account_for_project]
  namespace gcs = google::cloud::storage;
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string project_id) {
    StatusOr<gcs::ServiceAccount> service_account_details =
        client.GetServiceAccountForProject(project_id);

    if (!service_account_details) {
      std::cerr << "Error getting service account details, status="
                << service_account_details.status() << std::endl;
      return;
    }

    std::cout << "The service account details for project " << project_id
              << " are " << *service_account_details << std::endl;
  }
  // [END storage_get_service_account_for_project]
  //! [get service account for project]
  (std::move(client), project_id);
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!bucket_metadata) {
      std::cerr << "Error fetching bucket metadata for bucket " << bucket_name
                << ", status=" << bucket_metadata.status() << std::endl;
      return;
    }

    std::cout << "The default event-based hold for objects in bucket "
              << bucket_metadata->name() << " is "
              << (bucket_metadata->default_event_based_hold() ? "enabled"
                                                              : "disabled")
              << std::endl;
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      std::cerr << "Error fetching bucket metadata for bucket " << bucket_name
                << ", status=" << original.status() << std::endl;
      return;
    }

    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetDefaultEventBasedHold(true),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      std::cerr << "Error setting default event based hold in bucket "
                << original->name() << ", status=" << original.status()
                << std::endl;
      return;
    }

    std::cout << "The default event-based hold for objects in bucket "
              << bucket_name << " is "
              << (patched_metadata->default_event_based_hold() ? "enabled"
                                                               : "disabled")
              << std::endl;
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      std::cerr << "Error fetching bucket metadata for bucket " << bucket_name
                << ", status=" << original.status() << std::endl;
      return;
    }

    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetDefaultEventBasedHold(false),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      std::cerr << "Error disabling default event based hold in bucket "
                << original->name() << ", status=" << original.status()
                << std::endl;
      return;
    }

    std::cout << "The default event-based hold for objects in bucket "
              << bucket_name << " is "
              << (patched_metadata->default_event_based_hold() ? "enabled"
                                                               : "disabled")
              << std::endl;
  }
  // [END storage_disable_default_event_based_hold]
  //! [disable default event based hold]
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);

    if (!bucket_metadata) {
      std::cerr << "Error fetching the metadata for bucket " << bucket_name
                << ", status=" << bucket_metadata.status() << std::endl;
      return;
    }

    if (!bucket_metadata->has_retention_policy()) {
      std::cout << "The bucket " << bucket_metadata->name()
                << " does not have a retention policy set." << std::endl;
      return;
    }

    std::cout << "The bucket " << bucket_metadata->name()
              << " retention policy is set to "
              << bucket_metadata->retention_policy() << std::endl;
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name, std::chrono::seconds period) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      std::cerr << "Error fetching the metadata for bucket " << bucket_name
                << ", status=" << original.status() << std::endl;
      return;
    }

    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetRetentionPolicy(period),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      std::cerr << "Error setting the retention policy on bucket "
                << original->name() << ", status=" << patched_metadata.status()
                << std::endl;
      return;
    }

    if (!patched_metadata->has_retention_policy()) {
      std::cout << "The bucket " << patched_metadata->name()
                << " does not have a retention policy set." << std::endl;
      return;
    }

    std::cout << "The bucket " << patched_metadata->name()
              << " retention policy is set to "
              << patched_metadata->retention_policy() << std::endl;
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      std::cerr << "Error fetching the metadata for bucket " << bucket_name
                << ", status=" << original.status() << std::endl;
      return;
    }

    StatusOr<gcs::BucketMetadata> patched_metadata = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().ResetRetentionPolicy(),
        gcs::IfMetagenerationMatch(original->metageneration()));

    if (!patched_metadata) {
      std::cerr << "Error removing the retention policy on bucket "
                << original->name() << ", status=" << patched_metadata.status()
                << std::endl;
      return;
    }

    if (!patched_metadata->has_retention_policy()) {
      std::cout << "The bucket " << patched_metadata->name()
                << " does not have a retention policy set." << std::endl;
      return;
    }

    std::cout << "The bucket " << patched_metadata->name()
              << " retention policy is set to "
              << patched_metadata->retention_policy()
              << ". This is unexpected, maybe a concurrent change by another"
              << " application?" << std::endl;
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
  using google::cloud::StatusOr;
  [](gcs::Client client, std::string bucket_name) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);

    if (!original) {
      std::cerr << "Error fetching the metadata for bucket " << bucket_name
                << ", status=" << original.status() << std::endl;
      return;
    }

    StatusOr<gcs::BucketMetadata> updated_metadata =
        client.LockBucketRetentionPolicy(bucket_name,
                                         original->metageneration());

    if (!updated_metadata) {
      std::cerr << "Error locking bucket retention policy for bucket "
                << bucket_name << ", status=" << updated_metadata.status()
                << std::endl;
      return;
    }

    if (!updated_metadata->has_retention_policy()) {
      std::cerr << "The bucket " << updated_metadata->name()
                << " does not have a retention policy, even though the"
                << " operation to set it was successful.\n"
                << "This is unexpected, and may indicate that another"
                << " application has modified the bucket concurrently."
                << std::endl;
      return;
    }

    std::cout << "Retention policy successfully locked for bucket "
              << updated_metadata->name() << "\nNew retention policy is: "
              << updated_metadata->retention_policy()
              << "\nFull metadata: " << *updated_metadata << std::endl;
  }
  // [END storage_lock_retention_policy]
  //! [lock retention policy]
  (std::move(client), bucket_name);
}
}  // anonymous namespace

int main(int argc, char* argv[]) try {
  // Create a client to communicate with Google Cloud Storage.
  //! [create client]
  google::cloud::StatusOr<google::cloud::storage::Client> client =
      google::cloud::storage::Client::CreateDefaultClient();
  if (!client) {
    std::cerr << "Failed to create Storage Client, status=" << client.status()
              << std::endl;
    return 1;
  }
  //! [create client]

  using CommandType =
      std::function<void(google::cloud::storage::Client, int&, char* [])>;
  std::map<std::string, CommandType> commands = {
      {"list-buckets", &ListBuckets},
      {"list-buckets-for-project", &ListBucketsForProject},
      {"create-bucket", &CreateBucket},
      {"create-bucket-for-project", &CreateBucketForProject},
      {"get-bucket-metadata", &GetBucketMetadata},
      {"delete-bucket", &DeleteBucket},
      {"change-default-storage-class", &ChangeDefaultStorageClass},
      {"patch-bucket-storage-class", &PatchBucketStorageClass},
      {"patch-bucket-storage-class-with-builder",
       &PatchBucketStorageClassWithBuilder},
      {"add-bucket-default-kms-key", &AddBucketDefaultKmsKey},
      {"get-bucket-default-kms-key", &GetBucketDefaultKmsKey},
      {"remove-bucket-default-kms-key", &RemoveBucketDefaultKmsKey},
      {"add-bucket-label", &AddBucketLabel},
      {"get-bucket-labels", &GetBucketLabels},
      {"remove-bucket-label", &RemoveBucketLabel},
      {"get-billing", &GetBilling},
      {"enable-requester-pays", &EnableRequesterPays},
      {"disable-requester-pays", &DisableRequesterPays},
      {"write-object-requester-pays", &WriteObjectRequesterPays},
      {"read-object-requester-pays", &ReadObjectRequesterPays},
      {"get-service-account", &GetServiceAccount},
      {"get-service-account-for-project", &GetServiceAccountForProject},
      {"get-default-event-based-hold", &GetDefaultEventBasedHold},
      {"enable-default-event-based-hold", &EnableDefaultEventBasedHold},
      {"disable-default-event-based-hold", &DisableDefaultEventBasedHold},
      {"get-retention-policy", &GetRetentionPolicy},
      {"set-retention-policy", &SetRetentionPolicy},
      {"remove-retention-policy", &RemoveRetentionPolicy},
      {"lock-retention-policy", &LockRetentionPolicy},
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
  std::cerr << "Standard C++ exception raised: " << ex.what() << std::endl;
  return 1;
}

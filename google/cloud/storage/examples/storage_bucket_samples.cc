// Copyright 2018 Google LLC
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
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void ListBuckets(google::cloud::storage::Client client,
                 std::vector<std::string> const& /*argv*/) {
  //! [list buckets] [START storage_list_buckets]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client) {
    int count = 0;
    gcs::ListBucketsReader bucket_list = client.ListBuckets();
    for (auto&& bucket_metadata : bucket_list) {
      if (!bucket_metadata) throw std::move(bucket_metadata).status();

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

void ListBucketsForProject(google::cloud::storage::Client client,
                           std::vector<std::string> const& argv) {
  //! [list buckets for project]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& project_id) {
    int count = 0;
    for (auto&& bucket_metadata : client.ListBucketsForProject(project_id)) {
      if (!bucket_metadata) throw std::move(bucket_metadata).status();

      std::cout << bucket_metadata->name() << "\n";
      ++count;
    }

    if (count == 0) {
      std::cout << "No buckets in project " << project_id << "\n";
    }
  }
  //! [list buckets for project]
  (std::move(client), argv.at(0));
}

void CreateBucket(google::cloud::storage::Client client,
                  std::vector<std::string> const& argv) {
  //! [create bucket] [START storage_create_bucket]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.CreateBucket(bucket_name, gcs::BucketMetadata());
    if (!bucket_metadata) throw std::move(bucket_metadata).status();

    std::cout << "Bucket " << bucket_metadata->name() << " created."
              << "\nFull Metadata: " << *bucket_metadata << "\n";
  }
  //! [create bucket] [END storage_create_bucket]
  (std::move(client), argv.at(0));
}

void CreateBucketForProject(google::cloud::storage::Client client,
                            std::vector<std::string> const& argv) {
  //! [create bucket for project]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& project_id) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.CreateBucketForProject(bucket_name, project_id,
                                      gcs::BucketMetadata{});
    if (!bucket_metadata) throw std::move(bucket_metadata).status();

    std::cout << "Bucket " << bucket_metadata->name() << " created for project "
              << project_id << " [" << bucket_metadata->project_number() << "]"
              << "\nFull Metadata: " << *bucket_metadata << "\n";
  }
  //! [create bucket for project]
  (std::move(client), argv.at(0), argv.at(1));
}

void CreateBucketWithStorageClassLocation(
    google::cloud::storage::Client client,
    std::vector<std::string> const& argv) {
  //! [create bucket class location]
  // [START storage_create_bucket_class_location]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& storage_class, std::string const& location) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.CreateBucket(bucket_name, gcs::BucketMetadata()
                                             .set_storage_class(storage_class)
                                             .set_location(location));
    if (!bucket_metadata) throw std::move(bucket_metadata).status();

    std::cout << "Bucket " << bucket_metadata->name() << " created."
              << "\nFull Metadata: " << *bucket_metadata << "\n";
  }
  // [END storage_create_bucket_class_location]
  //! [create bucket class location]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CreateBucketDualRegion(google::cloud::storage::Client client,
                            std::vector<std::string> const& argv) {
  // [START storage_create_bucket_dual_region]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& region_a, std::string const& region_b) {
    auto metadata = client.CreateBucket(
        bucket_name,
        gcs::BucketMetadata().set_custom_placement_config(
            gcs::BucketCustomPlacementConfig{{region_a, region_b}}));
    if (!metadata) throw std::move(metadata).status();

    std::cout << "Bucket " << metadata->name() << " created."
              << "\nFull Metadata: " << *metadata << "\n";
  }
  // [END storage_create_bucket_dual_region]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void CreateBucketWithHNS(google::cloud::storage::Client client,
                         std::vector<std::string> const& argv) {
  // [START storage_create_bucket_hierarchical_namespace]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    auto metadata = client.CreateBucket(
        bucket_name,
        gcs::BucketMetadata()
            .set_hierarchical_namespace(gcs::BucketHierarchicalNamespace{true})
            .set_iam_configuration(gcs::BucketIamConfiguration{
                gcs::UniformBucketLevelAccess{true, {}}, absl::nullopt}));
    if (!metadata) throw std::move(metadata).status();

    std::cout << "Bucket " << metadata->name() << " created."
              << "\nFull Metadata: " << *metadata << "\n";
  }
  // [END storage_create_bucket_hierarchical_namespace]
  (std::move(client), argv.at(0));
}

void GetBucketMetadata(google::cloud::storage::Client client,
                       std::vector<std::string> const& argv) {
  //! [get bucket metadata]
  // [START storage_get_bucket_metadata]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);
    if (!bucket_metadata) throw std::move(bucket_metadata).status();

    std::cout << "The metadata for bucket " << bucket_metadata->name() << " is "
              << *bucket_metadata << "\n";
  }
  // [END storage_get_bucket_metadata]
  //! [get bucket metadata]
  (std::move(client), argv.at(0));
}

void DeleteBucket(google::cloud::storage::Client client,
                  std::vector<std::string> const& argv) {
  //! [delete bucket] [START storage_delete_bucket]
  namespace gcs = ::google::cloud::storage;
  [](gcs::Client client, std::string const& bucket_name) {
    google::cloud::Status status = client.DeleteBucket(bucket_name);
    if (!status.ok()) throw std::runtime_error(status.message());

    std::cout << "The bucket " << bucket_name << " was deleted successfully.\n";
  }
  //! [delete bucket] [END storage_delete_bucket]
  (std::move(client), argv.at(0));
}

void ChangeDefaultStorageClass(google::cloud::storage::Client client,
                               std::vector<std::string> const& argv) {
  //! [update bucket]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& storage_class) {
    StatusOr<gcs::BucketMetadata> meta = client.GetBucketMetadata(bucket_name);
    if (!meta) throw std::move(meta).status();

    meta->set_storage_class(storage_class);
    StatusOr<gcs::BucketMetadata> updated =
        client.UpdateBucket(bucket_name, *meta);
    if (!updated) throw std::move(updated).status();

    std::cout << "Updated the storage class in " << updated->name() << " to "
              << updated->storage_class() << "."
              << "\nFull metadata:" << *updated << "\n";
  }
  //! [update bucket]
  (std::move(client), argv.at(0), argv.at(1));
}

void PatchBucketStorageClass(google::cloud::storage::Client client,
                             std::vector<std::string> const& argv) {
  //! [patch bucket storage class] [START storage_change_default_storage_class]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& storage_class) {
    StatusOr<gcs::BucketMetadata> original =
        client.GetBucketMetadata(bucket_name);
    if (!original) throw std::move(original).status();

    gcs::BucketMetadata desired = *original;
    desired.set_storage_class(storage_class);

    StatusOr<gcs::BucketMetadata> patched =
        client.PatchBucket(bucket_name, *original, desired);
    if (!patched) throw std::move(patched).status();

    std::cout << "Storage class for bucket " << patched->name()
              << " has been patched to " << patched->storage_class() << "."
              << "\nFull metadata: " << *patched << "\n";
  }
  //! [patch bucket storage class] [END storage_change_default_storage_class]
  (std::move(client), argv.at(0), argv.at(1));
}

void PatchBucketStorageClassWithBuilder(google::cloud::storage::Client client,
                                        std::vector<std::string> const& argv) {
  //! [patch bucket storage class with builder]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& storage_class) {
    StatusOr<gcs::BucketMetadata> patched = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetStorageClass(storage_class));
    if (!patched) throw std::move(patched).status();

    std::cout << "Storage class for bucket " << patched->name()
              << " has been patched to " << patched->storage_class() << "."
              << "\nFull metadata: " << *patched << "\n";
  }
  //! [patch bucket storage class with builder]
  (std::move(client), argv.at(0), argv.at(1));
}

void GetBucketClassAndLocation(google::cloud::storage::Client client,
                               std::vector<std::string> const& argv) {
  // [START storage_get_bucket_class_and_location]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);
    if (!bucket_metadata) throw std::move(bucket_metadata).status();

    std::cout << "Bucket " << bucket_metadata->name()
              << " default storage class is "
              << bucket_metadata->storage_class() << ", and the location is "
              << bucket_metadata->location() << '\n';
  }
  // [END storage_get_bucket_class_and_location]
  (std::move(client), argv.at(0));
}

void EnableUniformBucketLevelAccess(google::cloud::storage::Client client,
                                    std::vector<std::string> const& argv) {
  //! [enable uniform bucket level access]
  // [START storage_enable_uniform_bucket_level_access]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    gcs::BucketIamConfiguration configuration;
    configuration.uniform_bucket_level_access =
        gcs::UniformBucketLevelAccess{true, {}};
    StatusOr<gcs::BucketMetadata> updated = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().SetIamConfiguration(
                         std::move(configuration)));
    if (!updated) throw std::move(updated).status();

    std::cout << "Successfully enabled Uniform Bucket Level Access on bucket "
              << updated->name() << "\n";
  }
  // [END storage_enable_uniform_bucket_level_access]
  //! [enable uniform bucket level access]
  (std::move(client), argv.at(0));
}

void DisableUniformBucketLevelAccess(google::cloud::storage::Client client,
                                     std::vector<std::string> const& argv) {
  //! [disable uniform bucket level access]
  // [START storage_disable_uniform_bucket_level_access]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    gcs::BucketIamConfiguration configuration;
    configuration.uniform_bucket_level_access =
        gcs::UniformBucketLevelAccess{false, {}};
    StatusOr<gcs::BucketMetadata> updated = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().SetIamConfiguration(
                         std::move(configuration)));
    if (!updated) throw std::move(updated).status();

    std::cout << "Successfully disabled Uniform Bucket Level Access on bucket "
              << updated->name() << "\n";
  }
  // [END storage_disable_uniform_bucket_level_access]
  //! [disable uniform bucket level access]
  (std::move(client), argv.at(0));
}

void GetUniformBucketLevelAccess(google::cloud::storage::Client client,
                                 std::vector<std::string> const& argv) {
  //! [get uniform bucket level access]
  // [START storage_get_uniform_bucket_level_access]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);
    if (!bucket_metadata) throw std::move(bucket_metadata).status();

    if (bucket_metadata->has_iam_configuration() &&
        bucket_metadata->iam_configuration()
            .uniform_bucket_level_access.has_value()) {
      gcs::UniformBucketLevelAccess uniform_bucket_level_access =
          *bucket_metadata->iam_configuration().uniform_bucket_level_access;

      std::cout << "Uniform Bucket Level Access is enabled for "
                << bucket_metadata->name() << "\n";
      std::cout << "Bucket will be locked on " << uniform_bucket_level_access
                << "\n";
    } else {
      std::cout << "Uniform Bucket Level Access is not enabled for "
                << bucket_metadata->name() << "\n";
    }
  }
  // [END storage_get_uniform_bucket_level_access]
  //! [get uniform bucket level access]
  (std::move(client), argv.at(0));
}

void SetPublicAccessPreventionEnforced(google::cloud::storage::Client client,
                                       std::vector<std::string> const& argv) {
  // [START storage_set_public_access_prevention_enforced]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    gcs::BucketIamConfiguration configuration;
    configuration.public_access_prevention =
        gcs::PublicAccessPreventionEnforced();
    StatusOr<gcs::BucketMetadata> updated = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().SetIamConfiguration(
                         std::move(configuration)));
    if (!updated) throw std::move(updated).status();

    std::cout << "Public Access Prevention is set to 'enforced' for "
              << updated->name() << "\n";
  }
  // [END storage_set_public_access_prevention_enforced]
  (std::move(client), argv.at(0));
}

void SetPublicAccessPreventionInherited(google::cloud::storage::Client client,
                                        std::vector<std::string> const& argv) {
  // [START storage_set_public_access_prevention_unspecified]
  // [START storage_set_public_access_prevention_inherited]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    gcs::BucketIamConfiguration configuration;
    configuration.public_access_prevention =
        gcs::PublicAccessPreventionInherited();
    auto updated = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().SetIamConfiguration(
                         std::move(configuration)));
    if (!updated) throw std::move(updated).status();

    std::cout << "Public Access Prevention is set to 'inherited' for "
              << updated->name() << "\n";
  }
  // [END storage_set_public_access_prevention_unspecified]
  // [END storage_set_public_access_prevention_inherited]
  (std::move(client), argv.at(0));
}

void GetPublicAccessPrevention(google::cloud::storage::Client client,
                               std::vector<std::string> const& argv) {
  // [START storage_get_public_access_prevention]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name);
    if (!bucket_metadata) throw std::move(bucket_metadata).status();

    if (bucket_metadata->has_iam_configuration() &&
        bucket_metadata->iam_configuration()
            .public_access_prevention.has_value()) {
      std::cout
          << "Public Access Prevention is "
          << *bucket_metadata->iam_configuration().public_access_prevention
          << " for bucket " << bucket_metadata->name() << "\n";
    } else {
      std::cout << "Public Access Prevention is not set for "
                << bucket_metadata->name() << "\n";
    }
  }
  // [END storage_get_public_access_prevention]
  (std::move(client), argv.at(0));
}

void CreateBucketAsyncTurbo(google::cloud::storage::Client client,
                            std::vector<std::string> const& argv) {
  // [START storage_create_bucket_turbo_replication]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    auto bucket =
        client.CreateBucket(bucket_name, gcs::BucketMetadata()
                                             .set_rpo(gcs::RpoAsyncTurbo())
                                             .set_location("NAM4"));
    if (!bucket) throw std::move(bucket).status();

    std::cout << "Created bucket " << bucket->name() << " with RPO set to "
              << bucket->rpo() << " in " << bucket->location() << ".\n";
  }
  // // [END storage_create_bucket_turbo_replication]
  (std::move(client), argv.at(0));
}

void SetRpoAsyncTurbo(google::cloud::storage::Client client,
                      std::vector<std::string> const& argv) {
  // [START storage_set_rpo_async_turbo]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    auto updated = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetRpo(gcs::RpoAsyncTurbo()));
    if (!updated) throw std::move(updated).status();

    std::cout << "RPO is set to 'ASYNC_TURBO' for " << updated->name() << "\n";
  }
  // [END storage_set_rpo_async_turbo]
  (std::move(client), argv.at(0));
}

void SetRpoDefault(google::cloud::storage::Client client,
                   std::vector<std::string> const& argv) {
  // [START storage_set_rpo_default]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    auto updated = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetRpo(gcs::RpoDefault()));
    if (!updated) throw std::move(updated).status();

    std::cout << "RPO is set to 'default' for " << updated->name() << "\n";
  }
  // [END storage_set_rpo_default]
  (std::move(client), argv.at(0));
}

void GetRpo(google::cloud::storage::Client client,
            std::vector<std::string> const& argv) {
  // [START storage_get_rpo]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    auto metadata = client.GetBucketMetadata(bucket_name);
    if (!metadata) throw std::move(metadata).status();

    std::cout << "RPO is " << metadata->rpo() << " for bucket "
              << metadata->name() << "\n";
  }
  // [END storage_get_rpo]
  (std::move(client), argv.at(0));
}

void AddBucketLabel(google::cloud::storage::Client client,
                    std::vector<std::string> const& argv) {
  //! [add bucket label] [START storage_add_bucket_label]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& label_key, std::string const& label_value) {
    StatusOr<gcs::BucketMetadata> updated = client.PatchBucket(
        bucket_name,
        gcs::BucketMetadataPatchBuilder().SetLabel(label_key, label_value));
    if (!updated) throw std::move(updated).status();

    std::cout << "Successfully set label " << label_key << " to " << label_value
              << " on bucket  " << updated->name() << ".";
    std::cout << " The bucket labels are now:";
    for (auto const& kv : updated->labels()) {
      std::cout << "\n  " << kv.first << ": " << kv.second;
    }
    std::cout << "\n";
  }
  //! [add bucket label] [END storage_add_bucket_label]
  (std::move(client), argv.at(0), argv.at(1), argv.at(2));
}

void GetBucketLabels(google::cloud::storage::Client client,
                     std::vector<std::string> const& argv) {
  //! [get bucket labels] [START storage_get_bucket_labels]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name) {
    StatusOr<gcs::BucketMetadata> bucket_metadata =
        client.GetBucketMetadata(bucket_name, gcs::Fields("labels"));
    if (!bucket_metadata) throw std::move(bucket_metadata).status();

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
  (std::move(client), argv.at(0));
}

void RemoveBucketLabel(google::cloud::storage::Client client,
                       std::vector<std::string> const& argv) {
  //! [remove bucket label] [START storage_remove_bucket_label]
  namespace gcs = ::google::cloud::storage;
  using ::google::cloud::StatusOr;
  [](gcs::Client client, std::string const& bucket_name,
     std::string const& label_key) {
    StatusOr<gcs::BucketMetadata> updated = client.PatchBucket(
        bucket_name, gcs::BucketMetadataPatchBuilder().ResetLabel(label_key));
    if (!updated) throw std::move(updated).status();

    std::cout << "Successfully reset label " << label_key << " on bucket  "
              << updated->name() << ".";
    if (updated->labels().empty()) {
      std::cout << " The bucket now has no labels.\n";
      return;
    }
    std::cout << " The bucket labels are now:";
    for (auto const& kv : updated->labels()) {
      std::cout << "\n  " << kv.first << ": " << kv.second;
    }
    std::cout << "\n";
  }
  //! [remove bucket label] [END storage_remove_bucket_label]
  (std::move(client), argv.at(0), argv.at(1));
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
  auto const rpo_bucket_name = examples::MakeRandomBucketName(generator);
  auto const dual_bucket_name = examples::MakeRandomBucketName(generator);
  auto client = gcs::Client(examples::CreateBucketOptions());

  // This is the only example that cleans up stale buckets. The examples run in
  // parallel (within a build and across the builds), having multiple examples
  // doing the same cleanup is probably more trouble than it is worth.
  auto const create_time_limit =
      std::chrono::system_clock::now() - std::chrono::hours(48);
  std::cout << "\nRemoving stale buckets for examples" << std::endl;
  examples::RemoveStaleBuckets(client, examples::BucketPrefix(),
                               create_time_limit);

  std::cout << "\nRunning ListBucketsForProject() example" << std::endl;
  ListBucketsForProject(client, {project_id});

  std::cout << "\nRunning CreateBucketForProject() example" << std::endl;
  CreateBucketForProject(client, {bucket_name, project_id});

  std::cout << "\nRunning GetBucketMetadata() example [1]" << std::endl;
  GetBucketMetadata(client, {bucket_name});

  std::cout << "\nRunning ChangeDefaultStorageClass() example" << std::endl;
  ChangeDefaultStorageClass(client, {bucket_name, "NEARLINE"});

  std::cout << "\nRunning PatchBucketStorageClass() example" << std::endl;
  PatchBucketStorageClass(client, {bucket_name, "STANDARD"});

  std::cout << "\nRunning PatchBucketStorageClassWithBuilder() example"
            << std::endl;
  PatchBucketStorageClassWithBuilder(client, {bucket_name, "COLDLINE"});

  std::cout << "\nRunning GetBucketClassAndLocation() example" << std::endl;
  GetBucketClassAndLocation(client, {bucket_name});

  std::cout << "\nRunning EnableUniformBucketLevelAccess() example"
            << std::endl;
  EnableUniformBucketLevelAccess(client, {bucket_name});

  std::cout << "\nRunning DisableUniformBucketLevelAccess() example"
            << std::endl;
  DisableUniformBucketLevelAccess(client, {bucket_name});

  std::cout << "\nRunning GetUniformBucketLevelAccess() example" << std::endl;
  GetUniformBucketLevelAccess(client, {bucket_name});

  std::cout << "\nRunning SetPublicAccessPreventionEnforced() example"
            << std::endl;
  SetPublicAccessPreventionEnforced(client, {bucket_name});

  std::cout << "\nRunning SetPublicAccessPreventionInherited() example"
            << std::endl;
  SetPublicAccessPreventionInherited(client, {bucket_name});

  std::cout << "\nRunning GetPublicAccessPrevention() example" << std::endl;
  GetPublicAccessPrevention(client, {bucket_name});

  std::cout << "\nRunning CreateBucketAsyncTurbo() example" << std::endl;
  CreateBucketAsyncTurbo(client, {rpo_bucket_name});

  std::cout << "\nRunning GetRpo() example" << std::endl;
  GetRpo(client, {rpo_bucket_name});

  std::cout << "\nRunning SetRpoDefault() example" << std::endl;
  SetRpoDefault(client, {rpo_bucket_name});

  std::cout << "\nRunning SetRpoAsyncTurbo() example" << std::endl;
  SetRpoAsyncTurbo(client, {rpo_bucket_name});

  std::cout << "\nRunning AddBucketLabel() example" << std::endl;
  AddBucketLabel(client, {bucket_name, "test-label", "test-label-value"});

  std::cout << "\nRunning GetBucketLabels() example [1]" << std::endl;
  GetBucketLabels(client, {bucket_name});

  std::cout << "\nRunning RemoveBucketLabel() example" << std::endl;
  RemoveBucketLabel(client, {bucket_name, "test-label"});

  std::cout << "\nRunning GetBucketLabels() example [2]" << std::endl;
  GetBucketLabels(client, {bucket_name});

  std::cout << "\nRunning DeleteBucket() example [3]" << std::endl;
  DeleteBucket(client, {bucket_name});

  std::cout << "\nRunning ListBuckets() example" << std::endl;
  ListBuckets(client, {});

  std::cout << "\nRunning CreateBucket() example" << std::endl;
  CreateBucket(client, {bucket_name});

  std::cout << "\nRunning GetBucketMetadata() example [2]" << std::endl;
  GetBucketMetadata(client, {bucket_name});

  std::cout << "\nRunning DeleteBucket() example [2]" << std::endl;
  DeleteBucket(client, {bucket_name});

  std::cout << "\nRunning CreateBucketDualRegion example" << std::endl;
  CreateBucketDualRegion(client, {dual_bucket_name, "us-east4", "us-central1"});

  (void)client.DeleteBucket(dual_bucket_name);

  std::cout << "\nRunning CreateBucketWithStorageClassLocation() example"
            << std::endl;
  CreateBucketWithStorageClassLocation(client, {bucket_name, "STANDARD", "US"});

  auto const hns_bucket_name = examples::MakeRandomBucketName(generator);
  std::cout << "\nRunning CreateBucketWithHNS() example" << std::endl;
  CreateBucketWithHNS(client, {hns_bucket_name});
  (void)client.DeleteBucket(hns_bucket_name);

  std::cout << "\nRunning DeleteBucket() example [3]" << std::endl;
  DeleteBucket(client, {bucket_name});

  std::cout << "\nRunning DeleteBucket() example [4]" << std::endl;
  DeleteBucket(client, {rpo_bucket_name});
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
      examples::CreateCommandEntry("list-buckets", {}, ListBuckets),
      examples::CreateCommandEntry("list-buckets-for-project", {"<project-id>"},
                                   ListBucketsForProject),
      make_entry("create-bucket", {}, CreateBucket),
      make_entry("create-bucket-for-project", {"<project-id>"},
                 CreateBucketForProject),
      make_entry("create-bucket-with-storage-class-location",
                 {"<storage-class>", "<location>"},
                 CreateBucketWithStorageClassLocation),
      make_entry("create-bucket-dual-region", {"<region-a>", "<region-b>"},
                 CreateBucketDualRegion),
      make_entry("create-bucket-with-hns", {}, CreateBucketWithHNS),
      make_entry("get-bucket-metadata", {}, GetBucketMetadata),
      make_entry("delete-bucket", {}, DeleteBucket),
      make_entry("change-default-storage-class", {"<new-class>"},
                 ChangeDefaultStorageClass),
      make_entry("patch-bucket-storage-class", {"<storage-class>"},
                 PatchBucketStorageClass),
      make_entry("patch-bucket-storage-classwith-builder", {"<storage-class>"},
                 PatchBucketStorageClassWithBuilder),
      make_entry("get-bucket-class-and-location", {},
                 GetBucketClassAndLocation),
      make_entry("enable-uniform-bucket-level-access", {},
                 EnableUniformBucketLevelAccess),
      make_entry("disable-uniform-bucket-level-access", {},
                 DisableUniformBucketLevelAccess),
      make_entry("get-uniform-bucket-level-access", {},
                 GetUniformBucketLevelAccess),
      make_entry("set-public-access-prevention-inherited", {},
                 SetPublicAccessPreventionInherited),
      make_entry("set-public-access-prevention-enforced", {},
                 SetPublicAccessPreventionEnforced),
      make_entry("get-public-access-prevention", {}, GetPublicAccessPrevention),
      make_entry("create-bucket-async-turbo", {}, CreateBucketAsyncTurbo),
      make_entry("set-rpo-default", {}, SetRpoDefault),
      make_entry("set-rpo-async-turn", {}, SetRpoAsyncTurbo),
      make_entry("get-rpo", {}, GetRpo),
      make_entry("add-bucket-label", {"<label-key>", "<label-value>"},
                 AddBucketLabel),
      make_entry("get-bucket-labels", {}, GetBucketLabels),
      make_entry("remove-bucket-label", {"<label-key>"}, RemoveBucketLabel),
      {"auto", RunAll},
  });
  return example.Run(argc, argv);
}

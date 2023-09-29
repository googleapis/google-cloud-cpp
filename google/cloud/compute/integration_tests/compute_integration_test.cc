// Copyright 2023 Google LLC
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

#include "google/cloud/compute/disks/v1/disks_client.h"
#include "google/cloud/compute/instances/v1/instances_client.h"
#include "google/cloud/internal/absl_str_cat_quiet.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/parse_rfc3339.h"
#include "google/cloud/internal/random.h"
#include "google/cloud/testing_util/integration_test.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <chrono>
#include <thread>

namespace google {
namespace cloud {
namespace compute_v1 {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Property;

class ComputeIntegrationTest
    : public ::google::cloud::testing_util::IntegrationTest {
 protected:
  void SetUp() override {
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id_.empty());
    zone_ = google::cloud::internal::GetEnv("GOOGLE_CLOUD_CPP_TEST_ZONE")
                .value_or("");
    ASSERT_FALSE(zone_.empty());
    generator_ = internal::MakeDefaultPRNG();
  }

  std::string CreateRandomName(std::string const& prefix) {
    return absl::StrCat(
        prefix, internal::Sample(generator_, 8,
                                 "abcdefghijklmnopqrstuvwxyz0123456789"));
  }

  internal::DefaultPRNG generator_;
  std::string project_id_;
  std::string zone_;
};

TEST_F(ComputeIntegrationTest, DeleteUnknownDisk) {
  namespace disks = ::google::cloud::compute_disks_v1;
  auto client = disks::DisksClient(disks::MakeDisksConnectionRest());

  StatusOr<google::cloud::cpp::compute::v1::Operation> delete_disk =
      client.DeleteDisk(project_id_, zone_, "not_found").get();
  EXPECT_THAT(
      delete_disk,
      StatusIs(StatusCode::kNotFound,
               HasSubstr("The resource "
                         "'projects/cloud-cpp-testing-resources/zones/"
                         "us-central1-a/disks/not_found' was not found")));
}

TEST_F(ComputeIntegrationTest, CreateDisks) {
  namespace disks = ::google::cloud::compute_disks_v1;
  auto client = disks::DisksClient(disks::MakeDisksConnectionRest());

  google::cloud::cpp::compute::v1::Disk disk;
  disk.set_name(CreateRandomName("int-test-disk-"));
  disk.set_size_gb("10");
  (*disk.mutable_labels())["test"] = "test";
  auto result = client.InsertDisk(project_id_, zone_, disk).get();
  ASSERT_THAT(result, testing_util::IsOk());

  auto get_disk = client.GetDisk(project_id_, zone_, disk.name());
  ASSERT_THAT(get_disk, IsOk());
  EXPECT_THAT(get_disk->name(), Eq(disk.name()));

  google::cloud::cpp::compute::v1::ZoneSetLabelsRequest request;
  request.set_label_fingerprint(get_disk->label_fingerprint());
  (*request.mutable_labels())["test"] = "test";
  auto set_label =
      client.SetLabels(project_id_, zone_, disk.name(), request).get();
  EXPECT_THAT(set_label, IsOk());

  auto const create_threshold =
      std::chrono::system_clock::now() - std::chrono::hours(48);
  for (auto const& d : client.ListDisks(project_id_, zone_)) {
    ASSERT_STATUS_OK(d);
    // Delete the disk we just created, we expect this to succeed.
    if (d->name() == disk.name()) {
      auto delete_disk = client.DeleteDisk(project_id_, zone_, d->name()).get();
      EXPECT_THAT(delete_disk, IsOk());
    }
    // Garbage collect old disks, ignore errors.
    auto creation_timestamp = internal::ParseRfc3339(d->creation_timestamp());
    if (creation_timestamp) {
      if ((d->labels().contains("test") || d->labels().contains("sample")) &&
          *creation_timestamp < create_threshold) {
        (void)client.DeleteDisk(project_id_, zone_, d->name()).get();
      }
    }
  }
}

TEST_F(ComputeIntegrationTest, PaginatedMapField) {
  namespace disks = ::google::cloud::compute_disks_v1;
  auto client = disks::DisksClient(disks::MakeDisksConnectionRest());

  auto disk_named = [](std::string const& name) {
    return Property(&google::cloud::cpp::compute::v1::Disk::name, Eq(name));
  };

  for (auto const& list : client.AggregatedListDisks(project_id_)) {
    ASSERT_STATUS_OK(list);
    if (list->first == zone_) {
      EXPECT_THAT(list->second.disks(),
                  Contains(disk_named("test-e2-micro-instance")));
      EXPECT_THAT(list->second.disks(),
                  Contains(disk_named("test2-e2-micro-instance")));
    }
  }
}

TEST_F(ComputeIntegrationTest, VerifyUpdateSendsUpdateMaskParameter) {
  namespace disks = ::google::cloud::compute_disks_v1;
  auto client = disks::DisksClient(disks::MakeDisksConnectionRest());

  google::cloud::cpp::compute::v1::Disk disk;
  disk.set_name(CreateRandomName("int-test-disk-"));
  disk.set_size_gb("10");
  (*disk.mutable_labels())["test"] = "test";
  auto result = client.InsertDisk(project_id_, zone_, disk).get();
  ASSERT_THAT(result, testing_util::IsOk());

  google::cloud::cpp::compute::v1::Disk disk_update = disk;
  disk_update.mutable_labels()->clear();

  google::cloud::cpp::compute::disks::v1::UpdateDiskRequest update_request;
  update_request.set_project(project_id_);
  update_request.set_zone(zone_);
  update_request.set_disk(disk.name());
  *(update_request.mutable_disk_resource()) = disk_update;
  auto update_disk = client.UpdateDisk(update_request).get();

  EXPECT_THAT(update_disk,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Empty updateMask is not supported")));

  update_request.set_update_mask("labels");
  update_disk = client.UpdateDisk(update_request).get();
  EXPECT_THAT(update_disk,
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Updating labels is not supported")));

  auto delete_disk = client.DeleteDisk(project_id_, zone_, disk.name()).get();
  EXPECT_STATUS_OK(delete_disk);
}

TEST_F(ComputeIntegrationTest, VerifyPatchResourceFieldNameFormat) {
  namespace instances = ::google::cloud::compute_instances_v1;
  auto client =
      instances::InstancesClient(instances::MakeInstancesConnectionRest());
  std::string const instance_name = "test-e2-micro-instance";
  google::cloud::cpp::compute::v1::ShieldedInstanceConfig
      shielded_instance_config;

  shielded_instance_config.set_enable_integrity_monitoring(false);
  shielded_instance_config.set_enable_vtpm(false);
  shielded_instance_config.set_enable_secure_boot(true);
  auto updated_instance = client.UpdateShieldedInstanceConfig(
      project_id_, zone_, instance_name, shielded_instance_config);
  ASSERT_STATUS_OK(updated_instance.get());

  auto get_instance = client.GetInstance(project_id_, zone_, instance_name);
  ASSERT_THAT(get_instance, IsOk());
  EXPECT_THAT(get_instance->name(), Eq(instance_name));
  EXPECT_FALSE(
      get_instance->shielded_instance_config().enable_integrity_monitoring());
  EXPECT_FALSE(get_instance->shielded_instance_config().enable_vtpm());
  EXPECT_TRUE(get_instance->shielded_instance_config().enable_secure_boot());

  shielded_instance_config.set_enable_integrity_monitoring(true);
  shielded_instance_config.set_enable_vtpm(true);
  shielded_instance_config.set_enable_secure_boot(false);
  updated_instance = client.UpdateShieldedInstanceConfig(
      project_id_, zone_, instance_name, shielded_instance_config);
  ASSERT_STATUS_OK(updated_instance.get());

  get_instance = client.GetInstance(project_id_, zone_, instance_name);
  ASSERT_THAT(get_instance, IsOk());
  EXPECT_THAT(get_instance->name(), Eq(instance_name));
  EXPECT_TRUE(
      get_instance->shielded_instance_config().enable_integrity_monitoring());
  EXPECT_TRUE(get_instance->shielded_instance_config().enable_vtpm());
  EXPECT_FALSE(get_instance->shielded_instance_config().enable_secure_boot());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace compute_v1
}  // namespace cloud
}  // namespace google

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
#include "google/cloud/internal/getenv.h"
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
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Not;

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
  }

  std::string project_id_;
  std::string zone_;
};

TEST_F(ComputeIntegrationTest, DeleteUnknownDisk) {
  namespace disks = ::google::cloud::compute_disks_v1;
  auto client = disks::DisksClient(
      google::cloud::ExperimentalTag{},
      disks::MakeDisksConnectionRest(google::cloud::ExperimentalTag{}));

  StatusOr<google::cloud::cpp::compute::v1::Operation> delete_disk =
      client.DeleteDisks(project_id_, zone_, "not_found").get();
  EXPECT_THAT(
      delete_disk,
      StatusIs(StatusCode::kNotFound,
               HasSubstr("The resource "
                         "'projects/cloud-cpp-testing-resources/zones/"
                         "us-central1-a/disks/not_found' was not found")));
}

TEST_F(ComputeIntegrationTest, CreateDisks) {
  namespace disks = ::google::cloud::compute_disks_v1;
  auto client = disks::DisksClient(
      google::cloud::ExperimentalTag{},
      disks::MakeDisksConnectionRest(google::cloud::ExperimentalTag{}));

  for (auto const& d : client.ListDisks(project_id_, zone_)) {
    ASSERT_STATUS_OK(d);
    if (d->labels().contains("test")) {
      auto delete_disk =
          client.DeleteDisks(project_id_, zone_, d->name()).get();
      EXPECT_THAT(delete_disk, IsOk());
    }
  }

  google::cloud::cpp::compute::v1::Disk disk;
  disk.set_name("int-test-disk");
  disk.set_size_gb("10");
  (*disk.mutable_labels())["test"] = "test";
  auto result = client.InsertDisks(project_id_, zone_, disk).get();
  ASSERT_THAT(result, testing_util::IsOk());

  auto get_disk = client.GetDisks(project_id_, zone_, "int-test-disk");
  ASSERT_THAT(get_disk, IsOk());

  google::cloud::cpp::compute::v1::ZoneSetLabelsRequest request;
  request.set_label_fingerprint(get_disk->label_fingerprint());
  (*request.mutable_labels())["test"] = "test";
  auto set_label =
      client.SetLabels(project_id_, zone_, "int-test-disk", request).get();
  EXPECT_THAT(set_label, IsOk());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace compute_v1
}  // namespace cloud
}  // namespace google

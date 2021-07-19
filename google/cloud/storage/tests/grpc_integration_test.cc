// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/object_stream.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <crc32c/crc32c.h>
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <vector>

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/grpc_error_delegate.h"
#include <grpcpp/grpcpp.h>
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

// When GOOGLE_CLOUD_CPP_HAVE_GRPC is not set these tests compile, but they
// actually just run against the regular GCS REST API. That is fine.
class GrpcIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  GrpcIntegrationTest()
      : grpc_config_("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", {}) {}

  void SetUp() override {
    std::string const grpc_config_value = GetParam();
    google::cloud::internal::SetEnv("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                    grpc_config_value);
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id_.empty()) << "GOOGLE_CLOUD_PROJECT is not set";
  }

  std::string project_id() const { return project_id_; }

  std::string MakeEntityName() {
    // We always use the viewers for the project because it is known to exist.
    return "project-viewers-" + project_id_;
  }

 private:
  std::string project_id_;
  std::string topic_name_;
  testing_util::ScopedEnvironment grpc_config_;
};

TEST_P(GrpcIntegrationTest, ObjectCRUD) {
  auto bucket_client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(bucket_client);

  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();
  auto bucket_metadata = bucket_client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);

  EXPECT_EQ(bucket_name, bucket_metadata->name());

  auto object_metadata = client->InsertObject(
      bucket_name, object_name, LoremIpsum(), IfGenerationMatch(0));
  ASSERT_STATUS_OK(object_metadata);

  auto stream = client->ReadObject(bucket_name, object_name);

  std::string actual(std::istreambuf_iterator<char>{stream}, {});
  EXPECT_EQ(LoremIpsum(), actual);
  EXPECT_STATUS_OK(stream.status());

  // This is part of the test, not just a cleanup.
  auto delete_object_status = client->DeleteObject(
      bucket_name, object_name, Generation(object_metadata->generation()));
  EXPECT_STATUS_OK(delete_object_status);

  auto delete_bucket_status = bucket_client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

TEST_P(GrpcIntegrationTest, WriteResume) {
  auto bucket_client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(bucket_client);

  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();
  auto bucket_metadata = bucket_client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);

  // We will construct the expected response while streaming the data up.
  std::ostringstream expected;

  // Create the object, but only if it does not exist already.
  std::string session_id;
  {
    auto old_os =
        client->WriteObject(bucket_name, object_name, IfGenerationMatch(0),
                            NewResumableUploadSession());
    ASSERT_TRUE(old_os.good()) << "status=" << old_os.metadata().status();
    session_id = old_os.resumable_session_id();
    std::move(old_os).Suspend();
  }

  auto os = client->WriteObject(bucket_name, object_name,
                                RestoreResumableUploadSession(session_id));
  ASSERT_TRUE(os.good()) << "status=" << os.metadata().status();
  EXPECT_EQ(session_id, os.resumable_session_id());
  os << LoremIpsum();
  os.Close();
  ASSERT_STATUS_OK(os.metadata());
  ScheduleForDelete(*os.metadata());

  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  if (UsingEmulator()) {
    EXPECT_TRUE(meta.has_metadata("x_emulator_upload"));
    EXPECT_EQ("resumable", meta.metadata("x_emulator_upload"));
  }

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);

  auto delete_bucket_status = bucket_client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

TEST_P(GrpcIntegrationTest, InsertLarge) {
  auto bucket_client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(bucket_client);

  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();
  auto bucket_metadata = bucket_client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);
  ScheduleForDelete(*bucket_metadata);

  // Insert an object that is larger than 4 MiB, and its size is not a
  // multiple of 256 KiB.
  auto const desired_size = 8 * 1024 * 1024L + 253 * 1024 + 15;
  auto data = MakeRandomData(desired_size);
  auto metadata = client->InsertObject(bucket_name, object_name, data,
                                       IfGenerationMatch(0));
  ASSERT_STATUS_OK(metadata);
  ScheduleForDelete(*metadata);

  EXPECT_EQ(desired_size, metadata->size());
}

TEST_P(GrpcIntegrationTest, StreamLargeChunks) {
  auto bucket_client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(bucket_client);

  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();
  auto bucket_metadata = bucket_client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);
  ScheduleForDelete(*std::move(bucket_metadata));

  // Insert an object in chunks larger than 4 MiB each.
  auto const desired_size = 8 * 1024 * 1024L;
  auto data = MakeRandomData(desired_size);
  auto stream =
      client->WriteObject(bucket_name, object_name, IfGenerationMatch(0));
  stream.write(data.data(), data.size());
  EXPECT_TRUE(stream.good());
  stream.write(data.data(), data.size());
  EXPECT_TRUE(stream.good());
  stream.Close();
  EXPECT_FALSE(stream.bad());
  ASSERT_STATUS_OK(stream.metadata());
  ScheduleForDelete(stream.metadata().value());

  EXPECT_EQ(2 * desired_size, stream.metadata()->size());
}

INSTANTIATE_TEST_SUITE_P(GrpcIntegrationMediaTest, GrpcIntegrationTest,
                         ::testing::Values("media"));

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

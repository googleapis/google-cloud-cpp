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
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <crc32c/crc32c.h>
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <algorithm>
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

using ::testing::Contains;
using ::testing::Not;

// When GOOGLE_CLOUD_CPP_HAVE_GRPC is not set these tests compile, but they
// actually just run against the regular GCS REST API. That is fine.
class GrpcIntegrationTest
    : public google::cloud::storage::testing::StorageIntegrationTest {
 protected:
  void SetUp() override {
    project_id_ =
        google::cloud::internal::GetEnv("GOOGLE_CLOUD_PROJECT").value_or("");
    ASSERT_FALSE(project_id_.empty()) << "GOOGLE_CLOUD_PROJECT is not set";
  }
  std::string project_id() const { return project_id_; }

 private:
  std::string project_id_;
  testing_util::ScopedEnvironment grpc_config_{
      "GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", "metadata"};
};

TEST_F(GrpcIntegrationTest, BucketCRUD) {
  auto client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto bucket_metadata = client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);

  EXPECT_EQ(bucket_name, bucket_metadata->name());

  auto list_bucket_names = [&client, this] {
    std::vector<std::string> names;
    for (auto b : client->ListBucketsForProject(project_id())) {
      EXPECT_STATUS_OK(b);
      if (!b) break;
      names.push_back(b->name());
    }
    return names;
  };
  EXPECT_THAT(list_bucket_names(), Contains(bucket_name));

  auto get = client->GetBucketMetadata(bucket_name);
  ASSERT_STATUS_OK(get);
  EXPECT_EQ(bucket_name, get->id());
  EXPECT_EQ(bucket_name, get->name());

  auto delete_status = client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_status);
  EXPECT_THAT(list_bucket_names(), Not(Contains(bucket_name)));
}

TEST_F(GrpcIntegrationTest, ObjectCRUD) {
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

  auto delete_object_status = client->DeleteObject(
      bucket_name, object_name, Generation(object_metadata->generation()));
  EXPECT_STATUS_OK(delete_object_status);

  auto delete_bucket_status = bucket_client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

TEST_F(GrpcIntegrationTest, WriteResume) {
  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();
  auto bucket_metadata = client->CreateBucketForProject(
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
  ObjectMetadata meta = os.metadata().value();
  EXPECT_EQ(object_name, meta.name());
  EXPECT_EQ(bucket_name, meta.bucket());
  if (UsingTestbench()) {
    EXPECT_TRUE(meta.has_metadata("x_testbench_upload"));
    EXPECT_EQ("resumable", meta.metadata("x_testbench_upload"));
  }

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);

  auto delete_bucket_status = client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

#if GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC
/// @test Verify that NOT_FOUND is returned for missing objects
TEST_F(GrpcIntegrationTest, GetObjectMediaNotFound) {
  auto bucket_client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(bucket_client);

  auto client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto bucket_metadata = bucket_client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);

  auto object_name = MakeRandomObjectName();

  auto channel = grpc::CreateChannel("storage.googleapis.com",
                                     grpc::GoogleDefaultCredentials());
  auto stub = google::storage::v1::Storage::NewStub(channel);

  grpc::ClientContext context;
  google::storage::v1::GetObjectMediaRequest request;
  request.set_bucket(bucket_name);
  request.set_object(object_name);
  auto stream = stub->GetObjectMedia(&context, request);
  google::storage::v1::GetObjectMediaResponse response;
  auto open = true;
  for (int i = 0; i != 100 && open; ++i) {
    open = stream->Read(&response);
  }
  EXPECT_FALSE(open);

  auto status = stream->Finish();
  ASSERT_EQ(grpc::StatusCode::NOT_FOUND, status.error_code())
      << "message = " << status.error_message();
  auto delete_bucket_status = bucket_client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}
#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

TEST_F(GrpcIntegrationTest, InsertLarge) {
  auto bucket_client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(bucket_client);

  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();
  auto bucket_metadata = bucket_client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);

  // Insert an object that is larger than 4 MiB, and its size is not a multiple
  // of 256 KiB.
  auto const desired_size = 8 * 1024 * 1024L + 253 * 1024 + 15;
  auto data = MakeRandomData(desired_size);
  auto metadata = client->InsertObject(bucket_name, object_name, data,
                                       IfGenerationMatch(0));
  EXPECT_STATUS_OK(metadata);
  if (!metadata) {
    auto delete_bucket_status = client->DeleteBucket(bucket_name);
    EXPECT_STATUS_OK(delete_bucket_status);
    return;
  }
  EXPECT_EQ(desired_size, metadata->size());

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);

  auto delete_bucket_status = bucket_client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

TEST_F(GrpcIntegrationTest, StreamLargeChunks) {
  auto bucket_client = MakeBucketIntegrationTestClient();
  ASSERT_STATUS_OK(bucket_client);

  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName();
  auto object_name = MakeRandomObjectName();
  auto bucket_metadata = bucket_client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);

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
  EXPECT_STATUS_OK(stream.metadata());

  EXPECT_EQ(2 * desired_size, stream.metadata()->size());

  auto status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(status);

  auto delete_bucket_status = bucket_client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

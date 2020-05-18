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
#include "google/cloud/storage/internal/nljson.h"
#include "google/cloud/storage/object_stream.h"
#include "google/cloud/storage/testing/storage_integration_test.h"
#include "google/cloud/internal/getenv.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include <crc32c/crc32c.h>
#include <gmock/gmock.h>
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
  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName("cloud-cpp-testing-");
  auto bucket_metadata = client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);

  EXPECT_EQ(bucket_name, bucket_metadata->name());

  auto bucket_reader = client->ListBucketsForProject(project_id());

  auto count = std::count_if(bucket_reader.begin(), bucket_reader.end(),
                             [&](StatusOr<BucketMetadata> const& b) {
                               return b && b->name() == bucket_name;
                             });
  EXPECT_NE(0, count);

  auto get = client->GetBucketMetadata(bucket_name);
  ASSERT_STATUS_OK(get);
  EXPECT_EQ(bucket_name, get->id());
  EXPECT_EQ(bucket_name, get->name());

  auto delete_status = client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_status);
  count = std::count_if(bucket_reader.begin(), bucket_reader.end(),
                        [&](StatusOr<BucketMetadata> const& b) {
                          return b && b->name() == bucket_name;
                        });
  EXPECT_EQ(0, count);
}

TEST_F(GrpcIntegrationTest, ObjectCRUD) {
  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName("cloud-cpp-testing-");
  auto object_name = MakeRandomObjectName();
  auto bucket_metadata = client->CreateBucketForProject(
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

  auto delete_bucket_status = client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

TEST_F(GrpcIntegrationTest, WriteResume) {
  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName("cloud-cpp-testing-");
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
  auto client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName("cloud-cpp-testing-");
  auto bucket_metadata = client->CreateBucketForProject(
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
  auto delete_bucket_status = client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

/**
 * @test Verify that buffers over 4MiB in size result in an error
 *
 * The gRPC API returns kResourceExhausted for streams larger than 4MiB, this
 * is a test to verify this is the behavior. There is a lot of extra complexity
 * in the code to perform resumable uploads which break large InsertObject()
 * requests into multiple streams. It would be unfortunate if that was
 * unjustified.
 */
TEST_F(GrpcIntegrationTest, ReproLargeInsert) {
  if (!UsingGrpc()) GTEST_SKIP();

  auto client = Client::CreateDefaultClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName("cloud-cpp-testing-");
  auto bucket_metadata = client->CreateBucketForProject(
      bucket_name, project_id(), BucketMetadata());
  ASSERT_STATUS_OK(bucket_metadata);

  auto object_name = MakeRandomObjectName();

  auto channel = grpc::CreateChannel("storage.googleapis.com",
                                     grpc::GoogleDefaultCredentials());
  auto stub = google::storage::v1::Storage::NewStub(channel);

  auto constexpr kMaxBuffersize = 4 * 1024 * 1024LL;
  auto constexpr kChunkSize = 256 * 1024L;
  static_assert(kMaxBuffersize % kChunkSize == 0, "Broken test configuration");

  auto send_in_chunks = [bucket_name, object_name, &stub, this](
                            int desired_size, std::int64_t chunk_size) {
    grpc::ClientContext context;
    google::storage::v1::Object object;
    auto stream = stub->InsertObject(&context, &object);

    google::storage::v1::InsertObjectRequest request;
    auto& spec = *request.mutable_insert_object_spec();
    auto& resource = *spec.mutable_resource();
    resource.set_bucket(bucket_name);
    resource.set_name(object_name);

    std::uint64_t offset = 0;
    auto data = MakeRandomData(chunk_size);
    request.mutable_checksummed_data()->mutable_crc32c()->set_value(
        crc32c::Crc32c(data));
    request.mutable_checksummed_data()->set_content(std::move(data));
    request.set_write_offset(offset);
    (void)stream->Write(request, grpc::WriteOptions().set_write_through());

    offset += chunk_size;
    data = MakeRandomData(desired_size - chunk_size);
    request.clear_insert_object_spec();
    request.mutable_checksummed_data()->mutable_crc32c()->set_value(
        crc32c::Crc32c(data));
    request.mutable_checksummed_data()->set_content(std::move(data));
    request.set_write_offset(offset);
    request.set_finish_write(true);
    (void)stream->Write(
        request, grpc::WriteOptions().set_write_through().set_last_message());

    return google::cloud::MakeStatusFromRpcError(stream->Finish());
  };

  // This creates a stream of exactly kMaxBufferSize
  auto status = send_in_chunks(kMaxBuffersize, kChunkSize);
  ASSERT_STATUS_OK(status);

  status = send_in_chunks(kMaxBuffersize + kChunkSize, kChunkSize);
  EXPECT_EQ(StatusCode::kResourceExhausted, status.code());

  status = send_in_chunks(kMaxBuffersize + 2 * kChunkSize, kChunkSize);
  EXPECT_EQ(StatusCode::kResourceExhausted, status.code());

  auto delete_object_status = client->DeleteObject(bucket_name, object_name);
  EXPECT_STATUS_OK(delete_object_status);

  auto delete_bucket_status = client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

#endif  // GOOGLE_CLOUD_CPP_STORAGE_HAVE_GRPC

TEST_F(GrpcIntegrationTest, InsertLarge) {
  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName("cloud-cpp-testing-");
  auto object_name = MakeRandomObjectName();
  auto bucket_metadata = client->CreateBucketForProject(
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

  auto delete_bucket_status = client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

TEST_F(GrpcIntegrationTest, StreamLargeChunks) {
  auto client = MakeIntegrationTestClient();
  ASSERT_STATUS_OK(client);

  auto bucket_name = MakeRandomBucketName("cloud-cpp-testing-");
  auto object_name = MakeRandomObjectName();
  auto bucket_metadata = client->CreateBucketForProject(
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

  auto delete_bucket_status = client->DeleteBucket(bucket_name);
  EXPECT_STATUS_OK(delete_bucket_status);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

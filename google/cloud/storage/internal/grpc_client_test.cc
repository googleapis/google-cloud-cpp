// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

namespace v2 = ::google::storage::v2;
using ::google::cloud::testing_util::GetMetadata;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

Status PermanentError() {
  return Status(StatusCode::kPermissionDenied, "uh-oh");
}

TEST(GrpcClient, QueryResumableUpload) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, QueryWriteStatus)
      .WillOnce([](grpc::ClientContext& context,
                   v2::QueryWriteStatusRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair("x-goog-quota-user", "test-quota-user"),
                        // Map JSON names to the `resource` subobject
                        Pair("x-goog-fieldmask", "resource(field1,field2)")));
        EXPECT_EQ(request.upload_id(), "test-only-upload-id");
        return PermanentError();
      });
  auto client = GrpcClient::CreateMock(mock);
  auto response = client->QueryResumableUpload(
      QueryResumableUploadRequest("test-only-upload-id")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST(GrpcClient, GetBucket) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([](grpc::ClientContext& context,
                   google::storage::v2::GetBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = GrpcClient::CreateMock(mock);
  auto response = client->GetBucketMetadata(
      GetBucketMetadataRequest("test-bucket")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST(GrpcClient, InsertObjectMedia) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, WriteObject)
      .WillOnce([](std::unique_ptr<grpc::ClientContext> context) {
        auto metadata = GetMetadata(*context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair("x-goog-quota-user", "test-quota-user"),
                        // Map JSON names to the `resource` subobject
                        Pair("x-goog-fieldmask", "resource(field1,field2)")));
        auto stream = absl::make_unique<testing::MockInsertStream>();
        EXPECT_CALL(*stream, Write)
            .WillOnce([](v2::WriteObjectRequest const&, grpc::WriteOptions) {
              return false;
            });
        EXPECT_CALL(*stream, Close).WillOnce(Return(PermanentError()));
        return std::unique_ptr<google::cloud::internal::StreamingWriteRpc<
            google::storage::v2::WriteObjectRequest,
            google::storage::v2::WriteObjectResponse>>(std::move(stream));
      });
  auto client = GrpcClient::CreateMock(mock);
  auto response = client->InsertObjectMedia(
      InsertObjectMediaRequest("test-bucket", "test-object",
                               "How vexingly quick daft zebras jump!")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST(GrpcClient, GetObjectMetadata) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([](grpc::ClientContext& context,
                   v2::GetObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.object(), "test-object");
        return PermanentError();
      });
  auto client = GrpcClient::CreateMock(mock);
  auto response = client->GetObjectMetadata(
      GetObjectMetadataRequest("test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST(GrpcClient, ReadObject) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, ReadObject)
      .WillOnce([](std::unique_ptr<grpc::ClientContext> context,
                   v2::ReadObjectRequest const& request) {
        auto metadata = GetMetadata(*context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.object(), "test-object");
        auto stream = absl::make_unique<testing::MockObjectMediaStream>();
        return std::unique_ptr<google::cloud::internal::StreamingReadRpc<
            google::storage::v2::ReadObjectResponse>>(std::move(stream));
      });
  auto client = GrpcClient::CreateMock(mock);
  auto stream = client->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
}

TEST(GrpcClient, ListObjects) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce([](grpc::ClientContext& context,
                   v2::ListObjectsRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.parent(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = GrpcClient::CreateMock(mock);
  auto response = client->ListObjects(
      ListObjectsRequest("test-bucket")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST(GrpcClient, DeleteObject) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](grpc::ClientContext& context,
                   google::storage::v2::DeleteObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.object(), "test-object");
        return PermanentError();
      });
  auto client = GrpcClient::CreateMock(mock);
  auto response = client->DeleteObject(
      DeleteObjectRequest("test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST(GrpcClient, RewriteObject) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, RewriteObject)
      .WillOnce([](grpc::ClientContext& context,
                   v2::RewriteObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair("x-goog-quota-user", "test-quota-user"),
                        // Map JSON names to the `resource` subobject
                        Pair("x-goog-fieldmask", "resource(field1,field2)")));
        EXPECT_THAT(request.source_bucket(),
                    "projects/_/buckets/test-source-bucket");
        EXPECT_THAT(request.source_object(), "test-source-object");
        EXPECT_THAT(request.destination().bucket(),
                    "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.destination().name(), "test-object");
        return PermanentError();
      });
  auto client = GrpcClient::CreateMock(mock);
  auto response = client->RewriteObject(
      RewriteObjectRequest("test-source-bucket", "test-source-object",
                           "test-bucket", "test-object", "test-token")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST(GrpcClient, CreateResumableSession) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, StartResumableWrite)
      .WillOnce([](grpc::ClientContext& context,
                   v2::StartResumableWriteRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair("x-goog-quota-user", "test-quota-user"),
                        // Map the JSON field names to the `resource` subobject
                        Pair("x-goog-fieldmask", "resource(field1,field2)")));
        EXPECT_THAT(request.write_object_spec().resource().bucket(),
                    "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.write_object_spec().resource().name(),
                    "test-object");
        return PermanentError();
      });
  auto client = GrpcClient::CreateMock(mock);
  auto response = client->CreateResumableSession(
      ResumableUploadRequest("test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST(GrpcClient, GetServiceAccount) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetServiceAccount)
      .WillOnce([](grpc::ClientContext& context,
                   v2::GetServiceAccountRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.project(), "projects/test-project-id");
        return PermanentError();
      });
  auto client = GrpcClient::CreateMock(mock);
  auto response = client->GetServiceAccount(
      GetProjectServiceAccountRequest("test-project-id")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

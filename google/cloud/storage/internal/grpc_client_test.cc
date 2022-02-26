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
#include "google/cloud/credentials.h"
#include "google/cloud/options.h"
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
using ::google::cloud::internal::CurrentOptions;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

auto constexpr kAuthority = "storage.googleapis.com";

class GrpcClientTest : public ::testing::Test {
 protected:
  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& context) {
    return validate_metadata_fixture_.GetMetadata(context);
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

Status PermanentError() {
  return Status(StatusCode::kPermissionDenied, "uh-oh");
}

std::shared_ptr<GrpcClient> CreateTestClient(
    std::shared_ptr<storage_internal::StorageStub> stub) {
  return GrpcClient::CreateMock(
      std::move(stub),
      Options{}.set<UnifiedCredentialsOption>(MakeInsecureCredentials()));
}

TEST_F(GrpcClientTest, QueryResumableUpload) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, QueryWriteStatus)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::QueryWriteStatusRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair("x-goog-quota-user", "test-quota-user"),
                        // Map JSON names to the `resource` subobject
                        Pair("x-goog-fieldmask", "resource(field1,field2)")));
        EXPECT_EQ(request.upload_id(), "test-only-upload-id");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->QueryResumableUpload(
      QueryResumableUploadRequest("test-only-upload-id")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, CreateBucket) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, CreateBucket)
      .WillOnce([this](
                    grpc::ClientContext& context,
                    google::storage::v2::CreateBucketRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.parent(), "projects/test-project");
        EXPECT_THAT(request.bucket_id(), "test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->CreateBucket(
      CreateBucketRequest("test-project",
                          BucketMetadata().set_name("test-bucket"))
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, GetBucket) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       google::storage::v2::GetBucketRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->GetBucketMetadata(
      GetBucketMetadataRequest("test-bucket")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, DeleteBucket) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, DeleteBucket)
      .WillOnce([this](
                    grpc::ClientContext& context,
                    google::storage::v2::DeleteBucketRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->DeleteBucket(
      DeleteBucketRequest("test-bucket")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, ListBuckets) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, ListBuckets)
      .WillOnce([this](grpc::ClientContext& context,
                       google::storage::v2::ListBucketsRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.parent(), "projects/test-project");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->ListBuckets(
      ListBucketsRequest("test-project")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, LockBucketRetentionPolicy) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, LockBucketRetentionPolicy)
      .WillOnce(
          [this](grpc::ClientContext& context,
                 google::storage::v2::LockBucketRetentionPolicyRequest const&) {
            EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
            auto metadata = GetMetadata(context);
            EXPECT_THAT(metadata,
                        UnorderedElementsAre(
                            Pair("x-goog-quota-user", "test-quota-user"),
                            Pair("x-goog-fieldmask", "field1,field2")));
            return PermanentError();
          });
  auto client = CreateTestClient(mock);
  auto response = client->LockBucketRetentionPolicy(
      LockBucketRetentionPolicyRequest("test-bucket", /*metageneration=*/7)
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, UpdateBucket) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([this](
                    grpc::ClientContext& context,
                    google::storage::v2::UpdateBucketRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket().name(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->UpdateBucket(
      UpdateBucketRequest(BucketMetadata{}.set_name("test-bucket"))
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, PatchBucket) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([this](
                    grpc::ClientContext& context,
                    google::storage::v2::UpdateBucketRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket().name(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->PatchBucket(
      PatchBucketRequest("test-bucket",
                         BucketMetadataPatchBuilder{}.SetLabel("l0", "v0"))
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, GetBucketIamPolicy) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetIamPolicy)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::GetIamPolicyRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.resource(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->GetBucketIamPolicy(
      GetBucketIamPolicyRequest("test-bucket")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, GetNativeBucketIamPolicy) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetIamPolicy)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::GetIamPolicyRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.resource(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->GetNativeBucketIamPolicy(
      GetBucketIamPolicyRequest("test-bucket")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, SetBucketIamPolicy) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, SetIamPolicy)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::SetIamPolicyRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.resource(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->SetBucketIamPolicy(
      SetBucketIamPolicyRequest(
          "test-bucket",
          IamPolicy{/*.version=*/1, /*.bindings=*/{}, /*.etag=*/""})
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, SetNativeBucketIamPolicy) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, SetIamPolicy)
      .WillOnce([this](grpc::ClientContext& context,
                       google::iam::v1::SetIamPolicyRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.resource(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->SetNativeBucketIamPolicy(
      SetNativeBucketIamPolicyRequest(
          "test-bucket", NativeIamPolicy(/*bindings=*/{}, /*etag=*/{}))
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, TestBucketIamPermissions) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, TestIamPermissions)
      .WillOnce([this](
                    grpc::ClientContext& context,
                    google::iam::v1::TestIamPermissionsRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.resource(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->TestBucketIamPermissions(
      TestBucketIamPermissionsRequest(
          "test-bucket", {"test.permission.1", "test.permission.2"})
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, InsertObjectMedia) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, WriteObject)
      .WillOnce([this](std::unique_ptr<grpc::ClientContext> context) {
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
  auto client = CreateTestClient(mock);
  auto response = client->InsertObjectMedia(
      InsertObjectMediaRequest("test-bucket", "test-object",
                               "How vexingly quick daft zebras jump!")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, CopyObject) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, RewriteObject)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::RewriteObjectRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair("x-goog-quota-user", "test-quota-user"),
                        // Map JSON names to the `resource` subobject
                        Pair("x-goog-fieldmask", "resource(field1,field2)")));
        EXPECT_THAT(request.source_bucket(),
                    "projects/_/buckets/test-source-bucket");
        EXPECT_THAT(request.source_object(), "test-source-object");
        EXPECT_THAT(request.destination_bucket(),
                    "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.destination_name(), "test-object");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->CopyObject(
      CopyObjectRequest("test-source-bucket", "test-source-object",
                        "test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, CopyObjectTooLarge) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, RewriteObject)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::RewriteObjectRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair("x-goog-quota-user", "test-quota-user"),
                        // Map JSON names to the `resource` subobject
                        Pair("x-goog-fieldmask", "resource(field1,field2)")));
        EXPECT_THAT(request.source_bucket(),
                    "projects/_/buckets/test-source-bucket");
        EXPECT_THAT(request.source_object(), "test-source-object");
        EXPECT_THAT(request.destination_bucket(),
                    "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.destination_name(), "test-object");
        v2::RewriteResponse response;
        response.set_done(false);
        response.set_rewrite_token("test-only-token");
        return response;
      });
  auto client = CreateTestClient(mock);
  auto response = client->CopyObject(
      CopyObjectRequest("test-source-bucket", "test-source-object",
                        "test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_THAT(response.status(), StatusIs(StatusCode::kOutOfRange));
}

TEST_F(GrpcClientTest, GetObjectMetadata) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetObjectRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.object(), "test-object");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->GetObjectMetadata(
      GetObjectMetadataRequest("test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, ReadObject) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, ReadObject)
      .WillOnce([this](std::unique_ptr<grpc::ClientContext> context,
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
  auto client = CreateTestClient(mock);
  auto stream = client->ReadObject(
      ReadObjectRangeRequest("test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
}

TEST_F(GrpcClientTest, ListObjects) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::ListObjectsRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.parent(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->ListObjects(
      ListObjectsRequest("test-bucket")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, DeleteObject) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([this](
                    grpc::ClientContext& context,
                    google::storage::v2::DeleteObjectRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.object(), "test-object");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->DeleteObject(
      DeleteObjectRequest("test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, UpdateObject) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, UpdateObject)
      .WillOnce([this](
                    grpc::ClientContext& context,
                    google::storage::v2::UpdateObjectRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.object().bucket(),
                    "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.object().name(), "test-object");
        return PermanentError();
      });
  auto client = GrpcClient::CreateMock(mock);
  auto response = client->UpdateObject(
      UpdateObjectRequest("test-bucket", "test-object",
                          // Typically, the metadata is first read from the
                          // service as part of an OCC loop. For this test, just
                          // use the default values for all fields
                          ObjectMetadata{})
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, PatchObject) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, UpdateObject)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::UpdateObjectRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.object().bucket(),
                    "projects/_/buckets/test-source-bucket");
        EXPECT_THAT(request.object().name(), "test-source-object");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->PatchObject(
      PatchObjectRequest(
          "test-source-bucket", "test-source-object",
          ObjectMetadataPatchBuilder{}.SetCacheControl("no-cache"))
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, ComposeObject) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::ComposeObjectRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.destination().bucket(),
                    "projects/_/buckets/test-source-bucket");
        EXPECT_THAT(request.destination().name(), "test-source-object");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->ComposeObject(
      ComposeObjectRequest("test-source-bucket", {}, "test-source-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, RewriteObject) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, RewriteObject)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::RewriteObjectRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair("x-goog-quota-user", "test-quota-user"),
                        // Map JSON names to the `resource` subobject
                        Pair("x-goog-fieldmask", "resource(field1,field2)")));
        EXPECT_THAT(request.source_bucket(),
                    "projects/_/buckets/test-source-bucket");
        EXPECT_THAT(request.source_object(), "test-source-object");
        EXPECT_THAT(request.destination_bucket(),
                    "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.destination_name(), "test-object");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->RewriteObject(
      RewriteObjectRequest("test-source-bucket", "test-source-object",
                           "test-bucket", "test-object", "test-token")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, CreateResumableSession) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, StartResumableWrite)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::StartResumableWriteRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
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
  auto client = CreateTestClient(mock);
  auto response = client->CreateResumableSession(
      ResumableUploadRequest("test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, GetServiceAccount) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetServiceAccount)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetServiceAccountRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.project(), "projects/test-project-id");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
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

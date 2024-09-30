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

#include "google/cloud/storage/internal/grpc/stub.h"
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/credentials.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::storage::v2;
using ::google::cloud::storage::BucketMetadata;
using ::google::cloud::storage::Fields;
using ::google::cloud::storage::ObjectMetadata;
using ::google::cloud::storage::QuotaUser;
using ::google::cloud::storage::internal::CreateNullHashFunction;
using ::google::cloud::storage::testing::MockInsertStream;
using ::google::cloud::storage::testing::MockStorageStub;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::testing::Contains;
using ::testing::Pair;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

class GrpcClientTest : public ::testing::Test {
 protected:
  std::multimap<std::string, std::string> GetMetadata(
      grpc::ClientContext& context) {
    return validate_metadata_fixture_.GetMetadata(context);
  }

 private:
  ValidateMetadataFixture validate_metadata_fixture_;
};

auto constexpr kIdempotencyTokenHeader = "x-goog-gcs-idempotency-token";

Status PermanentError() {
  return Status(StatusCode::kPermissionDenied, "uh-oh");
}

google::cloud::Options TestOptions() {
  return Options{}.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
}

rest_internal::RestContext TestContext() {
  return rest_internal::RestContext(TestOptions())
      .AddHeader(kIdempotencyTokenHeader, "test-token-1234");
}

std::unique_ptr<GrpcStub> CreateTestClient(
    std::shared_ptr<storage_internal::StorageStub> stub) {
  std::shared_ptr<google::cloud::internal::MinimalIamCredentialsStub> unused;
  return std::make_unique<GrpcStub>(std::move(stub), /*iam=*/unused,
                                    TestOptions());
}

TEST_F(GrpcClientTest, QueryResumableUpload) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, QueryWriteStatus)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::QueryWriteStatusRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_EQ(request.upload_id(), "test-only-upload-id");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = rest_internal::RestContext(TestOptions());
  auto response = client->QueryResumableUpload(
      context, TestOptions(),
      storage::internal::QueryResumableUploadRequest("test-only-upload-id")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, DeleteResumableUpload) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, CancelResumableWrite)
      .WillOnce(
          [this](
              grpc::ClientContext& context, Options const&,
              google::storage::v2::CancelResumableWriteRequest const& request) {
            auto metadata = GetMetadata(context);
            EXPECT_THAT(metadata,
                        UnorderedElementsAre(
                            Pair(kIdempotencyTokenHeader, "test-token-1234"),
                            Pair("x-goog-quota-user", "test-quota-user"),
                            Pair("x-goog-fieldmask", "field1,field2")));
            EXPECT_THAT(request.upload_id(), "test-only-upload-id");
            return PermanentError();
          });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->DeleteResumableUpload(
      context, TestOptions(),
      storage::internal::DeleteResumableUploadRequest("test-only-upload-id")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, UploadChunk) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, WriteObject)
      .WillOnce([this](auto context, Options const&) {
        auto metadata = GetMetadata(*context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2"),
                        Pair("x-goog-request-params",
                             "bucket=projects%2F_%2Fbuckets%2Ftest-bucket")));
        ::testing::InSequence sequence;
        auto stream = std::make_unique<MockInsertStream>();
        EXPECT_CALL(*stream, Write).WillOnce(Return(false));
        EXPECT_CALL(*stream, Close).WillOnce(Return(PermanentError()));
        return stream;
      });
  auto client = CreateTestClient(mock);
  auto context = rest_internal::RestContext(TestOptions());
  auto response = client->UploadChunk(
      context, TestOptions(),
      storage::internal::UploadChunkRequest(
          "projects/_/buckets/test-bucket/test-upload-id", 0, {},
          CreateNullHashFunction())
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, CreateBucket) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, CreateBucket)
      .WillOnce(
          [this](grpc::ClientContext& context, Options const&,
                 google::storage::v2::CreateBucketRequest const& request) {
            auto metadata = GetMetadata(context);
            EXPECT_THAT(metadata,
                        UnorderedElementsAre(
                            Pair(kIdempotencyTokenHeader, "test-token-1234"),
                            Pair("x-goog-quota-user", "test-quota-user"),
                            Pair("x-goog-fieldmask", "field1,field2")));
            EXPECT_THAT(request.parent(), "projects/_");
            EXPECT_THAT(request.bucket_id(), "test-bucket");
            EXPECT_THAT(request.bucket().project(), "projects/test-project");
            return PermanentError();
          });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->CreateBucket(
      context, TestOptions(),
      storage::internal::CreateBucketRequest(
          "test-project", BucketMetadata().set_name("test-bucket"))
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, CreateBucketAlreadyExists) {
  for (auto const code : {
           StatusCode::kAlreadyExists,
           StatusCode::kFailedPrecondition,
           StatusCode::kAborted,
       }) {
    SCOPED_TRACE("Testing with code " + StatusCodeToString(code));
    auto mock = std::make_shared<MockStorageStub>();
    EXPECT_CALL(*mock, CreateBucket)
        .WillOnce(Return(Status(code, "bucket already exists")));
    auto client = CreateTestClient(mock);
    auto context = TestContext();
    auto response = client->CreateBucket(
        context, TestOptions(),
        storage::internal::CreateBucketRequest(
            "test-project", BucketMetadata().set_name("test-bucket"))
            .set_multiple_options(Fields("field1,field2"),
                                  QuotaUser("test-quota-user")));
    EXPECT_THAT(response,
                StatusIs(StatusCode::kAlreadyExists, "bucket already exists"));
  }
}

TEST_F(GrpcClientTest, GetBucket) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::storage::v2::GetBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->GetBucketMetadata(
      context, TestOptions(),
      storage::internal::GetBucketMetadataRequest("test-bucket")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, DeleteBucket) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, DeleteBucket)
      .WillOnce(
          [this](grpc::ClientContext& context, Options const&,
                 google::storage::v2::DeleteBucketRequest const& request) {
            auto metadata = GetMetadata(context);
            EXPECT_THAT(metadata,
                        UnorderedElementsAre(
                            Pair(kIdempotencyTokenHeader, "test-token-1234"),
                            Pair("x-goog-quota-user", "test-quota-user"),
                            Pair("x-goog-fieldmask", "field1,field2")));
            EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket");
            return PermanentError();
          });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->DeleteBucket(
      context, TestOptions(),
      storage::internal::DeleteBucketRequest("test-bucket")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, ListBuckets) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, ListBuckets)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::storage::v2::ListBucketsRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.parent(), "projects/test-project");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->ListBuckets(
      context, TestOptions(),
      storage::internal::ListBucketsRequest("test-project")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, LockBucketRetentionPolicy) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, LockBucketRetentionPolicy)
      .WillOnce(
          [this](grpc::ClientContext& context, Options const&,
                 google::storage::v2::LockBucketRetentionPolicyRequest const&) {
            auto metadata = GetMetadata(context);
            EXPECT_THAT(metadata,
                        UnorderedElementsAre(
                            Pair(kIdempotencyTokenHeader, "test-token-1234"),
                            Pair("x-goog-quota-user", "test-quota-user"),
                            Pair("x-goog-fieldmask", "field1,field2")));
            return PermanentError();
          });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->LockBucketRetentionPolicy(
      context, TestOptions(),
      storage::internal::LockBucketRetentionPolicyRequest("test-bucket",
                                                          /*metageneration=*/7)
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, UpdateBucket) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([this](
                    grpc::ClientContext& context, Options const&,
                    google::storage::v2::UpdateBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket().name(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->UpdateBucket(
      context, TestOptions(),
      storage::internal::UpdateBucketRequest(
          BucketMetadata{}.set_name("test-bucket"))
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, PatchBucket) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([this](
                    grpc::ClientContext& context, Options const&,
                    google::storage::v2::UpdateBucketRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket().name(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->PatchBucket(
      context, TestOptions(),
      storage::internal::PatchBucketRequest(
          "test-bucket",
          storage::BucketMetadataPatchBuilder{}.SetLabel("l0", "v0"))
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, GetNativeBucketIamPolicy) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, GetIamPolicy)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::iam::v1::GetIamPolicyRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.resource(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->GetNativeBucketIamPolicy(
      context, TestOptions(),
      storage::internal::GetBucketIamPolicyRequest("test-bucket")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, SetNativeBucketIamPolicy) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, SetIamPolicy)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       google::iam::v1::SetIamPolicyRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.resource(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->SetNativeBucketIamPolicy(
      context, TestOptions(),
      storage::internal::SetNativeBucketIamPolicyRequest(
          "test-bucket", storage::NativeIamPolicy(/*bindings=*/{}, /*etag=*/{}))
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, TestBucketIamPermissions) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, TestIamPermissions)
      .WillOnce(
          [this](grpc::ClientContext& context, Options const&,
                 google::iam::v1::TestIamPermissionsRequest const& request) {
            auto metadata = GetMetadata(context);
            EXPECT_THAT(metadata,
                        UnorderedElementsAre(
                            Pair(kIdempotencyTokenHeader, "test-token-1234"),
                            Pair("x-goog-quota-user", "test-quota-user"),
                            Pair("x-goog-fieldmask", "field1,field2")));
            EXPECT_THAT(request.resource(), "projects/_/buckets/test-bucket");
            return PermanentError();
          });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->TestBucketIamPermissions(
      context, TestOptions(),
      storage::internal::TestBucketIamPermissionsRequest(
          "test-bucket", {"test.permission.1", "test.permission.2"})
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, InsertObjectMedia) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, WriteObject)
      .WillOnce([this](auto context, Options const&) {
        auto metadata = GetMetadata(*context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2"),
                        Pair("x-goog-request-params",
                             "bucket=projects%2F_%2Fbuckets%2Ftest-bucket")));
        ::testing::InSequence sequence;
        auto stream = std::make_unique<MockInsertStream>();
        EXPECT_CALL(*stream, Write).WillOnce(Return(false));
        EXPECT_CALL(*stream, Close).WillOnce(Return(PermanentError()));
        return stream;
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->InsertObjectMedia(
      context, TestOptions(),
      storage::internal::InsertObjectMediaRequest(
          "test-bucket", "test-object", "How vexingly quick daft zebras jump!")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, CopyObject) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, RewriteObject)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::RewriteObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.source_bucket(),
                    "projects/_/buckets/test-source-bucket");
        EXPECT_THAT(request.source_object(), "test-source-object");
        EXPECT_THAT(request.destination_bucket(),
                    "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.destination_name(), "test-object");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->CopyObject(
      context, TestOptions(),
      storage::internal::CopyObjectRequest("test-source-bucket",
                                           "test-source-object", "test-bucket",
                                           "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, CopyObjectTooLarge) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, RewriteObject)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::RewriteObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
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
  auto context = TestContext();
  auto response = client->CopyObject(
      context, TestOptions(),
      storage::internal::CopyObjectRequest("test-source-bucket",
                                           "test-source-object", "test-bucket",
                                           "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_THAT(response.status(), StatusIs(StatusCode::kOutOfRange));
}

TEST_F(GrpcClientTest, GetObjectMetadata) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, GetObject)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::GetObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.object(), "test-object");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->GetObjectMetadata(
      context, TestOptions(),
      storage::internal::GetObjectMetadataRequest("test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, ReadObject) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, ReadObject)
      .WillOnce([this](auto context, Options const& options,
                       v2::ReadObjectRequest const& request) {
        auto metadata = GetMetadata(*context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(options.get<UserAgentProductsOption>(),
                    Contains("test-only/1.2.3"));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.object(), "test-object");
        return std::make_unique<storage::testing::MockObjectMediaStream>();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto stream = client->ReadObject(
      context, TestOptions().set<UserAgentProductsOption>({"test-only/1.2.3"}),
      storage::internal::ReadObjectRangeRequest("test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
}

TEST_F(GrpcClientTest, ListObjects) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::ListObjectsRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.parent(), "projects/_/buckets/test-bucket");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->ListObjects(
      context, TestOptions(),
      storage::internal::ListObjectsRequest("test-bucket")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, DeleteObject) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce(
          [this](grpc::ClientContext& context, Options const&,
                 google::storage::v2::DeleteObjectRequest const& request) {
            auto metadata = GetMetadata(context);
            EXPECT_THAT(metadata,
                        UnorderedElementsAre(
                            Pair(kIdempotencyTokenHeader, "test-token-1234"),
                            Pair("x-goog-quota-user", "test-quota-user"),
                            Pair("x-goog-fieldmask", "field1,field2")));
            EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket");
            EXPECT_THAT(request.object(), "test-object");
            return PermanentError();
          });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->DeleteObject(
      context, TestOptions(),
      storage::internal::DeleteObjectRequest("test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, UpdateObject) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, UpdateObject)
      .WillOnce(
          [this](grpc::ClientContext& context, Options const&,
                 google::storage::v2::UpdateObjectRequest const& request) {
            auto metadata = GetMetadata(context);
            EXPECT_THAT(metadata,
                        UnorderedElementsAre(
                            Pair(kIdempotencyTokenHeader, "test-token-1234"),
                            Pair("x-goog-quota-user", "test-quota-user"),
                            Pair("x-goog-fieldmask", "field1,field2")));
            EXPECT_THAT(request.object().bucket(),
                        "projects/_/buckets/test-bucket");
            EXPECT_THAT(request.object().name(), "test-object");
            return PermanentError();
          });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->UpdateObject(
      context, TestOptions(),
      storage::internal::UpdateObjectRequest(
          "test-bucket", "test-object",
          // Typically, the metadata is first read from the
          // service as part of an OCC loop. For this test, just
          // use the default values for all fields
          ObjectMetadata{})
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, PatchObject) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, UpdateObject)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::UpdateObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.object().bucket(),
                    "projects/_/buckets/test-source-bucket");
        EXPECT_THAT(request.object().name(), "test-source-object");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->PatchObject(
      context, TestOptions(),
      storage::internal::PatchObjectRequest(
          "test-source-bucket", "test-source-object",
          storage::ObjectMetadataPatchBuilder{}.SetCacheControl("no-cache"))
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, ComposeObject) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::ComposeObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.destination().bucket(),
                    "projects/_/buckets/test-source-bucket");
        EXPECT_THAT(request.destination().name(), "test-source-object");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->ComposeObject(
      context, TestOptions(),
      storage::internal::ComposeObjectRequest("test-source-bucket", {},
                                              "test-source-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, RewriteObject) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, RewriteObject)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::RewriteObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.source_bucket(),
                    "projects/_/buckets/test-source-bucket");
        EXPECT_THAT(request.source_object(), "test-source-object");
        EXPECT_THAT(request.destination_bucket(),
                    "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.destination_name(), "test-object");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->RewriteObject(
      context, TestOptions(),
      storage::internal::RewriteObjectRequest(
          "test-source-bucket", "test-source-object", "test-bucket",
          "test-object", "test-token")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, RestoreObject) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, RestoreObject)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::RestoreObjectRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.bucket(), "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.object(), "test-object");
        EXPECT_THAT(request.generation(), 1234);
        EXPECT_THAT(request.copy_source_acl(), false);
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->RestoreObject(
      context, TestOptions(),
      storage::internal::RestoreObjectRequest("test-bucket", "test-object",
                                              1234)
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, CreateResumableUpload) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, StartResumableWrite)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::StartResumableWriteRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.write_object_spec().resource().bucket(),
                    "projects/_/buckets/test-bucket");
        EXPECT_THAT(request.write_object_spec().resource().name(),
                    "test-object");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->CreateResumableUpload(
      context, TestOptions(),
      storage::internal::ResumableUploadRequest("test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, GetServiceAccount) {
  auto mock = std::make_shared<MockStorageStub>();
  EXPECT_CALL(*mock, GetServiceAccount)
      .WillOnce([this](grpc::ClientContext& context, Options const&,
                       v2::GetServiceAccountRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair(kIdempotencyTokenHeader, "test-token-1234"),
                        Pair("x-goog-quota-user", "test-quota-user"),
                        Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.project(), "projects/test-project-id");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto context = TestContext();
  auto response = client->GetServiceAccount(
      context, TestOptions(),
      storage::internal::GetProjectServiceAccountRequest("test-project-id")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

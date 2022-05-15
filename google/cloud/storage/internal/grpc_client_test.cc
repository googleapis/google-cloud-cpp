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
#include "google/cloud/storage/grpc_plugin.h"
#include "google/cloud/storage/testing/mock_storage_stub.h"
#include "google/cloud/credentials.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/validate_metadata.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

namespace v2 = ::google::storage::v2;
using ::google::cloud::internal::CurrentOptions;
using ::google::cloud::storage_experimental::GrpcPluginOption;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
using ::google::protobuf::TextFormat;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

auto constexpr kAuthority = "storage.googleapis.com";
auto constexpr kBucketProtoText = R"pb(
  name: "projects/_/buckets/test-bucket-id"
  bucket_id: "test-bucket-id"
  project: "projects/123456"
  metageneration: 1234567
  location: "test-location"
  location_type: "REGIONAL"
  storage_class: "test-storage-class"
  rpo: "test-rpo"
  acl: { role: "test-role1" entity: "test-entity1" }
  acl: { role: "test-role2" entity: "test-entity2" }
  default_object_acl: { role: "test-role3" entity: "test-entity3" }
  default_object_acl: { role: "test-role4" entity: "test-entity4" }
  lifecycle {
    rule {
      action { type: "Delete" }
      condition {
        age_days: 90
        is_live: false
        matches_storage_class: "NEARLINE"
      }
    }
    rule {
      action { type: "SetStorageClass" storage_class: "NEARLINE" }
      condition { age_days: 7 is_live: true matches_storage_class: "STANDARD" }
    }
  }
  create_time: { seconds: 1565194924 nanos: 123456000 }
  cors: {
    origin: "test-origin-0"
    origin: "test-origin-1"
    method: "GET"
    method: "PUT"
    response_header: "test-header-0"
    response_header: "test-header-1"
    max_age_seconds: 1800
  }
  cors: {
    origin: "test-origin-2"
    origin: "test-origin-3"
    method: "POST"
    response_header: "test-header-3"
    max_age_seconds: 3600
  }
  update_time: { seconds: 1565194925 nanos: 123456000 }
  default_event_based_hold: true
  labels: { key: "test-key-1" value: "test-value-1" }
  labels: { key: "test-key-2" value: "test-value-2" }
  website { main_page_suffix: "index.html" not_found_page: "404.html" }
  versioning { enabled: true }
  logging {
    log_bucket: "test-log-bucket"
    log_object_prefix: "test-log-object-prefix"
  }
  owner { entity: "test-entity" entity_id: "test-entity-id" }
  encryption { default_kms_key: "test-default-kms-key-name" }
  billing { requester_pays: true }
  retention_policy {
    effective_time { seconds: 1565194926 nanos: 123456000 }
    is_locked: true
    retention_period: 86400
  }
  iam_config {
    uniform_bucket_level_access {
      enabled: true
      lock_time { seconds: 1565194927 nanos: 123456000 }
    }
    public_access_prevention: "inherited"
  }
)pb";

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

TEST_F(GrpcClientTest, DefaultOptionsGrpcNoEnv) {
  auto grpc_config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", absl::nullopt);
  auto opts = DefaultOptionsGrpc();
  EXPECT_EQ(opts.get<GrpcPluginOption>(), "none");
  opts = DefaultOptionsGrpc(Options{}.set<GrpcPluginOption>("configured"));
  EXPECT_EQ(opts.get<GrpcPluginOption>(), "configured");
}

TEST_F(GrpcClientTest, DefaultOptionsGrpcWithEnv) {
  auto grpc_config =
      ScopedEnvironment("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", "env");
  auto opts = DefaultOptionsGrpc();
  EXPECT_EQ(opts.get<GrpcPluginOption>(), "env");
  opts = DefaultOptionsGrpc(Options{}.set<GrpcPluginOption>("configured"));
  EXPECT_EQ(opts.get<GrpcPluginOption>(), "configured");
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
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
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

TEST_F(GrpcClientTest, CreateResumableUpload) {
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
  auto response = client->CreateResumableUpload(
      ResumableUploadRequest("test-bucket", "test-object")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, ListBucketAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        EXPECT_THAT(request.common_request_params().user_project(),
                    "test-user-project");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->ListBucketAcl(
      ListBucketAclRequest("test-bucket-name")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, ListBucketAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const&) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->ListBucketAcl(ListBucketAclRequest("test-bucket-id"));
  ASSERT_STATUS_OK(response);
  auto make_matcher = [](std::string const& role, std::string const& entity) {
    auto get_role = [](BucketAccessControl const& acl) { return acl.role(); };
    auto get_entity = [](BucketAccessControl const& acl) {
      return acl.entity();
    };
    auto get_bucket = [](BucketAccessControl const& acl) {
      return acl.bucket();
    };
    return AllOf(ResultOf(get_role, role), ResultOf(get_entity, entity),
                 ResultOf(get_bucket, "test-bucket-id"));
  };
  EXPECT_THAT(response->items,
              UnorderedElementsAre(make_matcher("test-role1", "test-entity1"),
                                   make_matcher("test-role2", "test-entity2")));
}

TEST_F(GrpcClientTest, GetBucketAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        EXPECT_THAT(request.common_request_params().user_project(),
                    "test-user-project");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->GetBucketAcl(
      GetBucketAclRequest("test-bucket-name", "test-entity1")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, GetBucketAclNotFound) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const&) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->GetBucketAcl(
      GetBucketAclRequest("test-bucket-id", "test-not-found"));
  EXPECT_THAT(response, StatusIs(StatusCode::kNotFound));
}

TEST_F(GrpcClientTest, GetBucketAclSuccess) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const&) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });

  auto client = CreateTestClient(mock);
  auto response = client->GetBucketAcl(
      GetBucketAclRequest("test-bucket-id", "test-entity1"));
  ASSERT_STATUS_OK(response);
  EXPECT_EQ(response->entity(), "test-entity1");
  EXPECT_EQ(response->role(), "test-role1");
  EXPECT_EQ(response->bucket(), "test-bucket-id");
}

TEST_F(GrpcClientTest, CreateBucketAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        EXPECT_THAT(request.common_request_params().user_project(),
                    "test-user-project");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->CreateBucketAcl(
      CreateBucketAclRequest("test-bucket-name", "test-entity1", "test-role1")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, CreateBucketAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const&) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateBucketRequest const& request) {
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        auto expected = v2::BucketAccessControl();
        expected.set_entity("test-new-entity");
        expected.set_role("test-new-role");
        EXPECT_THAT(request.bucket().acl(), Contains(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->CreateBucketAcl(CreateBucketAclRequest(
      "test-bucket-id", "test-new-entity", "test-new-role"));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientTest, CreateBucketAclAlreadyExists) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const&) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket).Times(0);

  auto client = CreateTestClient(mock);
  auto response = client->CreateBucketAcl(CreateBucketAclRequest(
      "test-bucket-id", "test-entity1", "test-new-role"));
  EXPECT_THAT(response, StatusIs(StatusCode::kAlreadyExists));
}

TEST_F(GrpcClientTest, DeleteBucketAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        EXPECT_THAT(request.common_request_params().user_project(),
                    "test-user-project");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->DeleteBucketAcl(
      DeleteBucketAclRequest("test-bucket-name", "test-entity1")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, DeleteBucketAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const&) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateBucketRequest const& request) {
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        auto expected = v2::BucketAccessControl();
        expected.set_entity("test-entity2");
        expected.set_role("test-role2");
        EXPECT_THAT(request.bucket().acl(),
                    ElementsAre(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->DeleteBucketAcl(
      DeleteBucketAclRequest("test-bucket-id", "test-entity1"));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientTest, DeleteBucketAclNotFound) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const&) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket).Times(0);

  auto client = CreateTestClient(mock);
  auto response = client->DeleteBucketAcl(
      DeleteBucketAclRequest("test-bucket-id", "test-not-found"));
  EXPECT_THAT(response, StatusIs(StatusCode::kNotFound));
}

TEST_F(GrpcClientTest, UpdateBucketAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        EXPECT_THAT(request.common_request_params().user_project(),
                    "test-user-project");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->UpdateBucketAcl(
      UpdateBucketAclRequest("test-bucket-name", "test-entity1", "updated-role")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, UpdateBucketAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const&) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateBucketRequest const& request) {
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        auto expected = v2::BucketAccessControl();
        expected.set_entity("test-entity1");
        expected.set_role("updated-role");
        EXPECT_THAT(request.bucket().acl(), Contains(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->UpdateBucketAcl(
      UpdateBucketAclRequest("test-bucket-id", "test-entity1", "updated-role"));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientTest, UpdateBucketAclNotFound) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const&) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket).Times(0);

  auto client = CreateTestClient(mock);
  auto response = client->UpdateBucketAcl(UpdateBucketAclRequest(
      "test-bucket-id", "test-not-found", "updated-role"));
  EXPECT_THAT(response, StatusIs(StatusCode::kNotFound));
}

TEST_F(GrpcClientTest, PatchBucketAclFailure) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetBucketRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(), "projects/_/buckets/test-bucket-name");
        EXPECT_THAT(request.common_request_params().user_project(),
                    "test-user-project");
        return PermanentError();
      });

  auto client = CreateTestClient(mock);
  auto response = client->PatchBucketAcl(
      PatchBucketAclRequest(
          "test-bucket-name", "test-entity1",
          BucketAccessControlPatchBuilder().set_role("updated-role"))
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user"),
                                UserProject("test-user-project")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, PatchBucketAclPatchFails) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const&) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket)
      .WillOnce([](grpc::ClientContext&,
                   v2::UpdateBucketRequest const& request) {
        EXPECT_EQ(request.bucket().name(), "projects/_/buckets/test-bucket-id");
        auto expected = v2::BucketAccessControl();
        expected.set_entity("test-entity1");
        expected.set_role("updated-role");
        EXPECT_THAT(request.bucket().acl(), Contains(IsProtoEqual(expected)));
        EXPECT_THAT(request.update_mask().paths(), ElementsAre("acl"));
        return Status(StatusCode::kFailedPrecondition, "conflict");
      });

  auto client = CreateTestClient(mock);
  auto response = client->PatchBucketAcl(PatchBucketAclRequest(
      "test-bucket-id", "test-entity1",
      BucketAccessControlPatchBuilder().set_role("updated-role")));
  EXPECT_THAT(response, StatusIs(StatusCode::kUnavailable));
}

TEST_F(GrpcClientTest, PatchBucketAclNotFound) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetBucket)
      .WillOnce([&](grpc::ClientContext&, v2::GetBucketRequest const&) {
        v2::Bucket response;
        EXPECT_TRUE(TextFormat::ParseFromString(kBucketProtoText, &response));
        return response;
      });
  EXPECT_CALL(*mock, UpdateBucket).Times(0);

  auto client = CreateTestClient(mock);
  auto response = client->PatchBucketAcl(PatchBucketAclRequest(
      "test-bucket-id", "test-not-found",
      BucketAccessControlPatchBuilder().set_role("updated-role")));
  EXPECT_THAT(response, StatusIs(StatusCode::kNotFound));
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

TEST_F(GrpcClientTest, CreateHmacKey) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, CreateHmacKey)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::CreateHmacKeyRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.project(), "projects/test-project-id");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->CreateHmacKey(
      CreateHmacKeyRequest("test-project-id", "test-service-account-email")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, DeleteHmacKey) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, DeleteHmacKey)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::DeleteHmacKeyRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.project(), "projects/test-project-id");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->DeleteHmacKey(
      DeleteHmacKeyRequest("test-project-id", "test-access-id")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, GetHmacKey) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetHmacKey)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetHmacKeyRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.project(), "projects/test-project-id");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->GetHmacKey(
      GetHmacKeyRequest("test-project-id", "test-access-id")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, ListHmacKeys) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, ListHmacKeys)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::ListHmacKeysRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.project(), "projects/test-project-id");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->ListHmacKeys(
      ListHmacKeysRequest("test-project-id")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, UpdateHmacKey) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, UpdateHmacKey)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::UpdateHmacKeyRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), kAuthority);
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.hmac_key().project(), "projects/test-project-id");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->UpdateHmacKey(
      UpdateHmacKeyRequest(
          "test-project-id", "test-access-id",
          HmacKeyMetadata().set_state(HmacKeyMetadata::state_deleted()))
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

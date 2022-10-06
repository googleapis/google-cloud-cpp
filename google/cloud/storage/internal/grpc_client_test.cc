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
#include "google/cloud/grpc_options.h"
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
using ::google::cloud::testing_util::ScopedEnvironment;
using ::google::cloud::testing_util::StatusIs;
using ::google::cloud::testing_util::ValidateMetadataFixture;
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

Status PermanentError() {
  return Status(StatusCode::kPermissionDenied, "uh-oh");
}

google::cloud::Options TestOptions() {
  return Options{}.set<UnifiedCredentialsOption>(MakeInsecureCredentials());
}

std::shared_ptr<GrpcClient> CreateTestClient(
    std::shared_ptr<storage_internal::StorageStub> stub) {
  return GrpcClient::CreateMock(std::move(stub), TestOptions());
}

TEST_F(GrpcClientTest, DefaultOptionsGrpcChannelCount) {
  using ::google::cloud::GrpcNumChannelsOption;
  struct TestCase {
    std::string endpoint;
    int lower_bound;
    int upper_bound;
  } cases[] = {
      {"storage.googleapis.com", 4, std::numeric_limits<int>::max()},
      {"google-c2p:///storage.googleapis.com", 1, 1},
      {"google-c2p-experimental:///storage.googleapis.com", 1, 1},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing with " + test.endpoint);
    auto opts =
        DefaultOptionsGrpc(TestOptions().set<EndpointOption>(test.endpoint));
    auto const count = opts.get<GrpcNumChannelsOption>();
    EXPECT_LE(test.lower_bound, count);
    EXPECT_GE(test.upper_bound, count);

    auto override = DefaultOptionsGrpc(TestOptions()
                                           .set<EndpointOption>(test.endpoint)
                                           .set<GrpcNumChannelsOption>(42));
    EXPECT_EQ(42, override.get<GrpcNumChannelsOption>());
  }
}

TEST_F(GrpcClientTest, DefaultOptionsGrpcEndpointNoEnv) {
  auto expected = std::string("storage.googleapis.com");
  auto alternatives = [](std::string const& value) {
    return std::vector<absl::optional<std::string>>{absl::nullopt, value};
  };

  for (auto const& opt : alternatives("from-option")) {
    SCOPED_TRACE("Testing with opt " + opt.value_or("<unset>"));
    auto options = TestOptions();
    if (opt.has_value()) {
      expected = *opt;
      options.set<EndpointOption>(*opt);
    }
    for (auto const& env : alternatives("from-env")) {
      SCOPED_TRACE("Testing with env " + opt.value_or("<unset>"));
      auto setenv = ScopedEnvironment(
          "CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT", env);
      if (env.has_value()) expected = *env;
      auto actual = DefaultOptionsGrpc(options);
      EXPECT_EQ(actual.get<EndpointOption>(), expected);
    }
  }
}

TEST_F(GrpcClientTest, QueryResumableUpload) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, QueryWriteStatus)
      .WillOnce([this](grpc::ClientContext& context,
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
  auto client = CreateTestClient(mock);
  auto response = client->QueryResumableUpload(
      QueryResumableUploadRequest("test-only-upload-id")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, DeleteResumableUpload) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, CancelResumableWrite)
      .WillOnce(
          [this](
              grpc::ClientContext& context,
              google::storage::v2::CancelResumableWriteRequest const& request) {
            auto metadata = GetMetadata(context);
            EXPECT_THAT(metadata,
                        UnorderedElementsAre(
                            Pair("x-goog-quota-user", "test-quota-user"),
                            Pair("x-goog-fieldmask", "field1,field2")));
            EXPECT_THAT(request.upload_id(), "test-only-upload-id");
            return PermanentError();
          });
  auto client = CreateTestClient(mock);
  auto response = client->DeleteResumableUpload(
      DeleteResumableUploadRequest("test-only-upload-id")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, UploadChunk) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, WriteObject)
      .WillOnce([this](std::unique_ptr<grpc::ClientContext> context) {
        auto metadata = GetMetadata(*context);
        EXPECT_THAT(metadata,
                    UnorderedElementsAre(
                        Pair("x-goog-quota-user", "test-quota-user"),
                        // Map JSON names to the `resource` subobject
                        Pair("x-goog-fieldmask", "resource(field1,field2)"),
                        Pair("x-goog-request-params",
                             "bucket=projects/_/buckets/test-bucket")));
        ::testing::InSequence sequence;
        auto stream = absl::make_unique<testing::MockInsertStream>();
        EXPECT_CALL(*stream, Write).WillOnce(Return(false));
        EXPECT_CALL(*stream, Close).WillOnce(Return(PermanentError()));
        return stream;
      });
  auto client = CreateTestClient(mock);
  auto response = client->UploadChunk(
      UploadChunkRequest("projects/_/buckets/test-bucket/test-upload-id", 0, {})
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
                        Pair("x-goog-fieldmask", "resource(field1,field2)"),
                        Pair("x-goog-request-params",
                             "bucket=projects/_/buckets/test-bucket")));
        ::testing::InSequence sequence;
        auto stream = absl::make_unique<testing::MockInsertStream>();
        EXPECT_CALL(*stream, Write).WillOnce(Return(false));
        EXPECT_CALL(*stream, Close).WillOnce(Return(PermanentError()));
        return stream;
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
        return absl::make_unique<testing::MockObjectMediaStream>();
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

TEST_F(GrpcClientTest, GetServiceAccount) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetServiceAccount)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetServiceAccountRequest const& request) {
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

TEST_F(GrpcClientTest, ListNotifications) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, ListNotifications)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::ListNotificationsRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.parent(), "projects/_/buckets/test-bucket-name");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->ListNotifications(
      ListNotificationsRequest("test-bucket-name")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, CreateNotification) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, CreateNotification)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::CreateNotificationRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.parent(), "projects/_/buckets/test-bucket-name");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->CreateNotification(
      CreateNotificationRequest("test-bucket-name", NotificationMetadata())
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, GetNotification) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, GetNotification)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::GetNotificationRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(),
                    "projects/_/buckets/test-bucket-name/notificationConfigs/"
                    "test-notification-id");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->GetNotification(
      GetNotificationRequest("test-bucket-name", "test-notification-id")
          .set_multiple_options(Fields("field1,field2"),
                                QuotaUser("test-quota-user")));
  EXPECT_EQ(response.status(), PermanentError());
}

TEST_F(GrpcClientTest, DeleteNotification) {
  auto mock = std::make_shared<testing::MockStorageStub>();
  EXPECT_CALL(*mock, DeleteNotification)
      .WillOnce([this](grpc::ClientContext& context,
                       v2::DeleteNotificationRequest const& request) {
        auto metadata = GetMetadata(context);
        EXPECT_THAT(metadata, UnorderedElementsAre(
                                  Pair("x-goog-quota-user", "test-quota-user"),
                                  Pair("x-goog-fieldmask", "field1,field2")));
        EXPECT_THAT(request.name(),
                    "projects/_/buckets/test-bucket-name/notificationConfigs/"
                    "test-notification-id");
        return PermanentError();
      });
  auto client = CreateTestClient(mock);
  auto response = client->DeleteNotification(
      DeleteNotificationRequest("test-bucket-name", "test-notification-id")
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

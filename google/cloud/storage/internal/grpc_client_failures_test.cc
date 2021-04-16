// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/internal/grpc_resumable_upload_session_url.h"
#include "google/cloud/storage/internal/hybrid_client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::AnyOf;

/**
 * @test Verify GrpcClient and HybridClient report failures correctly.
 *
 * TODO(milestone/22) - only accept `kUnavailable` as valid.
 */
class GrpcClientFailuresTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::string> {
 protected:
  GrpcClientFailuresTest()
      : grpc_config_("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", {}),
        rest_endpoint_("CLOUD_STORAGE_EMULATOR_ENDPOINT", {}),
        grpc_endpoint_("CLOUD_STORAGE_GRPC_ENDPOINT", {}) {}

  void SetUp() override {
    std::string const grpc_config = GetParam();
    google::cloud::internal::SetEnv("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                    grpc_config);
    auto options =
        Options{}
            .set<internal::RestEndpointOption>("http://localhost:1")
            .set<internal::IamEndpointOption>("http://localhost:1")
            .set<EndpointOption>("localhost:1")
            .set<internal::Oauth2CredentialsOption>(
                oauth2::CreateAnonymousCredentials())
            .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials());
    if (grpc_config == "metadata") {
      client_ = GrpcClient::Create(std::move(options));
    } else {
      client_ = HybridClient::Create(std::move(options));
    }
  }

  testing_util::ScopedEnvironment grpc_config_;
  testing_util::ScopedEnvironment rest_endpoint_;
  testing_util::ScopedEnvironment grpc_endpoint_;
  std::shared_ptr<internal::RawClient> client_;
};

TEST_P(GrpcClientFailuresTest, ListBuckets) {
  auto actual = client_->ListBuckets(ListBucketsRequest{"project_id"});
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, CreateBucket) {
  auto actual = client_->CreateBucket(
      CreateBucketRequest("bkt", BucketMetadata().set_name("bkt")));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, GetBucketMetadata) {
  auto actual = client_->GetBucketMetadata(GetBucketMetadataRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, DeleteBucket) {
  auto actual = client_->DeleteBucket(DeleteBucketRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, UpdateBucket) {
  auto actual = client_->UpdateBucket(
      UpdateBucketRequest(BucketMetadata().set_name("bkt")));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, PatchBucket) {
  auto actual = client_->PatchBucket(
      PatchBucketRequest("bkt", BucketMetadata().set_name("bkt"),
                         BucketMetadata().set_name("bkt")));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, GetBucketIamPolicy) {
  auto actual = client_->GetBucketIamPolicy(GetBucketIamPolicyRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, GetNativeBucketIamPolicy) {
  auto actual =
      client_->GetNativeBucketIamPolicy(GetBucketIamPolicyRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable)));
}

TEST_P(GrpcClientFailuresTest, SetBucketIamPolicy) {
  auto actual = client_->SetBucketIamPolicy(
      SetBucketIamPolicyRequest("bkt", google::cloud::IamPolicy{}));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, SetNativeBucketIamPolicy) {
  auto actual =
      client_->SetNativeBucketIamPolicy(SetNativeBucketIamPolicyRequest(
          "bkt", NativeIamPolicy(std::vector<NativeIamBinding>())));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable)));
}

TEST_P(GrpcClientFailuresTest, TestBucketIamPermissions) {
  auto actual = client_->TestBucketIamPermissions(
      TestBucketIamPermissionsRequest("bkt", {}));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable)));
}

TEST_P(GrpcClientFailuresTest, LockBucketRetentionPolicy) {
  auto actual = client_->LockBucketRetentionPolicy(
      LockBucketRetentionPolicyRequest("bkt", 0));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, InsertObjectMediaSimple) {
  auto actual = client_->InsertObjectMedia(
      InsertObjectMediaRequest("bkt", "obj", "contents")
          .set_multiple_options(DisableMD5Hash(true),
                                DisableCrc32cChecksum(true)));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, InsertObjectMediaMultipart) {
  auto actual = client_->InsertObjectMedia(
      InsertObjectMediaRequest("bkt", "obj", "contents"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, InsertObjectMediaXml) {
  auto actual = client_->InsertObjectMedia(
      InsertObjectMediaRequest("bkt", "obj", "contents")
          .set_multiple_options(Fields("")));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, GetObjectMetadata) {
  auto actual =
      client_->GetObjectMetadata(GetObjectMetadataRequest("bkt", "obj"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, ListObjects) {
  auto actual = client_->ListObjects(ListObjectsRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, DeleteObject) {
  auto actual = client_->DeleteObject(DeleteObjectRequest("bkt", "obj"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, UpdateObject) {
  auto actual = client_->UpdateObject(
      UpdateObjectRequest("bkt", "obj", ObjectMetadata()));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, PatchObject) {
  auto actual = client_->PatchObject(
      PatchObjectRequest("bkt", "obj", ObjectMetadata(), ObjectMetadata()));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, ComposeObject) {
  auto actual = client_->ComposeObject(ComposeObjectRequest("bkt", {}, "obj"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, ListBucketAcl) {
  auto actual = client_->ListBucketAcl(ListBucketAclRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, CopyObject) {
  auto actual =
      client_->CopyObject(CopyObjectRequest("bkt", "obj1", "bkt", "obj2"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, CreateBucketAcl) {
  auto actual =
      client_->CreateBucketAcl(CreateBucketAclRequest("bkt", "entity", "role"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, GetBucketAcl) {
  auto actual = client_->GetBucketAcl(GetBucketAclRequest("bkt", "entity"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, DeleteBucketAcl) {
  auto actual =
      client_->DeleteBucketAcl(DeleteBucketAclRequest("bkt", "entity"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, UpdateBucketAcl) {
  auto actual =
      client_->UpdateBucketAcl(UpdateBucketAclRequest("bkt", "entity", "role"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, PatchBucketAcl) {
  auto actual = client_->PatchBucketAcl(PatchBucketAclRequest(
      "bkt", "entity", BucketAccessControl(), BucketAccessControl()));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, ListObjectAcl) {
  auto actual = client_->ListObjectAcl(ListObjectAclRequest("bkt", "obj"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, CreateObjectAcl) {
  auto actual = client_->CreateObjectAcl(
      CreateObjectAclRequest("bkt", "obj", "entity", "role"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, DeleteObjectAcl) {
  auto actual =
      client_->DeleteObjectAcl(DeleteObjectAclRequest("bkt", "obj", "entity"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, GetObjectAcl) {
  auto actual =
      client_->GetObjectAcl(GetObjectAclRequest("bkt", "obj", "entity"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, UpdateObjectAcl) {
  auto actual = client_->UpdateObjectAcl(
      UpdateObjectAclRequest("bkt", "obj", "entity", "role"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, PatchObjectAcl) {
  auto actual = client_->PatchObjectAcl(PatchObjectAclRequest(
      "bkt", "obj", "entity", ObjectAccessControl(), ObjectAccessControl()));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, RewriteObject) {
  auto actual = client_->RewriteObject(
      RewriteObjectRequest("bkt", "obj", "bkt2", "obj2", "token"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, CreateResumableSession) {
  auto actual = client_->CreateResumableSession(
      ResumableUploadRequest("test-bucket", "test-object"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, DeleteResumableUpload) {
  auto actual = client_->DeleteResumableUpload(DeleteResumableUploadRequest(
      EncodeGrpcResumableUploadSessionUrl(ResumableUploadSessionGrpcParams{
          "test-bucket", "test-object", "test-upload-id"})));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable)));
}

TEST_P(GrpcClientFailuresTest, ListDefaultObjectAcl) {
  auto actual =
      client_->ListDefaultObjectAcl(ListDefaultObjectAclRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, CreateDefaultObjectAcl) {
  auto actual = client_->CreateDefaultObjectAcl(
      CreateDefaultObjectAclRequest("bkt", "entity", "role"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, DeleteDefaultObjectAcl) {
  auto actual = client_->DeleteDefaultObjectAcl(
      DeleteDefaultObjectAclRequest("bkt", "entity"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, GetDefaultObjectAcl) {
  auto actual =
      client_->GetDefaultObjectAcl(GetDefaultObjectAclRequest("bkt", "entity"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, UpdateDefaultObjectAcl) {
  auto actual = client_->UpdateDefaultObjectAcl(
      UpdateDefaultObjectAclRequest("bkt", "entity", "role"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, PatchDefaultObjectAcl) {
  auto actual = client_->PatchDefaultObjectAcl(PatchDefaultObjectAclRequest(
      "bkt", "entity", ObjectAccessControl(), ObjectAccessControl()));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, GetServiceAccount) {
  auto actual =
      client_->GetServiceAccount(GetProjectServiceAccountRequest("project_id"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, ListHmacKeyRequest) {
  auto actual = client_->ListHmacKeys(ListHmacKeysRequest("project_id"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, CreateHmacKeyRequest) {
  auto actual = client_->CreateHmacKey(
      CreateHmacKeyRequest("project_id", "service-account"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, SignBlob) {
  auto actual = client_->SignBlob(
      SignBlobRequest("test-service-account", "test-blob", {}));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, ListNotifications) {
  auto actual = client_->ListNotifications(ListNotificationsRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, CreateNotification) {
  auto actual = client_->CreateNotification(
      CreateNotificationRequest("bkt", NotificationMetadata()));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, GetNotification) {
  auto actual = client_->GetNotification(
      GetNotificationRequest("bkt", "notification_id"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

TEST_P(GrpcClientFailuresTest, DeleteNotification) {
  auto actual = client_->DeleteNotification(
      DeleteNotificationRequest("bkt", "notification_id"));
  EXPECT_THAT(actual, StatusIs(AnyOf(StatusCode::kUnavailable,
                                     StatusCode::kUnimplemented)));
}

INSTANTIATE_TEST_SUITE_P(HybridClientFailures, GrpcClientFailuresTest,
                         ::testing::Values("media"));

INSTANTIATE_TEST_SUITE_P(GrpcClientFailures, GrpcClientFailuresTest,
                         ::testing::Values("metadata"));

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

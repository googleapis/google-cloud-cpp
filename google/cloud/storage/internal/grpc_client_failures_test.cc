// Copyright 2020 Google LLC
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
#include "google/cloud/storage/internal/hybrid_client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/setenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::OptionsSpan;
using ::google::cloud::testing_util::StatusIs;

/**
 * @test Verify GrpcClient and HybridClient report failures correctly.
 */
class GrpcClientFailuresTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::string> {
 protected:
  GrpcClientFailuresTest()
      : grpc_config_("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG", {}),
        rest_endpoint_("CLOUD_STORAGE_EMULATOR_ENDPOINT", {}),
        grpc_endpoint_("CLOUD_STORAGE_EXPERIMENTAL_GRPC_TESTBENCH_ENDPOINT",
                       {}) {}

  void SetUp() override {
    std::string const grpc_config = GetParam();
    google::cloud::testing_util::SetEnv("GOOGLE_CLOUD_CPP_STORAGE_GRPC_CONFIG",
                                        grpc_config);
    auto options =
        Options{}
            .set<RestEndpointOption>("http://localhost:1")
            .set<IamEndpointOption>("http://localhost:1")
            .set<EndpointOption>("localhost:1")
            .set<Oauth2CredentialsOption>(oauth2::CreateAnonymousCredentials())
            .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials());
    if (grpc_config == "metadata") {
      client_ = storage_internal::GrpcClient::Create(
          storage_internal::DefaultOptionsGrpc(std::move(options)));
    } else {
      client_ = storage_internal::HybridClient::Create(
          storage_internal::DefaultOptionsGrpc(std::move(options)));
    }
  }

  testing_util::ScopedEnvironment grpc_config_;
  testing_util::ScopedEnvironment rest_endpoint_;
  testing_util::ScopedEnvironment grpc_endpoint_;
  std::shared_ptr<internal::RawClient> client_;
};

TEST_P(GrpcClientFailuresTest, ListBuckets) {
  OptionsSpan const span(client_->options());
  auto actual = client_->ListBuckets(ListBucketsRequest{"project_id"});
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, CreateBucket) {
  OptionsSpan const span(client_->options());
  auto actual = client_->CreateBucket(
      CreateBucketRequest("bkt", BucketMetadata().set_name("bkt")));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetBucketMetadata) {
  OptionsSpan const span(client_->options());
  auto actual = client_->GetBucketMetadata(GetBucketMetadataRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, DeleteBucket) {
  OptionsSpan const span(client_->options());
  auto actual = client_->DeleteBucket(DeleteBucketRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, UpdateBucket) {
  OptionsSpan const span(client_->options());
  auto actual = client_->UpdateBucket(
      UpdateBucketRequest(BucketMetadata().set_name("bkt")));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, PatchBucket) {
  OptionsSpan const span(client_->options());
  auto actual = client_->PatchBucket(
      PatchBucketRequest("bkt", BucketMetadata().set_name("bkt"),
                         BucketMetadata().set_name("bkt")));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetNativeBucketIamPolicy) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->GetNativeBucketIamPolicy(GetBucketIamPolicyRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, SetNativeBucketIamPolicy) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->SetNativeBucketIamPolicy(SetNativeBucketIamPolicyRequest(
          "bkt", NativeIamPolicy(std::vector<NativeIamBinding>())));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, TestBucketIamPermissions) {
  OptionsSpan const span(client_->options());
  auto actual = client_->TestBucketIamPermissions(
      TestBucketIamPermissionsRequest("bkt", {}));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, LockBucketRetentionPolicy) {
  OptionsSpan const span(client_->options());
  auto actual = client_->LockBucketRetentionPolicy(
      LockBucketRetentionPolicyRequest("bkt", 0));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, InsertObjectMediaSimple) {
  OptionsSpan const span(client_->options());
  auto actual = client_->InsertObjectMedia(
      InsertObjectMediaRequest("bkt", "obj", "contents")
          .set_multiple_options(DisableMD5Hash(true),
                                DisableCrc32cChecksum(true)));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, InsertObjectMediaMultipart) {
  OptionsSpan const span(client_->options());
  auto actual = client_->InsertObjectMedia(
      InsertObjectMediaRequest("bkt", "obj", "contents"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, InsertObjectMedia) {
  OptionsSpan const span(client_->options());
  auto actual = client_->InsertObjectMedia(
      InsertObjectMediaRequest("bkt", "obj", "contents")
          .set_multiple_options(Fields("")));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetObjectMetadata) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->GetObjectMetadata(GetObjectMetadataRequest("bkt", "obj"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, ListObjects) {
  OptionsSpan const span(client_->options());
  auto actual = client_->ListObjects(ListObjectsRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, DeleteObject) {
  OptionsSpan const span(client_->options());
  auto actual = client_->DeleteObject(DeleteObjectRequest("bkt", "obj"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, UpdateObject) {
  OptionsSpan const span(client_->options());
  auto actual = client_->UpdateObject(
      UpdateObjectRequest("bkt", "obj", ObjectMetadata()));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, PatchObject) {
  OptionsSpan const span(client_->options());
  auto actual = client_->PatchObject(
      PatchObjectRequest("bkt", "obj", ObjectMetadata(), ObjectMetadata()));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, ComposeObject) {
  OptionsSpan const span(client_->options());
  auto actual = client_->ComposeObject(ComposeObjectRequest("bkt", {}, "obj"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, ListBucketAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_->ListBucketAcl(ListBucketAclRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, CopyObject) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->CopyObject(CopyObjectRequest("bkt", "obj1", "bkt", "obj2"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, CreateBucketAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->CreateBucketAcl(CreateBucketAclRequest("bkt", "entity", "role"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetBucketAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_->GetBucketAcl(GetBucketAclRequest("bkt", "entity"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, DeleteBucketAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->DeleteBucketAcl(DeleteBucketAclRequest("bkt", "entity"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, UpdateBucketAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->UpdateBucketAcl(UpdateBucketAclRequest("bkt", "entity", "role"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, PatchBucketAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_->PatchBucketAcl(PatchBucketAclRequest(
      "bkt", "entity", BucketAccessControl(), BucketAccessControl()));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, ListObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_->ListObjectAcl(ListObjectAclRequest("bkt", "obj"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, CreateObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_->CreateObjectAcl(
      CreateObjectAclRequest("bkt", "obj", "entity", "role"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, DeleteObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->DeleteObjectAcl(DeleteObjectAclRequest("bkt", "obj", "entity"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->GetObjectAcl(GetObjectAclRequest("bkt", "obj", "entity"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, UpdateObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_->UpdateObjectAcl(
      UpdateObjectAclRequest("bkt", "obj", "entity", "role"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, PatchObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_->PatchObjectAcl(PatchObjectAclRequest(
      "bkt", "obj", "entity", ObjectAccessControl(), ObjectAccessControl()));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, RewriteObject) {
  OptionsSpan const span(client_->options());
  auto actual = client_->RewriteObject(
      RewriteObjectRequest("bkt", "obj", "bkt2", "obj2", "token"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, CreateResumableUpload) {
  OptionsSpan const span(client_->options());
  auto actual = client_->CreateResumableUpload(
      ResumableUploadRequest("test-bucket", "test-object"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, DeleteResumableUpload) {
  OptionsSpan const span(client_->options());
  auto actual = client_->DeleteResumableUpload(
      DeleteResumableUploadRequest("test-upload-id"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, ListDefaultObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->ListDefaultObjectAcl(ListDefaultObjectAclRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, CreateDefaultObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_->CreateDefaultObjectAcl(
      CreateDefaultObjectAclRequest("bkt", "entity", "role"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, DeleteDefaultObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_->DeleteDefaultObjectAcl(
      DeleteDefaultObjectAclRequest("bkt", "entity"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetDefaultObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->GetDefaultObjectAcl(GetDefaultObjectAclRequest("bkt", "entity"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, UpdateDefaultObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_->UpdateDefaultObjectAcl(
      UpdateDefaultObjectAclRequest("bkt", "entity", "role"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, PatchDefaultObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_->PatchDefaultObjectAcl(PatchDefaultObjectAclRequest(
      "bkt", "entity", ObjectAccessControl(), ObjectAccessControl()));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetServiceAccount) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->GetServiceAccount(GetProjectServiceAccountRequest("project_id"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, ListHmacKeyRequest) {
  OptionsSpan const span(client_->options());
  auto actual = client_->ListHmacKeys(ListHmacKeysRequest("project_id"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, CreateHmacKeyRequest) {
  OptionsSpan const span(client_->options());
  auto actual = client_->CreateHmacKey(
      CreateHmacKeyRequest("project_id", "service-account"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, SignBlob) {
  OptionsSpan const span(client_->options());
  auto actual = client_->SignBlob(
      SignBlobRequest("test-service-account", "test-blob", {}));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, ListNotifications) {
  OptionsSpan const span(client_->options());
  auto actual = client_->ListNotifications(ListNotificationsRequest("bkt"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, CreateNotification) {
  OptionsSpan const span(client_->options());
  auto actual = client_->CreateNotification(
      CreateNotificationRequest("bkt", NotificationMetadata()));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, GetNotification) {
  OptionsSpan const span(client_->options());
  auto actual = client_->GetNotification(
      GetNotificationRequest("bkt", "notification_id"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

TEST_P(GrpcClientFailuresTest, DeleteNotification) {
  OptionsSpan const span(client_->options());
  auto actual = client_->DeleteNotification(
      DeleteNotificationRequest("bkt", "notification_id"));
  EXPECT_THAT(actual, StatusIs(StatusCode::kUnavailable));
}

INSTANTIATE_TEST_SUITE_P(HybridClientFailures, GrpcClientFailuresTest,
                         ::testing::Values("media"));

INSTANTIATE_TEST_SUITE_P(GrpcClientFailures, GrpcClientFailuresTest,
                         ::testing::Values("metadata"));

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/curl_client.h"
#include "google/cloud/internal/make_unique.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include <gmock/gmock.h>
#include <memory>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::google::cloud::storage::Status;
using ::google::cloud::storage::oauth2::Credentials;

long const STATUS_ERROR_CODE = 503;
std::string const STATUS_ERROR_MSG =
    "FailingCredentials doing its job, failing";

// We create a credential class that always fails to fetch an access token; this
// allows us to check that CurlClient methods fail early when their setup steps
// (which include adding the authorization header) return a failure Status.
// Note that if the methods performing the setup steps were not templated, we
// could simply mock those out instead via MOCK_METHOD<N> macros.
class FailingCredentials : public Credentials {
 public:
  std::pair<Status, std::string> AuthorizationHeader() override {
    return std::make_pair(Status(STATUS_ERROR_CODE, STATUS_ERROR_MSG), "");
  }
};

class CurlClientTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {
    client_ = CurlClient::Create(std::make_shared<FailingCredentials>());
  }

  static void TearDownTestCase() { client_.reset(); }

  static std::shared_ptr<CurlClient> client_;
};

std::shared_ptr<CurlClient> CurlClientTest::client_;

void TestCorrectFailureStatus(Status const& status) {
  EXPECT_EQ(status.status_code(), STATUS_ERROR_CODE);
  EXPECT_EQ(status.error_message(), STATUS_ERROR_MSG);
}

TEST_F(CurlClientTest, UploadChunk) {
  auto status_and_foo = client_->UploadChunk(
      UploadChunkRequest("http://unused.googleapis.com/invalid-session-id", 0U,
                         std::string{}, 0U));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, QueryResumableUpload) {
  auto status_and_foo =
      client_->QueryResumableUpload(QueryResumableUploadRequest(
          "http://unused.googleapis.com/invalid-session-id"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, ListBuckets) {
  auto status_and_foo = client_->ListBuckets(ListBucketsRequest{"project_id"});
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, CreateBucket) {
  auto status_and_foo = client_->CreateBucket(
      CreateBucketRequest("bkt", BucketMetadata().set_name("bkt")));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, GetBucketMetadata) {
  auto status_and_foo =
      client_->GetBucketMetadata(GetBucketMetadataRequest("bkt"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, DeleteBucket) {
  auto status_and_foo = client_->DeleteBucket(DeleteBucketRequest("bkt"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, UpdateBucket) {
  auto status_and_foo = client_->UpdateBucket(
      UpdateBucketRequest(BucketMetadata().set_name("bkt")));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, PatchBucket) {
  auto status_and_foo = client_->PatchBucket(
      PatchBucketRequest("bkt", BucketMetadata().set_name("bkt"),
                         BucketMetadata().set_name("bkt")));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, GetBucketIamPolicy) {
  auto status_and_foo =
      client_->GetBucketIamPolicy(GetBucketIamPolicyRequest("bkt"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, SetBucketIamPolicy) {
  auto status_and_foo = client_->SetBucketIamPolicy(
      SetBucketIamPolicyRequest("bkt", google::cloud::IamPolicy{}));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, TestBucketIamPermissions) {
  auto status_and_foo = client_->TestBucketIamPermissions(
      TestBucketIamPermissionsRequest("bkt", {}));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, LockBucketRetentionPolicy) {
  auto status_and_foo = client_->LockBucketRetentionPolicy(
      LockBucketRetentionPolicyRequest("bkt", 0U));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, InsertObjectMedia) {
  auto status_and_foo = client_->InsertObjectMedia(
      InsertObjectMediaRequest("bkt", "obj", "contents"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, GetObjectMetadata) {
  auto status_and_foo =
      client_->GetObjectMetadata(GetObjectMetadataRequest("bkt", "obj"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, ReadObject) {
  auto status_and_foo =
      client_->ReadObject(ReadObjectRangeRequest("bkt", "obj"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, WriteObject) {
  auto status_and_foo =
      client_->WriteObject(InsertObjectStreamingRequest("bkt", "obj"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, ListObjects) {
  auto status_and_foo = client_->ListObjects(ListObjectsRequest("bkt"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, DeleteObject) {
  auto status_and_foo =
      client_->DeleteObject(DeleteObjectRequest("bkt", "obj"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, UpdateObject) {
  auto status_and_foo = client_->UpdateObject(
      UpdateObjectRequest("bkt", "obj", ObjectMetadata()));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, PatchObject) {
  auto status_and_foo = client_->PatchObject(
      PatchObjectRequest("bkt", "obj", ObjectMetadata(), ObjectMetadata()));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, ComposeObject) {
  auto status_and_foo = client_->ComposeObject(
      ComposeObjectRequest("bkt", {}, "obj"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, ListBucketAcl) {
  auto status_and_foo = client_->ListBucketAcl(ListBucketAclRequest("bkt"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, CopyObject) {
  auto status_and_foo = client_->CopyObject(
      CopyObjectRequest("bkt", "obj1", "bkt", "obj2"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, CreateBucketAcl) {
  auto status_and_foo =
      client_->CreateBucketAcl(CreateBucketAclRequest("bkt", "entity", "role"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, GetBucketAcl) {
  auto status_and_foo =
      client_->GetBucketAcl(GetBucketAclRequest("bkt", "entity"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, DeleteBucketAcl) {
  auto status_and_foo =
      client_->DeleteBucketAcl(DeleteBucketAclRequest("bkt", "entity"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, UpdateBucketAcl) {
  auto status_and_foo =
      client_->UpdateBucketAcl(UpdateBucketAclRequest("bkt", "entity", "role"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, PatchBucketAcl) {
  auto status_and_foo = client_->PatchBucketAcl(PatchBucketAclRequest(
      "bkt", "entity", BucketAccessControl(), BucketAccessControl()));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, ListObjectAcl) {
  auto status_and_foo =
      client_->ListObjectAcl(ListObjectAclRequest("bkt", "obj"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, CreateObjectAcl) {
  auto status_and_foo = client_->CreateObjectAcl(
      CreateObjectAclRequest("bkt", "obj", "entity", "role"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, DeleteObjectAcl) {
  auto status_and_foo =
      client_->DeleteObjectAcl(DeleteObjectAclRequest("bkt", "obj", "entity"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, GetObjectAcl) {
  auto status_and_foo =
      client_->GetObjectAcl(GetObjectAclRequest("bkt", "obj", "entity"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, UpdateObjectAcl) {
  auto status_and_foo = client_->UpdateObjectAcl(
      UpdateObjectAclRequest("bkt", "obj", "entity", "role"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, PatchObjectAcl) {
  auto status_and_foo = client_->PatchObjectAcl(PatchObjectAclRequest(
      "bkt", "obj", "entity", ObjectAccessControl(), ObjectAccessControl()));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, RewriteObject) {
  auto status_and_foo = client_->RewriteObject(RewriteObjectRequest(
      "bkt", "obj", "bkt2", "obj2", "token"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, CreateResumableSession) {
  auto status_and_foo = client_->CreateResumableSession(
      ResumableUploadRequest("test-bucket", "test-object"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, ListDefaultObjectAcl) {
  auto status_and_foo =
      client_->ListDefaultObjectAcl(ListDefaultObjectAclRequest("bkt"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, CreateDefaultObjectAcl) {
  auto status_and_foo = client_->CreateDefaultObjectAcl(
      CreateDefaultObjectAclRequest("bkt", "entity", "role"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, DeleteDefaultObjectAcl) {
  auto status_and_foo = client_->DeleteDefaultObjectAcl(
      DeleteDefaultObjectAclRequest("bkt", "entity"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, GetDefaultObjectAcl) {
  auto status_and_foo =
      client_->GetDefaultObjectAcl(GetDefaultObjectAclRequest("bkt", "entity"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, UpdateDefaultObjectAcl) {
  auto status_and_foo = client_->UpdateDefaultObjectAcl(
      UpdateDefaultObjectAclRequest("bkt", "entity", "role"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, PatchDefaultObjectAcl) {
  auto status_and_foo =
      client_->PatchDefaultObjectAcl(PatchDefaultObjectAclRequest(
          "bkt", "entity", ObjectAccessControl(), ObjectAccessControl()));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, GetServiceAccount) {
  auto status_and_foo =
      client_->GetServiceAccount(GetProjectServiceAccountRequest("project_id"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, ListNotifications) {
  auto status_and_foo =
      client_->ListNotifications(ListNotificationsRequest("bkt"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, CreateNotification) {
  auto status_and_foo = client_->CreateNotification(
      CreateNotificationRequest("bkt", NotificationMetadata()));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, GetNotification) {
  auto status_and_foo = client_->GetNotification(
      GetNotificationRequest("bkt", "notification_id"));
  TestCorrectFailureStatus(status_and_foo.first);
}

TEST_F(CurlClientTest, DeleteNotification) {
  auto status_and_foo = client_->DeleteNotification(
      DeleteNotificationRequest("bkt", "notification_id"));
  TestCorrectFailureStatus(status_and_foo.first);
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

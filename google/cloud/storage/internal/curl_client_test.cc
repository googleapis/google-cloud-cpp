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
#include "google/cloud/internal/setenv.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/testing_util/environment_variable_restore.h"
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
using ::google::cloud::storage::oauth2::Credentials;
using ::testing::HasSubstr;

StatusCode const STATUS_ERROR_CODE = StatusCode::kUnavailable;
std::string const STATUS_ERROR_MSG =
    "FailingCredentials doing its job, failing";

// We create a credential class that always fails to fetch an access token; this
// allows us to check that CurlClient methods fail early when their setup steps
// (which include adding the authorization header) return a failure Status.
// Note that if the methods performing the setup steps were not templated, we
// could simply mock those out instead via MOCK_METHOD<N> macros.
class FailingCredentials : public Credentials {
 public:
  StatusOr<std::string> AuthorizationHeader() override {
    return Status(STATUS_ERROR_CODE, STATUS_ERROR_MSG);
  }
};

class CurlClientTest : public ::testing::Test,
                       public ::testing::WithParamInterface<std::string> {
 protected:
  CurlClientTest()
      : expected_status_code_(),
        endpoint_("CLOUD_STORAGE_TESTBENCH_ENDPOINT") {}
  ~CurlClientTest() override { endpoint_.TearDown(); }

  void SetUp() override {
    std::string const error_type = GetParam();
    if (error_type == "credentials-failure") {
      client_ = CurlClient::Create(std::make_shared<FailingCredentials>());
      expected_status_code_ = STATUS_ERROR_CODE;
      expected_status_substr_ = STATUS_ERROR_MSG;
    } else if (error_type == "libcurl-failure") {
      google::cloud::internal::SetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT",
                                      "http://localhost:0");
      client_ =
          CurlClient::Create(ClientOptions(oauth2::CreateAnonymousCredentials())
                                 .set_endpoint("http://localhost:0"));
      expected_status_code_ = StatusCode::kUnknown;
      expected_status_substr_ = "CURL error";
    } else {
      FAIL() << "Invalid test parameter value: " << error_type
             << ", expected either credentials-failure or libcurl-failure";
    }
  }

  void TearDown() override { client_.reset(); }

  void TestCorrectFailureStatus(Status const& status) {
    EXPECT_EQ(expected_status_code_, status.status_code());
    EXPECT_THAT(status.error_message(), HasSubstr(expected_status_substr_));
  }

  std::shared_ptr<CurlClient> client_;
  long expected_status_code_;
  std::string expected_status_substr_;
  testing_util::EnvironmentVariableRestore endpoint_;
};

TEST_P(CurlClientTest, UploadChunk) {
  // Use http://localhost:0 to force a libcurl failure
  auto status_or_foo = client_->UploadChunk(UploadChunkRequest(
      "http://localhost:0/invalid-session-id", 0U, std::string{}, 0U));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, QueryResumableUpload) {
  // Use http://localhost:0 to force a libcurl failure
  auto status_or_foo = client_->QueryResumableUpload(
      QueryResumableUploadRequest("http://localhost:0/invalid-session-id"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, ListBuckets) {
  auto status_or_foo = client_->ListBuckets(ListBucketsRequest{"project_id"});
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, CreateBucket) {
  auto status_or_foo = client_->CreateBucket(
      CreateBucketRequest("bkt", BucketMetadata().set_name("bkt")));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, GetBucketMetadata) {
  auto status_or_foo =
      client_->GetBucketMetadata(GetBucketMetadataRequest("bkt"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, DeleteBucket) {
  auto status_or_foo = client_->DeleteBucket(DeleteBucketRequest("bkt"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, UpdateBucket) {
  auto status_or_foo = client_->UpdateBucket(
      UpdateBucketRequest(BucketMetadata().set_name("bkt")));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, PatchBucket) {
  auto status_or_foo = client_->PatchBucket(
      PatchBucketRequest("bkt", BucketMetadata().set_name("bkt"),
                         BucketMetadata().set_name("bkt")));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, GetBucketIamPolicy) {
  auto status_or_foo =
      client_->GetBucketIamPolicy(GetBucketIamPolicyRequest("bkt"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, SetBucketIamPolicy) {
  auto status_or_foo = client_->SetBucketIamPolicy(
      SetBucketIamPolicyRequest("bkt", google::cloud::IamPolicy{}));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, TestBucketIamPermissions) {
  auto status_or_foo = client_->TestBucketIamPermissions(
      TestBucketIamPermissionsRequest("bkt", {}));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, LockBucketRetentionPolicy) {
  auto status_or_foo = client_->LockBucketRetentionPolicy(
      LockBucketRetentionPolicyRequest("bkt", 0U));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, InsertObjectMediaSimple) {
  auto status_or_foo = client_->InsertObjectMedia(
      InsertObjectMediaRequest("bkt", "obj", "contents")
          .set_multiple_options(DisableMD5Hash(true),
                                DisableCrc32cChecksum(true)));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, InsertObjectMediaMultipart) {
  std::string const error_type = GetParam();
  if (error_type != "credentials-failure") {
    // TODO(#1735) - enable this test when ObjectWriteStream uses StatusOr.
    return;
  }
  auto status_or_foo = client_->InsertObjectMedia(
      InsertObjectMediaRequest("bkt", "obj", "contents"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, InsertObjectMediaXml) {
  auto status_or_foo = client_->InsertObjectMedia(
      InsertObjectMediaRequest("bkt", "obj", "contents")
          .set_multiple_options(Fields("")));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, GetObjectMetadata) {
  auto status_or_foo =
      client_->GetObjectMetadata(GetObjectMetadataRequest("bkt", "obj"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, ReadObject) {
  std::string const error_type = GetParam();
  if (error_type != "credentials-failure") {
    // TODO(#1736) - enable this test when ObjectReadStream uses StatusOr.
    return;
  }
  auto status_or_foo =
      client_->ReadObject(ReadObjectRangeRequest("bkt", "obj"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, WriteObject) {
  std::string const error_type = GetParam();
  if (error_type != "credentials-failure") {
    // TODO(#1735) - enable this test when ObjectWriteStream uses StatusOr.
    return;
  }
  auto status_or_foo =
      client_->WriteObject(InsertObjectStreamingRequest("bkt", "obj"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, ListObjects) {
  auto status_or_foo = client_->ListObjects(ListObjectsRequest("bkt"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, DeleteObject) {
  auto status_or_foo = client_->DeleteObject(DeleteObjectRequest("bkt", "obj"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, UpdateObject) {
  auto status_or_foo = client_->UpdateObject(
      UpdateObjectRequest("bkt", "obj", ObjectMetadata()));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, PatchObject) {
  auto status_or_foo = client_->PatchObject(
      PatchObjectRequest("bkt", "obj", ObjectMetadata(), ObjectMetadata()));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, ComposeObject) {
  auto status_or_foo =
      client_->ComposeObject(ComposeObjectRequest("bkt", {}, "obj"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, ListBucketAcl) {
  auto status_or_foo = client_->ListBucketAcl(ListBucketAclRequest("bkt"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, CopyObject) {
  auto status_or_foo =
      client_->CopyObject(CopyObjectRequest("bkt", "obj1", "bkt", "obj2"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, CreateBucketAcl) {
  auto status_or_foo =
      client_->CreateBucketAcl(CreateBucketAclRequest("bkt", "entity", "role"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, GetBucketAcl) {
  auto status_or_foo =
      client_->GetBucketAcl(GetBucketAclRequest("bkt", "entity"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, DeleteBucketAcl) {
  auto status_or_foo =
      client_->DeleteBucketAcl(DeleteBucketAclRequest("bkt", "entity"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, UpdateBucketAcl) {
  auto status_or_foo =
      client_->UpdateBucketAcl(UpdateBucketAclRequest("bkt", "entity", "role"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, PatchBucketAcl) {
  auto status_or_foo = client_->PatchBucketAcl(PatchBucketAclRequest(
      "bkt", "entity", BucketAccessControl(), BucketAccessControl()));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, ListObjectAcl) {
  auto status_or_foo =
      client_->ListObjectAcl(ListObjectAclRequest("bkt", "obj"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, CreateObjectAcl) {
  auto status_or_foo = client_->CreateObjectAcl(
      CreateObjectAclRequest("bkt", "obj", "entity", "role"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, DeleteObjectAcl) {
  auto status_or_foo =
      client_->DeleteObjectAcl(DeleteObjectAclRequest("bkt", "obj", "entity"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, GetObjectAcl) {
  auto status_or_foo =
      client_->GetObjectAcl(GetObjectAclRequest("bkt", "obj", "entity"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, UpdateObjectAcl) {
  auto status_or_foo = client_->UpdateObjectAcl(
      UpdateObjectAclRequest("bkt", "obj", "entity", "role"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, PatchObjectAcl) {
  auto status_or_foo = client_->PatchObjectAcl(PatchObjectAclRequest(
      "bkt", "obj", "entity", ObjectAccessControl(), ObjectAccessControl()));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, RewriteObject) {
  auto status_or_foo = client_->RewriteObject(
      RewriteObjectRequest("bkt", "obj", "bkt2", "obj2", "token"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, CreateResumableSession) {
  auto status_or_foo = client_->CreateResumableSession(
      ResumableUploadRequest("test-bucket", "test-object"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, ListDefaultObjectAcl) {
  auto status_or_foo =
      client_->ListDefaultObjectAcl(ListDefaultObjectAclRequest("bkt"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, CreateDefaultObjectAcl) {
  auto status_or_foo = client_->CreateDefaultObjectAcl(
      CreateDefaultObjectAclRequest("bkt", "entity", "role"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, DeleteDefaultObjectAcl) {
  auto status_or_foo = client_->DeleteDefaultObjectAcl(
      DeleteDefaultObjectAclRequest("bkt", "entity"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, GetDefaultObjectAcl) {
  auto status_or_foo =
      client_->GetDefaultObjectAcl(GetDefaultObjectAclRequest("bkt", "entity"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, UpdateDefaultObjectAcl) {
  auto status_or_foo = client_->UpdateDefaultObjectAcl(
      UpdateDefaultObjectAclRequest("bkt", "entity", "role"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, PatchDefaultObjectAcl) {
  auto status_or_foo =
      client_->PatchDefaultObjectAcl(PatchDefaultObjectAclRequest(
          "bkt", "entity", ObjectAccessControl(), ObjectAccessControl()));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, GetServiceAccount) {
  auto status_or_foo =
      client_->GetServiceAccount(GetProjectServiceAccountRequest("project_id"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, ListNotifications) {
  auto status_or_foo =
      client_->ListNotifications(ListNotificationsRequest("bkt"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, CreateNotification) {
  auto status_or_foo = client_->CreateNotification(
      CreateNotificationRequest("bkt", NotificationMetadata()));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, GetNotification) {
  auto status_or_foo = client_->GetNotification(
      GetNotificationRequest("bkt", "notification_id"));
  TestCorrectFailureStatus(status_or_foo.status());
}

TEST_P(CurlClientTest, DeleteNotification) {
  auto status_or_foo = client_->DeleteNotification(
      DeleteNotificationRequest("bkt", "notification_id"));
  TestCorrectFailureStatus(status_or_foo.status());
}

INSTANTIATE_TEST_CASE_P(CredentialsFailure, CurlClientTest,
                        ::testing::Values("credentials-failure"));

INSTANTIATE_TEST_CASE_P(LibCurlFailure, CurlClientTest,
                        ::testing::Values("libcurl-failure"));

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

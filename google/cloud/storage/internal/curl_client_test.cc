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
#include "google/cloud/storage/iam_policy.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/internal/setenv.h"
#include "google/cloud/testing_util/scoped_environment.h"
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

StatusCode const kStatusErrorCode = StatusCode::kUnavailable;
std::string const kStatusErrorMsg = "FailingCredentials doing its job, failing";

// We create a credential class that always fails to fetch an access token; this
// allows us to check that CurlClient methods fail early when their setup steps
// (which include adding the authorization header) return a failure Status.
// Note that if the methods performing the setup steps were not templated, we
// could simply mock those out instead via MOCK_METHOD<N> macros.
class FailingCredentials : public Credentials {
 public:
  StatusOr<std::string> AuthorizationHeader() override {
    return Status(kStatusErrorCode, kStatusErrorMsg);
  }
};

class CurlClientTest : public ::testing::Test,
                       public ::testing::WithParamInterface<std::string> {
 protected:
  CurlClientTest() : endpoint_("CLOUD_STORAGE_TESTBENCH_ENDPOINT", {}) {}

  void SetUp() override {
    std::string const error_type = GetParam();
    if (error_type == "credentials-failure") {
      client_ = CurlClient::Create(std::make_shared<FailingCredentials>());
      // We know exactly what error to expect, so setup the assertions to be
      // very strict.
      check_status_ = [](Status const& actual) {
        EXPECT_EQ(kStatusErrorCode, actual.code());
        EXPECT_THAT(actual.message(), HasSubstr(kStatusErrorMsg));
      };
    } else if (error_type == "libcurl-failure") {
      google::cloud::internal::SetEnv("CLOUD_STORAGE_TESTBENCH_ENDPOINT",
                                      "http://localhost:1");
      client_ =
          CurlClient::Create(ClientOptions(oauth2::CreateAnonymousCredentials())
                                 .set_endpoint("http://localhost:1"));
      // We do not know what libcurl will return. Some kind of error, but varies
      // by version of libcurl. Just make sure it is an error and the CURL
      // details are included in the error message.
      check_status_ = [](Status const& actual) {
        EXPECT_FALSE(actual.ok());
        EXPECT_THAT(actual.message(), HasSubstr("CURL error"));
      };
    } else {
      FAIL() << "Invalid test parameter value: " << error_type
             << ", expected either credentials-failure or libcurl-failure";
    }
  }

  void CheckStatus(Status const& actual) { check_status_(actual); }

  void TearDown() override { client_.reset(); }

  std::shared_ptr<CurlClient> client_;
  std::function<void(Status const& status)> check_status_;
  testing_util::ScopedEnvironment endpoint_;
};

TEST_P(CurlClientTest, UploadChunk) {
  // Use an invalid port (0) to force a libcurl failure
  auto actual = client_
                    ->UploadChunk(UploadChunkRequest(
                        "http://localhost:1/invalid-session-id", 0,
                        {ConstBuffer{std::string{}}}, 0))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, QueryResumableUpload) {
  // Use http://localhost:1 to force a libcurl failure
  auto actual = client_
                    ->QueryResumableUpload(QueryResumableUploadRequest(
                        "http://localhost:9/invalid-session-id"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ListBuckets) {
  auto actual = client_->ListBuckets(ListBucketsRequest{"project_id"}).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, CreateBucket) {
  auto actual = client_
                    ->CreateBucket(CreateBucketRequest(
                        "bkt", BucketMetadata().set_name("bkt")))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetBucketMetadata) {
  auto actual =
      client_->GetBucketMetadata(GetBucketMetadataRequest("bkt")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteBucket) {
  auto actual = client_->DeleteBucket(DeleteBucketRequest("bkt")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, UpdateBucket) {
  auto actual =
      client_
          ->UpdateBucket(UpdateBucketRequest(BucketMetadata().set_name("bkt")))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, PatchBucket) {
  auto actual = client_
                    ->PatchBucket(PatchBucketRequest(
                        "bkt", BucketMetadata().set_name("bkt"),
                        BucketMetadata().set_name("bkt")))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetBucketIamPolicy) {
  auto actual =
      client_->GetBucketIamPolicy(GetBucketIamPolicyRequest("bkt")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetNativeBucketIamPolicy) {
  auto actual =
      client_->GetNativeBucketIamPolicy(GetBucketIamPolicyRequest("bkt"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, SetBucketIamPolicy) {
  auto actual = client_
                    ->SetBucketIamPolicy(SetBucketIamPolicyRequest(
                        "bkt", google::cloud::IamPolicy{}))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, SetNativeBucketIamPolicy) {
  auto actual =
      client_
          ->SetNativeBucketIamPolicy(SetNativeBucketIamPolicyRequest(
              "bkt", NativeIamPolicy(std::vector<NativeIamBinding>())))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, TestBucketIamPermissions) {
  auto actual =
      client_
          ->TestBucketIamPermissions(TestBucketIamPermissionsRequest("bkt", {}))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, LockBucketRetentionPolicy) {
  auto actual = client_
                    ->LockBucketRetentionPolicy(
                        LockBucketRetentionPolicyRequest("bkt", 0))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, InsertObjectMediaSimple) {
  auto actual = client_
                    ->InsertObjectMedia(
                        InsertObjectMediaRequest("bkt", "obj", "contents")
                            .set_multiple_options(DisableMD5Hash(true),
                                                  DisableCrc32cChecksum(true)))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, InsertObjectMediaMultipart) {
  auto actual = client_
                    ->InsertObjectMedia(
                        InsertObjectMediaRequest("bkt", "obj", "contents"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, InsertObjectMediaXml) {
  auto actual =
      client_
          ->InsertObjectMedia(InsertObjectMediaRequest("bkt", "obj", "contents")
                                  .set_multiple_options(Fields("")))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetObjectMetadata) {
  auto actual =
      client_->GetObjectMetadata(GetObjectMetadataRequest("bkt", "obj"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ReadObjectXml) {
  if (GetParam() == "libcurl-failure") {
    return;
  }
  auto actual =
      client_->ReadObject(ReadObjectRangeRequest("bkt", "obj")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ReadObjectJson) {
  if (GetParam() == "libcurl-failure") {
    return;
  }
  auto actual =
      client_
          ->ReadObject(ReadObjectRangeRequest("bkt", "obj")
                           .set_multiple_options(IfGenerationNotMatch(0)))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ListObjects) {
  auto actual = client_->ListObjects(ListObjectsRequest("bkt")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteObject) {
  auto actual =
      client_->DeleteObject(DeleteObjectRequest("bkt", "obj")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, UpdateObject) {
  auto actual =
      client_->UpdateObject(UpdateObjectRequest("bkt", "obj", ObjectMetadata()))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, PatchObject) {
  auto actual = client_
                    ->PatchObject(PatchObjectRequest(
                        "bkt", "obj", ObjectMetadata(), ObjectMetadata()))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ComposeObject) {
  auto actual =
      client_->ComposeObject(ComposeObjectRequest("bkt", {}, "obj")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ListBucketAcl) {
  auto actual = client_->ListBucketAcl(ListBucketAclRequest("bkt")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, CopyObject) {
  auto actual =
      client_->CopyObject(CopyObjectRequest("bkt", "obj1", "bkt", "obj2"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, CreateBucketAcl) {
  auto actual =
      client_->CreateBucketAcl(CreateBucketAclRequest("bkt", "entity", "role"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetBucketAcl) {
  auto actual =
      client_->GetBucketAcl(GetBucketAclRequest("bkt", "entity")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteBucketAcl) {
  auto actual =
      client_->DeleteBucketAcl(DeleteBucketAclRequest("bkt", "entity"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, UpdateBucketAcl) {
  auto actual =
      client_->UpdateBucketAcl(UpdateBucketAclRequest("bkt", "entity", "role"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, PatchBucketAcl) {
  auto actual =
      client_
          ->PatchBucketAcl(PatchBucketAclRequest(
              "bkt", "entity", BucketAccessControl(), BucketAccessControl()))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ListObjectAcl) {
  auto actual =
      client_->ListObjectAcl(ListObjectAclRequest("bkt", "obj")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, CreateObjectAcl) {
  auto actual = client_
                    ->CreateObjectAcl(
                        CreateObjectAclRequest("bkt", "obj", "entity", "role"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteObjectAcl) {
  auto actual =
      client_->DeleteObjectAcl(DeleteObjectAclRequest("bkt", "obj", "entity"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetObjectAcl) {
  auto actual =
      client_->GetObjectAcl(GetObjectAclRequest("bkt", "obj", "entity"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, UpdateObjectAcl) {
  auto actual = client_
                    ->UpdateObjectAcl(
                        UpdateObjectAclRequest("bkt", "obj", "entity", "role"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, PatchObjectAcl) {
  auto actual = client_
                    ->PatchObjectAcl(PatchObjectAclRequest(
                        "bkt", "obj", "entity", ObjectAccessControl(),
                        ObjectAccessControl()))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, RewriteObject) {
  auto actual = client_
                    ->RewriteObject(RewriteObjectRequest("bkt", "obj", "bkt2",
                                                         "obj2", "token"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, CreateResumableSession) {
  auto actual = client_
                    ->CreateResumableSession(
                        ResumableUploadRequest("test-bucket", "test-object"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteResumableUpload) {
  auto actual = client_
                    ->DeleteResumableUpload(
                        DeleteResumableUploadRequest("test-upload-session-url"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ListDefaultObjectAcl) {
  auto actual =
      client_->ListDefaultObjectAcl(ListDefaultObjectAclRequest("bkt"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, CreateDefaultObjectAcl) {
  auto actual = client_
                    ->CreateDefaultObjectAcl(
                        CreateDefaultObjectAclRequest("bkt", "entity", "role"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteDefaultObjectAcl) {
  auto actual = client_
                    ->DeleteDefaultObjectAcl(
                        DeleteDefaultObjectAclRequest("bkt", "entity"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetDefaultObjectAcl) {
  auto actual =
      client_->GetDefaultObjectAcl(GetDefaultObjectAclRequest("bkt", "entity"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, UpdateDefaultObjectAcl) {
  auto actual = client_
                    ->UpdateDefaultObjectAcl(
                        UpdateDefaultObjectAclRequest("bkt", "entity", "role"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, PatchDefaultObjectAcl) {
  auto actual =
      client_
          ->PatchDefaultObjectAcl(PatchDefaultObjectAclRequest(
              "bkt", "entity", ObjectAccessControl(), ObjectAccessControl()))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetServiceAccount) {
  auto actual =
      client_->GetServiceAccount(GetProjectServiceAccountRequest("project_id"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ListHmacKeyRequest) {
  auto status =
      client_->ListHmacKeys(ListHmacKeysRequest("project_id")).status();
  CheckStatus(status);
}

TEST_P(CurlClientTest, CreateHmacKeyRequest) {
  auto actual =
      client_
          ->CreateHmacKey(CreateHmacKeyRequest("project_id", "service-account"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, SignBlob) {
  auto actual =
      client_
          ->SignBlob(SignBlobRequest("test-service-account", "test-blob", {}))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ListNotifications) {
  auto actual =
      client_->ListNotifications(ListNotificationsRequest("bkt")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, CreateNotification) {
  auto actual = client_
                    ->CreateNotification(CreateNotificationRequest(
                        "bkt", NotificationMetadata()))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetNotification) {
  auto actual =
      client_->GetNotification(GetNotificationRequest("bkt", "notification_id"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteNotification) {
  auto actual = client_
                    ->DeleteNotification(
                        DeleteNotificationRequest("bkt", "notification_id"))
                    .status();
  CheckStatus(actual);
}

INSTANTIATE_TEST_SUITE_P(CredentialsFailure, CurlClientTest,
                         ::testing::Values("credentials-failure"));

INSTANTIATE_TEST_SUITE_P(LibCurlFailure, CurlClientTest,
                         ::testing::Values("libcurl-failure"));

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

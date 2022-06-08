// Copyright 2018 Google LLC
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

#include "google/cloud/storage/internal/curl_client.h"
#include "google/cloud/storage/iam_policy.h"
#include "google/cloud/storage/internal/curl_request_builder.h"
#include "google/cloud/storage/oauth2/credentials.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/setenv.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::OptionsSpan;
using ::google::cloud::storage::oauth2::Credentials;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;
using ::testing::Not;

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
  CurlClientTest() : endpoint_("CLOUD_STORAGE_EMULATOR_ENDPOINT", {}) {}

  void SetUp() override {
    std::string const error_type = GetParam();
    if (error_type == "credentials-failure") {
      client_ = CurlClient::Create(Options{}.set<Oauth2CredentialsOption>(
          std::make_shared<FailingCredentials>()));
      // We know exactly what error to expect, so setup the assertions to be
      // very strict.
      check_status_ = [](Status const& actual) {
        EXPECT_THAT(actual,
                    StatusIs(kStatusErrorCode, HasSubstr(kStatusErrorMsg)));
      };
    } else if (error_type == "libcurl-failure") {
      google::cloud::testing_util::SetEnv("CLOUD_STORAGE_EMULATOR_ENDPOINT",
                                          "http://localhost:1");
      client_ = CurlClient::Create(
          Options{}
              .set<Oauth2CredentialsOption>(
                  oauth2::CreateAnonymousCredentials())
              .set<RestEndpointOption>("http://localhost:1"));
      // We do not know what libcurl will return. Some kind of error, but varies
      // by version of libcurl. Just make sure it is an error and the CURL
      // details are included in the error message.
      check_status_ = [](Status const& actual) {
        EXPECT_THAT(actual,
                    StatusIs(Not(StatusCode::kOk), HasSubstr("CURL error")));
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

TEST(CurlClientStandaloneFunctions, HostHeader) {
  struct Test {
    std::string endpoint;
    std::string authority;
    std::string service;
    std::string expected;
  } cases[] = {
      {"https://storage.googleapis.com", "", "storage",
       "Host: storage.googleapis.com"},
      {"https://storage.googleapis.com", "auth", "storage", "Host: auth"},
      {"https://storage.googleapis.com:443", "", "storage",
       "Host: storage.googleapis.com"},
      {"https://restricted.googleapis.com", "", "storage",
       "Host: storage.googleapis.com"},
      {"https://private.googleapis.com", "", "storage",
       "Host: storage.googleapis.com"},
      {"https://restricted.googleapis.com", "", "iamcredentials",
       "Host: iamcredentials.googleapis.com"},
      {"https://private.googleapis.com", "", "iamcredentials",
       "Host: iamcredentials.googleapis.com"},
      {"http://localhost:8080", "", "", ""},
      {"http://localhost:8080", "auth", "", "Host: auth"},
      {"http://[::1]", "", "", ""},
      {"http://[::1]/", "", "", ""},
      {"http://[::1]/foo/bar", "", "", ""},
      {"http://[::1]:8080/", "", "", ""},
      {"http://[::1]:8080/foo/bar", "", "", ""},
      {"http://localhost:8080", "", "", ""},
      {"https://storage-download.127.0.0.1.nip.io/xmlapi/", "", "", ""},
      {"https://gcs.127.0.0.1.nip.io/storage/v1/", "", "", ""},
      {"https://gcs.127.0.0.1.nip.io:4443/upload/storage/v1/", "", "", ""},
      {"https://gcs.127.0.0.1.nip.io:4443/upload/storage/v1/", "", "", ""},
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Testing for " + test.endpoint + ", " + test.service);
    auto options = Options{}.set<RestEndpointOption>(test.endpoint);
    if (!test.authority.empty()) options.set<AuthorityOption>(test.authority);
    auto const actual = HostHeader(options, test.service.c_str());
    EXPECT_EQ(test.expected, actual);
  }
}

TEST_P(CurlClientTest, UploadChunk) {
  // Use http://localhost:1 to force a libcurl failure
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->UploadChunk(UploadChunkRequest(
                        "http://localhost:1/invalid-session-id", 0,
                        {ConstBuffer{std::string{}}}))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, QueryResumableUpload) {
  // Use http://localhost:1 to force a libcurl failure
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->QueryResumableUpload(QueryResumableUploadRequest(
                        "http://localhost:9/invalid-session-id"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ListBuckets) {
  OptionsSpan const span(client_->options());
  auto actual = client_->ListBuckets(ListBucketsRequest{"project_id"}).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, CreateBucket) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->CreateBucket(CreateBucketRequest(
                        "bkt", BucketMetadata().set_name("bkt")))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetBucketMetadata) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->GetBucketMetadata(GetBucketMetadataRequest("bkt")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteBucket) {
  OptionsSpan const span(client_->options());
  auto actual = client_->DeleteBucket(DeleteBucketRequest("bkt")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, UpdateBucket) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_
          ->UpdateBucket(UpdateBucketRequest(BucketMetadata().set_name("bkt")))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, PatchBucket) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->PatchBucket(PatchBucketRequest(
                        "bkt", BucketMetadata().set_name("bkt"),
                        BucketMetadata().set_name("bkt")))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetNativeBucketIamPolicy) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->GetNativeBucketIamPolicy(GetBucketIamPolicyRequest("bkt"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, SetNativeBucketIamPolicy) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_
          ->SetNativeBucketIamPolicy(SetNativeBucketIamPolicyRequest(
              "bkt", NativeIamPolicy(std::vector<NativeIamBinding>())))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, TestBucketIamPermissions) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_
          ->TestBucketIamPermissions(TestBucketIamPermissionsRequest("bkt", {}))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, LockBucketRetentionPolicy) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->LockBucketRetentionPolicy(
                        LockBucketRetentionPolicyRequest("bkt", 0))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, InsertObjectMediaSimple) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->InsertObjectMedia(
                        InsertObjectMediaRequest("bkt", "obj", "contents")
                            .set_multiple_options(DisableMD5Hash(true),
                                                  DisableCrc32cChecksum(true)))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, InsertObjectMediaMultipart) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->InsertObjectMedia(
                        InsertObjectMediaRequest("bkt", "obj", "contents"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, InsertObjectMediaXml) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_
          ->InsertObjectMedia(InsertObjectMediaRequest("bkt", "obj", "contents")
                                  .set_multiple_options(Fields("")))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetObjectMetadata) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->GetObjectMetadata(GetObjectMetadataRequest("bkt", "obj"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ReadObjectXml) {
  OptionsSpan const span(client_->options());
  if (GetParam() == "libcurl-failure") GTEST_SKIP();
  auto actual =
      client_->ReadObject(ReadObjectRangeRequest("bkt", "obj")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ReadObjectJson) {
  OptionsSpan const span(client_->options());
  if (GetParam() == "libcurl-failure") GTEST_SKIP();
  auto actual =
      client_
          ->ReadObject(ReadObjectRangeRequest("bkt", "obj")
                           .set_multiple_options(IfGenerationNotMatch(0)))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ListObjects) {
  OptionsSpan const span(client_->options());
  auto actual = client_->ListObjects(ListObjectsRequest("bkt")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteObject) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->DeleteObject(DeleteObjectRequest("bkt", "obj")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, UpdateObject) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->UpdateObject(UpdateObjectRequest("bkt", "obj", ObjectMetadata()))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, PatchObject) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->PatchObject(PatchObjectRequest(
                        "bkt", "obj", ObjectMetadata(), ObjectMetadata()))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ComposeObject) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->ComposeObject(ComposeObjectRequest("bkt", {}, "obj")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ListBucketAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_->ListBucketAcl(ListBucketAclRequest("bkt")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, CopyObject) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->CopyObject(CopyObjectRequest("bkt", "obj1", "bkt", "obj2"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, CreateBucketAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->CreateBucketAcl(CreateBucketAclRequest("bkt", "entity", "role"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetBucketAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->GetBucketAcl(GetBucketAclRequest("bkt", "entity")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteBucketAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->DeleteBucketAcl(DeleteBucketAclRequest("bkt", "entity"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, UpdateBucketAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->UpdateBucketAcl(UpdateBucketAclRequest("bkt", "entity", "role"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, PatchBucketAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_
          ->PatchBucketAcl(PatchBucketAclRequest(
              "bkt", "entity", BucketAccessControl(), BucketAccessControl()))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ListObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->ListObjectAcl(ListObjectAclRequest("bkt", "obj")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, CreateObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->CreateObjectAcl(
                        CreateObjectAclRequest("bkt", "obj", "entity", "role"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->DeleteObjectAcl(DeleteObjectAclRequest("bkt", "obj", "entity"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->GetObjectAcl(GetObjectAclRequest("bkt", "obj", "entity"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, UpdateObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->UpdateObjectAcl(
                        UpdateObjectAclRequest("bkt", "obj", "entity", "role"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, PatchObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->PatchObjectAcl(PatchObjectAclRequest(
                        "bkt", "obj", "entity", ObjectAccessControl(),
                        ObjectAccessControl()))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, RewriteObject) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->RewriteObject(RewriteObjectRequest("bkt", "obj", "bkt2",
                                                         "obj2", "token"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, CreateResumableUpload) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->CreateResumableUpload(
                        ResumableUploadRequest("test-bucket", "test-object"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteResumableUpload) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->DeleteResumableUpload(
                        DeleteResumableUploadRequest("test-upload-session-url"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ListDefaultObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->ListDefaultObjectAcl(ListDefaultObjectAclRequest("bkt"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, CreateDefaultObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->CreateDefaultObjectAcl(
                        CreateDefaultObjectAclRequest("bkt", "entity", "role"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteDefaultObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->DeleteDefaultObjectAcl(
                        DeleteDefaultObjectAclRequest("bkt", "entity"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetDefaultObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->GetDefaultObjectAcl(GetDefaultObjectAclRequest("bkt", "entity"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, UpdateDefaultObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->UpdateDefaultObjectAcl(
                        UpdateDefaultObjectAclRequest("bkt", "entity", "role"))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, PatchDefaultObjectAcl) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_
          ->PatchDefaultObjectAcl(PatchDefaultObjectAclRequest(
              "bkt", "entity", ObjectAccessControl(), ObjectAccessControl()))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetServiceAccount) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->GetServiceAccount(GetProjectServiceAccountRequest("project_id"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ListHmacKeyRequest) {
  OptionsSpan const span(client_->options());
  auto status =
      client_->ListHmacKeys(ListHmacKeysRequest("project_id")).status();
  CheckStatus(status);
}

TEST_P(CurlClientTest, CreateHmacKeyRequest) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_
          ->CreateHmacKey(CreateHmacKeyRequest("project_id", "service-account"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, SignBlob) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_
          ->SignBlob(SignBlobRequest("test-service-account", "test-blob", {}))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, ListNotifications) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->ListNotifications(ListNotificationsRequest("bkt")).status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, CreateNotification) {
  OptionsSpan const span(client_->options());
  auto actual = client_
                    ->CreateNotification(CreateNotificationRequest(
                        "bkt", NotificationMetadata()))
                    .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, GetNotification) {
  OptionsSpan const span(client_->options());
  auto actual =
      client_->GetNotification(GetNotificationRequest("bkt", "notification_id"))
          .status();
  CheckStatus(actual);
}

TEST_P(CurlClientTest, DeleteNotification) {
  OptionsSpan const span(client_->options());
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
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

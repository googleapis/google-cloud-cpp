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

#include "google/cloud/storage/internal/rest/client.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::rest_internal::RestContext;
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::An;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Matcher;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::Return;

TEST(RestClientTest, ResolveStorageAuthorityProdEndpoint) {
  auto options =
      Options{}.set<RestEndpointOption>("https://storage.googleapis.com");
  auto result_options = RestClient::ResolveStorageAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(),
              Eq("storage.googleapis.com"));
}

TEST(RestClientTest, ResolveStorageAuthorityEapEndpoint) {
  auto options =
      Options{}.set<RestEndpointOption>("https://eap.googleapis.com");
  auto result_options = RestClient::ResolveStorageAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(),
              Eq("storage.googleapis.com"));
}

TEST(RestClientTest, ResolveStorageAuthorityNonGoogleEndpoint) {
  auto options = Options{}.set<RestEndpointOption>("https://localhost");
  auto result_options = RestClient::ResolveStorageAuthority(options);
  EXPECT_FALSE(result_options.has<AuthorityOption>());
}

TEST(RestClientTest, ResolveStorageAuthorityOptionSpecified) {
  auto options = Options{}
                     .set<RestEndpointOption>("https://storage.googleapis.com")
                     .set<AuthorityOption>("auth_option_set");
  auto result_options = RestClient::ResolveStorageAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(), Eq("auth_option_set"));
}

TEST(RestClientTest, ResolveIamAuthorityProdEndpoint) {
  auto options =
      Options{}.set<IamEndpointOption>("https://iamcredentials.googleapis.com");
  auto result_options = RestClient::ResolveIamAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(),
              Eq("iamcredentials.googleapis.com"));
}

TEST(RestClientTest, ResolveIamAuthorityEapEndpoint) {
  auto options = Options{}.set<IamEndpointOption>("https://eap.googleapis.com");
  auto result_options = RestClient::ResolveIamAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(),
              Eq("iamcredentials.googleapis.com"));
}

TEST(RestClientTest, ResolveIamAuthorityNonGoogleEndpoint) {
  auto options = Options{}.set<IamEndpointOption>("https://localhost");
  auto result_options = RestClient::ResolveIamAuthority(options);
  EXPECT_FALSE(result_options.has<AuthorityOption>());
}

TEST(RestClientTest, ResolveIamAuthorityOptionSpecified) {
  auto options =
      Options{}
          .set<IamEndpointOption>("https://iamcredentials.googleapis.com")
          .set<AuthorityOption>("auth_option_set");
  auto result_options = RestClient::ResolveIamAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(), Eq("auth_option_set"));
}

Options TestOptions() {
  return Options{}
      .set<UserAgentProductsOption>({"p1/v1", "p2/v2"})
      .set<TargetApiVersionOption>("vTest");
}

RestContext TestContext() {
  return RestContext(TestOptions()).AddHeader("test-header", "test-value");
}

Matcher<RestContext&> ExpectedContext() {
  return AllOf(ResultOf(
                   "context includes UserAgentProductsOption",
                   [](RestContext& context) {
                     return context.options().get<UserAgentProductsOption>();
                   },
                   ElementsAre("p1/v1", "p2/v2")),
               ResultOf(
                   "context includes test-header",
                   [](RestContext& context) { return context.headers(); },
                   Contains(Pair("test-header", Contains("test-value")))));
}

Matcher<RestRequest const&> ExpectedRequest() {
  return ResultOf(
      "request includes TargetApiVersionOption",
      [](RestRequest const& r) { return r.path(); },
      HasSubstr("storage/vTest/"));
}

Matcher<std::vector<absl::Span<char const>> const&> ExpectedPayload() {
  return An<std::vector<absl::Span<char const>> const&>();
}

TEST(RestClientTest, ListBuckets) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->ListBuckets(context, TestOptions(), ListBucketsRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CreateBucket) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->CreateBucket(context, TestOptions(), CreateBucketRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetBucketMetadata) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->GetBucketMetadata(context, TestOptions(),
                                          GetBucketMetadataRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteBucket) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->DeleteBucket(context, TestOptions(), DeleteBucketRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, UpdateBucket) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->UpdateBucket(context, TestOptions(), UpdateBucketRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, PatchBucket) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->PatchBucket(context, TestOptions(), PatchBucketRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetNativeBucketIamPolicy) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->GetNativeBucketIamPolicy(context, TestOptions(),
                                                 GetBucketIamPolicyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, SetNativeBucketIamPolicy) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->SetNativeBucketIamPolicy(
      context, TestOptions(), SetNativeBucketIamPolicyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, TestBucketIamPermissions) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->TestBucketIamPermissions(
      context, TestOptions(), TestBucketIamPermissionsRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, LockBucketRetentionPolicy) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->LockBucketRetentionPolicy(
      context, TestOptions(), LockBucketRetentionPolicyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, InsertObjectMedia) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->InsertObjectMedia(context, TestOptions(),
                                          InsertObjectMediaRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetObjectMetadata) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->GetObjectMetadata(context, TestOptions(),
                                          GetObjectMetadataRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ReadObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->ReadObject(context, TestOptions(), ReadObjectRangeRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ListObjects) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->ListObjects(context, TestOptions(), ListObjectsRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->DeleteObject(context, TestOptions(), DeleteObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, UpdateObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->UpdateObject(context, TestOptions(), UpdateObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, PatchObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->PatchObject(context, TestOptions(), PatchObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ComposeObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->ComposeObject(context, TestOptions(), ComposeObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CreateResumableUpload) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->CreateResumableUpload(context, TestOptions(),
                                              ResumableUploadRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, QueryResumableUpload) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), _, _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->QueryResumableUpload(context, TestOptions(),
                                             QueryResumableUploadRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteResumableUpload) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->DeleteResumableUpload(context, TestOptions(),
                                              DeleteResumableUploadRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, UploadChunk) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), _, _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->UploadChunk(context, TestOptions(), UploadChunkRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ListBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->ListBucketAcl(context, TestOptions(), ListBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CopyObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->CopyObject(context, TestOptions(), CopyObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CreateBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->CreateBucketAcl(context, TestOptions(), CreateBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->GetBucketAcl(context, TestOptions(), GetBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->DeleteBucketAcl(context, TestOptions(), DeleteBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, UpdateBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->UpdateBucketAcl(context, TestOptions(), UpdateBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, PatchBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->PatchBucketAcl(context, TestOptions(), PatchBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ListObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->ListObjectAcl(context, TestOptions(), ListObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CreateObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->CreateObjectAcl(context, TestOptions(), CreateObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->DeleteObjectAcl(context, TestOptions(), DeleteObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->GetObjectAcl(context, TestOptions(), GetObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, UpdateObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->UpdateObjectAcl(context, TestOptions(), UpdateObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, PatchObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->PatchObjectAcl(context, TestOptions(), PatchObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, RewriteObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->RewriteObject(context, TestOptions(), RewriteObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ListDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->ListDefaultObjectAcl(context, TestOptions(),
                                             ListDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CreateDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->CreateDefaultObjectAcl(context, TestOptions(),
                                               CreateDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->DeleteDefaultObjectAcl(context, TestOptions(),
                                               DeleteDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->GetDefaultObjectAcl(context, TestOptions(),
                                            GetDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, UpdateDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->UpdateDefaultObjectAcl(context, TestOptions(),
                                               UpdateDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, PatchDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->PatchDefaultObjectAcl(context, TestOptions(),
                                              PatchDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetServiceAccount) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->GetServiceAccount(context, TestOptions(),
                                          GetProjectServiceAccountRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ListHmacKeys) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->ListHmacKeys(context, TestOptions(), ListHmacKeysRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CreateHmacKey) {
  auto mock = std::make_shared<MockRestClient>();
  auto expected_payload =
      An<std::vector<std::pair<std::string, std::string>> const&>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), expected_payload))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->CreateHmacKey(context, TestOptions(), CreateHmacKeyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteHmacKey) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->DeleteHmacKey(context, TestOptions(), DeleteHmacKeyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetHmacKey) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->GetHmacKey(context, TestOptions(), GetHmacKeyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, UpdateHmacKey) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->UpdateHmacKey(context, TestOptions(), UpdateHmacKeyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, SignBlob) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Post(ExpectedContext(), _, ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->SignBlob(context, TestOptions(), SignBlobRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ListNotifications) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->ListNotifications(context, TestOptions(),
                                          ListNotificationsRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CreateNotification) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->CreateNotification(context, TestOptions(),
                                           CreateNotificationRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetNotification) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->GetNotification(context, TestOptions(), GetNotificationRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteNotification) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestClient>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->DeleteNotification(context, TestOptions(),
                                           DeleteNotificationRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

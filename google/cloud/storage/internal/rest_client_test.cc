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

#include "google/cloud/storage/internal/rest_client.h"
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

using ::google::cloud::internal::OptionsSpan;
using ::google::cloud::rest_internal::RestContext;
using ::google::cloud::rest_internal::RestRequest;
using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::testing_util::MockRestClient;
using ::google::cloud::testing_util::StatusIs;
using ::testing::_;
using ::testing::An;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Matcher;
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

Matcher<RestContext&> ExpectedContext() {
  return ResultOf(
      "context includes UserAgentProductsOption",
      [](RestContext& context) {
        return context.options().get<UserAgentProductsOption>();
      },
      ElementsAre("p1/v1", "p2/v2"));
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
  auto tested = RestClient::Create(TestOptions(), mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->ListBuckets(ListBucketsRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CreateBucket) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->CreateBucket(CreateBucketRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetBucketMetadata) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->GetBucketMetadata(GetBucketMetadataRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteBucket) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->DeleteBucket(DeleteBucketRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, UpdateBucket) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->UpdateBucket(UpdateBucketRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, PatchBucket) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->PatchBucket(PatchBucketRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetNativeBucketIamPolicy) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->GetNativeBucketIamPolicy(GetBucketIamPolicyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, SetNativeBucketIamPolicy) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status =
      tested->SetNativeBucketIamPolicy(SetNativeBucketIamPolicyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, TestBucketIamPermissions) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status =
      tested->TestBucketIamPermissions(TestBucketIamPermissionsRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, LockBucketRetentionPolicy) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status =
      tested->LockBucketRetentionPolicy(LockBucketRetentionPolicyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, InsertObjectMedia) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->InsertObjectMedia(InsertObjectMediaRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetObjectMetadata) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->GetObjectMetadata(GetObjectMetadataRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ReadObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->ReadObject(ReadObjectRangeRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ListObjects) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->ListObjects(ListObjectsRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->DeleteObject(DeleteObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, UpdateObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->UpdateObject(UpdateObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, PatchObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->PatchObject(PatchObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ComposeObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->ComposeObject(ComposeObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CreateResumableUpload) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->CreateResumableUpload(ResumableUploadRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, QueryResumableUpload) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), _, _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->QueryResumableUpload(QueryResumableUploadRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteResumableUpload) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->DeleteResumableUpload(DeleteResumableUploadRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, UploadChunk) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), _, _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->UploadChunk(UploadChunkRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ListBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->ListBucketAcl(ListBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CopyObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->CopyObject(CopyObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CreateBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->CreateBucketAcl(CreateBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->GetBucketAcl(GetBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->DeleteBucketAcl(DeleteBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, UpdateBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->UpdateBucketAcl(UpdateBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, PatchBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->PatchBucketAcl(PatchBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ListObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->ListObjectAcl(ListObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CreateObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->CreateObjectAcl(CreateObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->DeleteObjectAcl(DeleteObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->GetObjectAcl(GetObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, UpdateObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->UpdateObjectAcl(UpdateObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, PatchObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->PatchObjectAcl(PatchObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, RewriteObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->RewriteObject(RewriteObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ListDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->ListDefaultObjectAcl(ListDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CreateDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->CreateDefaultObjectAcl(CreateDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->DeleteDefaultObjectAcl(DeleteDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->GetDefaultObjectAcl(GetDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, UpdateDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->UpdateDefaultObjectAcl(UpdateDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, PatchDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->PatchDefaultObjectAcl(PatchDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetServiceAccount) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->GetServiceAccount(GetProjectServiceAccountRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ListHmacKeys) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->ListHmacKeys(ListHmacKeysRequest());
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
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->CreateHmacKey(CreateHmacKeyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteHmacKey) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->DeleteHmacKey(DeleteHmacKeyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetHmacKey) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->GetHmacKey(GetHmacKeyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, UpdateHmacKey) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->UpdateHmacKey(UpdateHmacKeyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, SignBlob) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Post(ExpectedContext(), _, ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->SignBlob(SignBlobRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, ListNotifications) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->ListNotifications(ListNotificationsRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, CreateNotification) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->CreateNotification(CreateNotificationRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, GetNotification) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->GetNotification(GetNotificationRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestClientTest, DeleteNotification) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = RestClient::Create(Options{}, mock, mock);
  OptionsSpan span(TestOptions());
  auto status = tested->DeleteNotification(DeleteNotificationRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

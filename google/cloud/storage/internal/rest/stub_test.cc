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

#include "google/cloud/storage/internal/rest/stub.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/internal/api_client_header.h"
#include "google/cloud/testing_util/mock_rest_client.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::internal::HandCraftedLibClientHeader;
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
using ::testing::Not;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::Return;

TEST(RestStubTest, ResolveStorageAuthorityProdEndpoint) {
  auto options =
      Options{}.set<RestEndpointOption>("https://storage.googleapis.com");
  auto result_options = RestStub::ResolveStorageAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(),
              Eq("storage.googleapis.com"));
}

TEST(RestStubTest, ResolveStorageAuthorityEapEndpoint) {
  auto options =
      Options{}.set<RestEndpointOption>("https://eap.googleapis.com");
  auto result_options = RestStub::ResolveStorageAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(),
              Eq("storage.googleapis.com"));
}

TEST(RestStubTest, ResolveStorageAuthorityNonGoogleEndpoint) {
  auto options = Options{}.set<RestEndpointOption>("https://localhost");
  auto result_options = RestStub::ResolveStorageAuthority(options);
  EXPECT_FALSE(result_options.has<AuthorityOption>());
}

TEST(RestStubTest, ResolveStorageAuthorityOptionSpecified) {
  auto options = Options{}
                     .set<RestEndpointOption>("https://storage.googleapis.com")
                     .set<AuthorityOption>("auth_option_set");
  auto result_options = RestStub::ResolveStorageAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(), Eq("auth_option_set"));
}

TEST(RestStubTest, ResolveIamAuthorityProdEndpoint) {
  auto options =
      Options{}.set<IamEndpointOption>("https://iamcredentials.googleapis.com");
  auto result_options = RestStub::ResolveIamAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(),
              Eq("iamcredentials.googleapis.com"));
}

TEST(RestStubTest, ResolveIamAuthorityEapEndpoint) {
  auto options = Options{}.set<IamEndpointOption>("https://eap.googleapis.com");
  auto result_options = RestStub::ResolveIamAuthority(options);
  EXPECT_THAT(result_options.get<AuthorityOption>(),
              Eq("iamcredentials.googleapis.com"));
}

TEST(RestStubTest, ResolveIamAuthorityNonGoogleEndpoint) {
  auto options = Options{}.set<IamEndpointOption>("https://localhost");
  auto result_options = RestStub::ResolveIamAuthority(options);
  EXPECT_FALSE(result_options.has<AuthorityOption>());
}

TEST(RestStubTest, ResolveIamAuthorityOptionSpecified) {
  auto options =
      Options{}
          .set<IamEndpointOption>("https://iamcredentials.googleapis.com")
          .set<AuthorityOption>("auth_option_set");
  auto result_options = RestStub::ResolveIamAuthority(options);
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
  return AllOf(ResultOf(
                   "request includes TargetApiVersionOption",
                   [](RestRequest const& r) { return r.path(); },
                   HasSubstr("storage/vTest/")),
               ResultOf(
                   "request includes x-goog-api-client header",
                   [](RestRequest const& r) { return r.headers(); },
                   Contains(Pair("x-goog-api-client",
                                 ElementsAre(HandCraftedLibClientHeader())))));
}

Matcher<std::vector<absl::Span<char const>> const&> ExpectedPayload() {
  return An<std::vector<absl::Span<char const>> const&>();
}

TEST(RestStubTest, GlobalCustomHeadersAppearInRequestTest) {
  google::cloud::Options global_opts;
  global_opts.set<google::cloud::CustomHeadersOption>(
      {{"custom-header-1", "value1"}, {"custom-header-2", "value2"}});
  auto mock_client = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock_client, Get(_, _))
      .WillOnce([](google::cloud::rest_internal::RestContext&,
                   google::cloud::rest_internal::RestRequest const& request) {
        auto const& headers = request.headers();
        EXPECT_THAT(headers,
                    Contains(Pair("custom-header-1",
                                  std::vector<std::string>{"value1"})));
        EXPECT_THAT(headers,
                    Contains(Pair("custom-header-2",
                                  std::vector<std::string>{"value2"})));

        return PermanentError();
      });
  auto stub = std::make_unique<RestStub>(global_opts, mock_client, mock_client);
  ListObjectsRequest list_req("test_bucket");
  RestContext context(global_opts);
  auto result = stub->ListObjects(context, global_opts, list_req);
  EXPECT_FALSE(result.ok());
}

TEST(RestStubTest, ListBuckets) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->ListBuckets(context, TestOptions(), ListBucketsRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, CreateBucket) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->CreateBucket(context, TestOptions(), CreateBucketRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, ListBucketsIncludesPageTokenWhenPresentInRequest) {
  auto mock = std::make_shared<MockRestClient>();
  std::string expected_token = "test-page-token";
  ListBucketsRequest request("test-project-id");
  request.set_page_token(expected_token);

  EXPECT_CALL(*mock,
              Get(ExpectedContext(),
                  ResultOf(
                      "request parameters contain 'pageToken'",
                      [](RestRequest const& r) { return r.parameters(); },
                      Contains(Pair("pageToken", expected_token)))))
      .WillOnce(Return(PermanentError()));

  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  tested->ListBuckets(context, TestOptions(), request);
}

TEST(RestStubTest, ListBucketsOmitsPageTokenWhenEmptyInRequest) {
  auto mock = std::make_shared<MockRestClient>();
  ListBucketsRequest request("test-project-id");

  EXPECT_CALL(*mock,
              Get(ExpectedContext(),
                  ResultOf(
                      "request parameters do not contain 'pageToken'",
                      [](RestRequest const& r) { return r.parameters(); },
                      Not(Contains(Pair("pageToken", _))))))
      .WillOnce(Return(PermanentError()));

  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  tested->ListBuckets(context, TestOptions(), request);
}

TEST(RestStubTest, GetBucketMetadata) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->GetBucketMetadata(context, TestOptions(),
                                          GetBucketMetadataRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, DeleteBucket) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->DeleteBucket(context, TestOptions(), DeleteBucketRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, UpdateBucket) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->UpdateBucket(context, TestOptions(), UpdateBucketRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, PatchBucket) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->PatchBucket(context, TestOptions(), PatchBucketRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, GetNativeBucketIamPolicy) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->GetNativeBucketIamPolicy(context, TestOptions(),
                                                 GetBucketIamPolicyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, SetNativeBucketIamPolicy) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->SetNativeBucketIamPolicy(
      context, TestOptions(), SetNativeBucketIamPolicyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, TestBucketIamPermissions) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->TestBucketIamPermissions(
      context, TestOptions(), TestBucketIamPermissionsRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, LockBucketRetentionPolicy) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->LockBucketRetentionPolicy(
      context, TestOptions(), LockBucketRetentionPolicyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, InsertObjectMedia) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->InsertObjectMedia(context, TestOptions(),
                                          InsertObjectMediaRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, GetObjectMetadata) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->GetObjectMetadata(context, TestOptions(),
                                          GetObjectMetadataRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, ReadObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->ReadObject(context, TestOptions(), ReadObjectRangeRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, ReadObjectReadLastConflictsWithReadFromOffset) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest())).Times(0);
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->ReadObject(context, TestOptions(),
                                   ReadObjectRangeRequest()
                                       .set_option(ReadLast(5))
                                       .set_option(ReadFromOffset(7)));
  EXPECT_THAT(status, StatusIs(StatusCode::kInvalidArgument));
}

TEST(RestStubTest, ReadObjectReadLastConflictsWithReadRange) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest())).Times(0);
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->ReadObject(context, TestOptions(),
                                   ReadObjectRangeRequest()
                                       .set_option(ReadLast(5))
                                       .set_option(ReadRange(0, 7)));
  EXPECT_THAT(status, StatusIs(StatusCode::kInvalidArgument));
}

TEST(RestStubTest, ListObjects) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->ListObjects(context, TestOptions(), ListObjectsRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, DeleteObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->DeleteObject(context, TestOptions(), DeleteObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, UpdateObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->UpdateObject(context, TestOptions(), UpdateObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, MoveObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->MoveObject(context, TestOptions(), MoveObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, PatchObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->PatchObject(context, TestOptions(), PatchObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, ComposeObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->ComposeObject(context, TestOptions(), ComposeObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, CreateResumableUpload) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->CreateResumableUpload(context, TestOptions(),
                                              ResumableUploadRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, QueryResumableUpload) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), _, _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->QueryResumableUpload(context, TestOptions(),
                                             QueryResumableUploadRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, DeleteResumableUpload) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->DeleteResumableUpload(context, TestOptions(),
                                              DeleteResumableUploadRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, UploadChunk) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), _, _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->UploadChunk(context, TestOptions(), UploadChunkRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, ListBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->ListBucketAcl(context, TestOptions(), ListBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, CopyObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->CopyObject(context, TestOptions(), CopyObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, CreateBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->CreateBucketAcl(context, TestOptions(), CreateBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, GetBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->GetBucketAcl(context, TestOptions(), GetBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, DeleteBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->DeleteBucketAcl(context, TestOptions(), DeleteBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, UpdateBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->UpdateBucketAcl(context, TestOptions(), UpdateBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, PatchBucketAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->PatchBucketAcl(context, TestOptions(), PatchBucketAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, ListObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->ListObjectAcl(context, TestOptions(), ListObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, CreateObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->CreateObjectAcl(context, TestOptions(), CreateObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, DeleteObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->DeleteObjectAcl(context, TestOptions(), DeleteObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, GetObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->GetObjectAcl(context, TestOptions(), GetObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, UpdateObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->UpdateObjectAcl(context, TestOptions(), UpdateObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, PatchObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->PatchObjectAcl(context, TestOptions(), PatchObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, RewriteObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->RewriteObject(context, TestOptions(), RewriteObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, RestoreObject) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->RestoreObject(context, TestOptions(), RestoreObjectRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, ListDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->ListDefaultObjectAcl(context, TestOptions(),
                                             ListDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, CreateDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->CreateDefaultObjectAcl(context, TestOptions(),
                                               CreateDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, DeleteDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->DeleteDefaultObjectAcl(context, TestOptions(),
                                               DeleteDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, GetDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->GetDefaultObjectAcl(context, TestOptions(),
                                            GetDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, UpdateDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->UpdateDefaultObjectAcl(context, TestOptions(),
                                               UpdateDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, PatchDefaultObjectAcl) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Patch(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->PatchDefaultObjectAcl(context, TestOptions(),
                                              PatchDefaultObjectAclRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, GetServiceAccount) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->GetServiceAccount(context, TestOptions(),
                                          GetProjectServiceAccountRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, ListHmacKeys) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->ListHmacKeys(context, TestOptions(), ListHmacKeysRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, CreateHmacKey) {
  auto mock = std::make_shared<MockRestClient>();
  auto expected_payload =
      An<std::vector<std::pair<std::string, std::string>> const&>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), expected_payload))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->CreateHmacKey(context, TestOptions(), CreateHmacKeyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, DeleteHmacKey) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->DeleteHmacKey(context, TestOptions(), DeleteHmacKeyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, GetHmacKey) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->GetHmacKey(context, TestOptions(), GetHmacKeyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, UpdateHmacKey) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Put(ExpectedContext(), ExpectedRequest(), _))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->UpdateHmacKey(context, TestOptions(), UpdateHmacKeyRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, SignBlob) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Post(ExpectedContext(), _, ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->SignBlob(context, TestOptions(), SignBlobRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, ListNotifications) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->ListNotifications(context, TestOptions(),
                                          ListNotificationsRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, CreateNotification) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock,
              Post(ExpectedContext(), ExpectedRequest(), ExpectedPayload()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status = tested->CreateNotification(context, TestOptions(),
                                           CreateNotificationRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, GetNotification) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Get(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
  auto context = TestContext();
  auto status =
      tested->GetNotification(context, TestOptions(), GetNotificationRequest());
  EXPECT_THAT(status,
              StatusIs(PermanentError().code(), PermanentError().message()));
}

TEST(RestStubTest, DeleteNotification) {
  auto mock = std::make_shared<MockRestClient>();
  EXPECT_CALL(*mock, Delete(ExpectedContext(), ExpectedRequest()))
      .WillOnce(Return(PermanentError()));
  auto tested = std::make_unique<RestStub>(Options{}, mock, mock);
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

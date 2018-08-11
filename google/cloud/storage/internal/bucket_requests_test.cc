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

#include "google/cloud/storage/internal/bucket_requests.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {
using ::testing::HasSubstr;
using ::testing::Not;

TEST(GetBucketMetadataRequestTest, OStreamBasic) {
  GetBucketMetadataRequest request("my-bucket");
  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("my-bucket"));
}

TEST(GetBucketMetadataRequestTest, OStreamParameter) {
  GetBucketMetadataRequest request("my-bucket");
  request.set_multiple_options(IfMetaGenerationNotMatch(7),
                               UserProject("my-project"));
  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(os.str(), HasSubstr("userProject=my-project"));
}

TEST(ListBucketsRequestTest, Simple) {
  ListBucketsRequest request("my-project");
  EXPECT_EQ("my-project", request.project_id());
  request.set_option(Prefix("foo-"));
}

TEST(ListBucketsRequestTest, OStream) {
  ListBucketsRequest request("project-to-list");
  request.set_multiple_options(UserProject("project-to-bill"),
                               Prefix("foo-bar-baz"));
  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("ListBucketsRequest"));
  EXPECT_THAT(os.str(), HasSubstr("project_id=project-to-list"));
  EXPECT_THAT(os.str(), HasSubstr("userProject=project-to-bill"));
  EXPECT_THAT(os.str(), HasSubstr("prefix=foo-bar-baz"));
}

TEST(ListBucketsResponseTest, Parse) {
  std::string bucket1 = R"""({
      "kind": "storage#bucket",
      "id": "foo-bar-baz-1",
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar-baz-1",
      "projectNumber": "123456789",
      "name": "foo-bar-baz",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": "4",
      "location": "US",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""";
  std::string bucket2 = R"""({
      "kind": "storage#bucket",
      "id": "foo-bar-baz-2",
      "selfLink": "https://www.googleapis.com/storage/v1/b/foo-bar-baz-2",
      "projectNumber": "123456789",
      "name": "foo-bar-baz",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": "4",
      "location": "US",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""";
  std::string text = R"""({
      "kind": "storage#buckets",
      "nextPageToken": "some-token-42",
      "items":
)""";
  text += "[" + bucket1 + "," + bucket2 + "]}";

  auto b1 = BucketMetadata::ParseFromString(bucket1);
  auto b2 = BucketMetadata::ParseFromString(bucket2);

  auto actual =
      ListBucketsResponse::FromHttpResponse(HttpResponse{200, text, {}});
  EXPECT_EQ("some-token-42", actual.next_page_token);
  EXPECT_THAT(actual.items, ::testing::ElementsAre(b1, b2));
}

TEST(CreateBucketsRequestTest, Basic) {
  CreateBucketRequest request("project-for-new-bucket",
                              BucketMetadata().set_name("test-bucket"));
  request.set_multiple_options(UserProject("project-to-bill"),
                               PredefinedAcl("projectPrivate"));

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("CreateBucketRequest"));
  EXPECT_THAT(actual, HasSubstr("project_id=project-for-new-bucket"));
  EXPECT_THAT(actual, HasSubstr("userProject=project-to-bill"));
  EXPECT_THAT(actual, HasSubstr("predefinedAcl=projectPrivate"));
  EXPECT_THAT(actual, HasSubstr("name=test-bucket"));
}

TEST(DeleteBucketRequestTest, OStream) {
  DeleteBucketRequest request("my-bucket");
  request.set_multiple_options(IfMetaGenerationNotMatch(7),
                               UserProject("my-project"));
  EXPECT_EQ("my-bucket", request.bucket_name());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("my-bucket"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
}

BucketMetadata CreateBucketMetadataForTest() {
  return BucketMetadata::ParseFromString(R"""({
      "kind": "storage#bucket",
      "id": "test-bucket",
      "selfLink": "https://www.googleapis.com/storage/v1/b/test-bucket",
      "projectNumber": "123456789",
      "name": "test-bucket",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": "4",
      "location": "US",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""");
}

TEST(UpdateBucketRequestTest, Simple) {
  BucketMetadata metadata = CreateBucketMetadataForTest();
  metadata.set_name("my-bucket");
  UpdateBucketRequest request(metadata);
  request.set_multiple_options(IfMetaGenerationNotMatch(7),
                               UserProject("my-project"));
  EXPECT_EQ(metadata, request.metadata());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("my-bucket"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
}

TEST(PatchBucketRequestTest, DiffSetAcl) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_acl({});
  BucketMetadata updated = original;
  updated.set_acl({BucketAccessControl::ParseFromString(R"""({
    "entity": "user-test-user",
    "role": "OWNER"})""")});
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({
      "acl": [{"entity": "user-test-user", "role": "OWNER"}]
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetAcl) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_acl({BucketAccessControl::ParseFromString(R"""({
    "entity": "user-test-user",
    "role": "OWNER"})""")});
  BucketMetadata updated = original;
  updated.set_acl({});
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"acl": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetBilling) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_billing();
  BucketMetadata updated = original;
  updated.set_billing(BucketBilling(true));
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({
      "billing": {"requesterPays": true}
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetBilling) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_billing(BucketBilling(true));
  BucketMetadata updated = original;
  updated.reset_billing();
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"billing": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetCors) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_cors({});
  BucketMetadata updated = original;
  CorsEntry e1;
  e1.max_age_seconds = 86400;
  CorsEntry e2{{}, {"m1", "m2"}, {"o1"}, {"r1"}};
  updated.set_cors({e1, e2});
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({
      "cors": [{
          "maxAgeSeconds": 86400
       }, {
           "method": ["m1", "m2"],
           "origin": ["o1"],
           "responseHeader": ["r1"]
       }]
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetCors) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_cors({CorsEntry{{}, {"m1"}, {}, {}}});
  BucketMetadata updated = original;
  updated.set_cors({});
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"cors": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetDefaultAcl) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_default_acl({});
  BucketMetadata updated = original;
  updated.set_default_acl({ObjectAccessControl::ParseFromString(R"""({
    "entity": "user-test-user",
    "role": "OWNER"})""")});
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({
      "defaultObjectAcl": [{"entity": "user-test-user", "role": "OWNER"}]
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetDefaultAcl) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_default_acl({ObjectAccessControl::ParseFromString(R"""({
    "entity": "user-test-user",
    "role": "OWNER"})""")});
  BucketMetadata updated = original;
  updated.set_default_acl({});
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"defaultObjectAcl": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetEncryption) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_encryption();
  BucketMetadata updated = original;
  updated.set_encryption(BucketEncryption{"invalid-key-name-just-for-test"});
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({
      "encryption": {"defaultKmsKeyName": "invalid-key-name-just-for-test"}
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetEncryption) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_encryption(BucketEncryption{"invalid-key-name-just-for-test"});
  BucketMetadata updated = original;
  updated.reset_encryption();
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"encryption": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetLabels) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.mutable_labels() = {
      {"label1", "v1"},
      {"label2", "v2"},
  };
  BucketMetadata updated = original;
  updated.mutable_labels().erase("label2");
  updated.mutable_labels().insert({"label3", "v3"});
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({
      "labels": {"label2": null, "label3": "v3"}
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetLabels) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.mutable_labels() = {
      {"label1", "v1"},
      {"label2", "v2"},
  };
  BucketMetadata updated = original;
  updated.mutable_labels().clear();
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"labels": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetLifecycle) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_lifecycle();;
  BucketMetadata updated = original;
  updated.set_lifecycle(BucketLifecycle{{LifecycleRule(
      LifecycleRule::NumNewerVersions(5), LifecycleRule::Delete())}});
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({
      "lifecycle": {
          "rule": [
              {
                  "condition": {"numNewerVersions": 5},
                  "action": {"type": "Delete"}
              }
          ]
       }
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetLifecycle) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_lifecycle(BucketLifecycle{{LifecycleRule(
      LifecycleRule::NumNewerVersions(5), LifecycleRule::Delete())}});
  BucketMetadata updated = original;
  updated.reset_lifecycle();
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"lifecycle": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetLocation) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_location("");
  BucketMetadata updated = original;
  updated.set_location("EU");
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"location": "EU"})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetLocation) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_location("US");
  BucketMetadata updated = original;
  updated.set_location("");
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"location": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetLogging) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_logging();
  BucketMetadata updated = original;
  updated.set_logging(BucketLogging{"test-log-bucket", "test-log-prefix"});
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({
      "logging": {
          "logBucket": "test-log-bucket",
          "logPrefix": "test-log-prefix"
      }
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetLogging) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_logging(BucketLogging{"test-log-bucket", "test-log-prefix"});
  BucketMetadata updated = original;
  updated.reset_logging();
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"logging": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetName) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_name("");
  BucketMetadata updated = original;
  updated.set_name("new-bucket-name");
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"name": "new-bucket-name"})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetName) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_name("bucket-name");
  BucketMetadata updated = original;
  updated.set_name("");
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"name": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetStorageClass) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_storage_class(storage_class::Standard());
  BucketMetadata updated = original;
  updated.set_storage_class(storage_class::Coldline());
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"storageClass": "COLDLINE"})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetStorageClass) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_storage_class(storage_class::Standard());
  BucketMetadata updated = original;
  updated.set_storage_class("");
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"storageClass": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetVersioning) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_versioning();
  BucketMetadata updated = original;
  updated.disable_versioning();
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({
      "versioning": {"enabled": false}
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetVersioning) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.enable_versioning();
  BucketMetadata updated = original;
  updated.reset_versioning();
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"versioning": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetWebsite) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_website();
  BucketMetadata updated = original;
  updated.set_website(BucketWebsite{"idx.htm", "404.htm"});
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({
      "website": {
          "mainPageSuffix": "idx.htm",
          "notFoundPage": "404.htm"
      }
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetWebsite) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_website(BucketWebsite{"idx.htm", "404.htm"});
  BucketMetadata updated = original;
  updated.reset_website();
  PatchBucketRequest request("test-bucket", original, updated);

  nl::json patch = nl::json::parse(request.payload());
  nl::json expected = nl::json::parse(R"""({"website": null})""");
  EXPECT_EQ(expected, patch);
}


TEST(PatchBucketRequestTest, DiffOStream) {
  BucketMetadata original = CreateBucketMetadataForTest();
  BucketMetadata updated = original;
  updated.set_location("EU").set_storage_class("NEARLINE");
  PatchBucketRequest request("test-bucket", original, updated);
  request.set_multiple_options(IfMetaGenerationNotMatch(7),
                               UserProject("my-project"));
  EXPECT_EQ("test-bucket", request.bucket());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("test-bucket"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
  EXPECT_THAT(actual, HasSubstr("NEARLINE"));
  EXPECT_THAT(actual, HasSubstr("EU"));
  EXPECT_THAT(actual, Not(HasSubstr("defaultObjectAcl")));
}

TEST(PatchBucketRequestTest, Builder) {
  PatchBucketRequest request("test-bucket", BucketMetadataPatchBuilder()
                                                .SetStorageClass("NEARLINE")
                                                .ResetDefaultAcl());
  request.set_multiple_options(IfMetaGenerationNotMatch(7),
                               UserProject("my-project"));
  EXPECT_EQ("test-bucket", request.bucket());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("test-bucket"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
  EXPECT_THAT(actual, HasSubstr("NEARLINE"));
  EXPECT_THAT(actual, Not(HasSubstr("EU")));
  EXPECT_THAT(actual, HasSubstr("defaultObjectAcl"));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

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
#include "google/cloud/storage/iam_policy.h"
#include "google/cloud/storage/internal/bucket_acl_requests.h"
#include "google/cloud/storage/internal/object_acl_requests.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Not;

/// @test Verify that we parse JSON objects into LifecycleRule objects.
TEST(LifecycleRuleParserTest, ParseFailure) {
  auto actual = LifecycleRuleParser::FromString("{123");
  EXPECT_FALSE(actual.ok());
}

TEST(LifecycleRuleParserTest, ParseCreatedBeforeFailure) {
  auto actual = LifecycleRuleParser::FromString(R"js({
    "condition": {
      "createdBefore": "xxxx"
    }
  })js");
  EXPECT_FALSE(actual.ok());
}

TEST(LifecycleRuleParserTest, ParseFull) {
  auto actual = LifecycleRuleParser::FromString(R"js({
    "action": {
      "type": "SetStorageClass",
      "setStorageClass": "COLDLINE"
    }
    "condition": {
      "age": 30,
      "createdBefore": "2020-07-20",
      "isLive": "false",
      "matchesStorageClass": ["STANDARD", "NEARLINE"],
      "numNewerVersions": 3
    }
  })js");
  EXPECT_FALSE(actual.ok());
}

/// @test Verify that we parse JSON objects into BucketMetadata objects.
TEST(BucketMetadataParserTest, ParseFailure) {
  auto actual = internal::BucketMetadataParser::FromString("{123");
  EXPECT_FALSE(actual.ok());
}

/// @test Verify that we parse JSON objects into BucketMetadata objects.
TEST(BucketMetadataParserTest, ParseAclFailure) {
  auto actual = internal::BucketMetadataParser::FromString(
      R"""({"acl: ["invalid-item"]})""");
  EXPECT_FALSE(actual.ok());
}

/// @test Verify that we parse JSON objects into BucketMetadata objects.
TEST(BucketMetadataParserTest, ParseDefaultObjecAclFailure) {
  auto actual = internal::BucketMetadataParser::FromString(
      R"""({"defaultObjectAcl: ["invalid-item"]})""");
  EXPECT_FALSE(actual.ok());
}

/// @test Verify that we parse JSON objects into BucketMetadata objects.
TEST(BucketMetadataParserTest, ParseLifecycleFailure) {
  auto actual = internal::BucketMetadataParser::FromString(
      R"""({"lifecycle: {"rule": [ "invalid-item" ]}})""");
  EXPECT_FALSE(actual.ok());
}

TEST(GetBucketMetadataRequestTest, OStreamBasic) {
  GetBucketMetadataRequest request("my-bucket");
  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("my-bucket"));
}

TEST(GetBucketMetadataRequestTest, OStreamParameter) {
  GetBucketMetadataRequest request("my-bucket");
  request.set_multiple_options(IfMetagenerationNotMatch(7),
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
      "selfLink": "https://storage.googleapis.com/storage/v1/b/foo-bar-baz-1",
      "projectNumber": "123456789",
      "name": "foo-bar-baz",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": "4",
      "location": "US",
      "locationType": "regional",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""";
  std::string bucket2 = R"""({
      "kind": "storage#bucket",
      "id": "foo-bar-baz-2",
      "selfLink": "https://storage.googleapis.com/storage/v1/b/foo-bar-baz-2",
      "projectNumber": "123456789",
      "name": "foo-bar-baz",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": "4",
      "location": "US",
      "locationType": "regional",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""";
  std::string text = R"""({
      "kind": "storage#buckets",
      "nextPageToken": "some-token-42",
      "items":
)""";
  text += "[" + bucket1 + "," + bucket2 + "]}";

  auto b1 = internal::BucketMetadataParser::FromString(bucket1).value();
  auto b2 = internal::BucketMetadataParser::FromString(bucket2).value();

  auto actual = ListBucketsResponse::FromHttpResponse(text).value();
  EXPECT_EQ("some-token-42", actual.next_page_token);
  EXPECT_THAT(actual.items, ::testing::ElementsAre(b1, b2));
}

TEST(ListBucketsResponseTest, ParseFailure) {
  std::string text("{123");
  StatusOr<ListBucketsResponse> actual =
      ListBucketsResponse::FromHttpResponse(text);
  EXPECT_FALSE(actual.ok());
}

TEST(ListBucketsResponseTest, ParseFailureInItems) {
  std::string text = R"""({"items": [ "invalid-item" ]})""";

  auto actual = ListBucketsResponse::FromHttpResponse(text);
  EXPECT_FALSE(actual.ok());
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
  request.set_multiple_options(IfMetagenerationNotMatch(7),
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
  return internal::BucketMetadataParser::FromString(R"""({
      "kind": "storage#bucket",
      "id": "test-bucket",
      "selfLink": "https://storage.googleapis.com/storage/v1/b/test-bucket",
      "projectNumber": "123456789",
      "name": "test-bucket",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "metageneration": "4",
      "location": "US",
      "locationType": "regional",
      "storageClass": "STANDARD",
      "etag": "XYZ="
})""")
      .value();
}

TEST(UpdateBucketRequestTest, Simple) {
  BucketMetadata metadata = CreateBucketMetadataForTest();
  metadata.set_name("my-bucket");
  UpdateBucketRequest request(metadata);
  request.set_multiple_options(IfMetagenerationNotMatch(7),
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
  updated.set_acl({internal::BucketAccessControlParser::FromString(
                       R"""({"entity": "user-test-user", "role": "OWNER"})""")
                       .value()});
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(
      R"""({"acl": [{"entity": "user-test-user", "role": "OWNER"}]})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetAcl) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_acl({internal::BucketAccessControlParser::FromString(
                        R"""({"entity": "user-test-user", "role": "OWNER"})""")
                        .value()});
  BucketMetadata updated = original;
  updated.set_acl({});
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"acl": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetBilling) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_billing();
  BucketMetadata updated = original;
  updated.set_billing(BucketBilling(true));
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
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

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"billing": null})""");
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

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
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

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"cors": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetDefaultEventBasedHold) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_default_event_based_hold(false);
  BucketMetadata updated = original;
  updated.set_default_event_based_hold(true);
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
      "defaultEventBasedHold": true
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetDefaultAcl) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_default_acl({});
  BucketMetadata updated = original;
  updated.set_default_acl(
      {internal::ObjectAccessControlParser::FromString(
           R"""({"entity": "user-test-user", "role": "OWNER"})""")
           .value()});
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
      "defaultObjectAcl": [{"entity": "user-test-user", "role": "OWNER"}]
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetDefaultAcl) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_default_acl(
      {internal::ObjectAccessControlParser::FromString(
           R"""({"entity": "user-test-user", "role": "OWNER"})""")
           .value()});
  BucketMetadata updated = original;
  updated.set_default_acl({});
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"defaultObjectAcl": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetEncryption) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_encryption();
  BucketMetadata updated = original;
  updated.set_encryption(BucketEncryption{"invalid-key-name-just-for-test"});
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
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

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"encryption": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetIamConfigurationBPO) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_encryption();
  BucketMetadata updated = original;
  BucketIamConfiguration configuration;
  configuration.bucket_policy_only = BucketPolicyOnly{true, {}};
  updated.set_iam_configuration(std::move(configuration));
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
      "iamConfiguration": {
          "bucketPolicyOnly": {"enabled": true},
          "uniformBucketLevelAccess":{"enabled": true}
       }
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetIamConfigurationUBLA) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_encryption();
  BucketMetadata updated = original;
  BucketIamConfiguration configuration;
  configuration.uniform_bucket_level_access =
      UniformBucketLevelAccess{true, {}};
  updated.set_iam_configuration(std::move(configuration));
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
      "iamConfiguration": {
          "bucketPolicyOnly": {"enabled": true},
          "uniformBucketLevelAccess":{"enabled": true}
       }
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetIamConfigurationBPO) {
  BucketMetadata original = CreateBucketMetadataForTest();
  BucketIamConfiguration configuration;
  configuration.bucket_policy_only = BucketPolicyOnly{true, {}};
  original.set_iam_configuration(std::move(configuration));
  BucketMetadata updated = original;
  updated.reset_iam_configuration();
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"iamConfiguration": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetIamConfigurationUBLA) {
  BucketMetadata original = CreateBucketMetadataForTest();
  BucketIamConfiguration configuration;
  configuration.uniform_bucket_level_access =
      UniformBucketLevelAccess{true, {}};
  original.set_iam_configuration(std::move(configuration));
  BucketMetadata updated = original;
  updated.reset_iam_configuration();
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"iamConfiguration": null})""");
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

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
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

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"labels": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetLifecycle) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_lifecycle();
  BucketMetadata updated = original;
  updated.set_lifecycle(BucketLifecycle{{LifecycleRule(
      LifecycleRule::NumNewerVersions(5), LifecycleRule::Delete())}});
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
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

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"lifecycle": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetLogging) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_logging();
  BucketMetadata updated = original;
  updated.set_logging(BucketLogging{"test-log-bucket", "test-log-prefix"});
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
      "logging": {
          "logBucket": "test-log-bucket",
          "logObjectPrefix": "test-log-prefix"
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

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"logging": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetName) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_name("");
  BucketMetadata updated = original;
  updated.set_name("new-bucket-name");
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"name": "new-bucket-name"})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetName) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_name("bucket-name");
  BucketMetadata updated = original;
  updated.set_name("");
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"name": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetRetentionPolicy) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_retention_policy();
  BucketMetadata updated = original;
  updated.set_retention_policy(std::chrono::seconds(60));
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
      "retentionPolicy": {
          "retentionPeriod": 60
      }
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetRetentionPolicy) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_retention_policy(std::chrono::seconds(60));
  BucketMetadata updated = original;
  updated.reset_retention_policy();
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"retentionPolicy": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetStorageClass) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_storage_class(storage_class::Standard());
  BucketMetadata updated = original;
  updated.set_storage_class(storage_class::Coldline());
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"storageClass": "COLDLINE"})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffResetStorageClass) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.set_storage_class(storage_class::Standard());
  BucketMetadata updated = original;
  updated.set_storage_class("");
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"storageClass": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetVersioning) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_versioning();
  BucketMetadata updated = original;
  updated.disable_versioning();
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
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

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"versioning": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffSetWebsite) {
  BucketMetadata original = CreateBucketMetadataForTest();
  original.reset_website();
  BucketMetadata updated = original;
  updated.set_website(BucketWebsite{"idx.htm", "404.htm"});
  PatchBucketRequest request("test-bucket", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
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

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"website": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchBucketRequestTest, DiffOStream) {
  BucketMetadata original = CreateBucketMetadataForTest();
  BucketMetadata updated = original;
  updated.set_storage_class("NEARLINE");
  PatchBucketRequest request("test-bucket", original, updated);
  request.set_multiple_options(IfMetagenerationNotMatch(7),
                               UserProject("my-project"));
  EXPECT_EQ("test-bucket", request.bucket());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("test-bucket"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
  EXPECT_THAT(actual, HasSubstr("NEARLINE"));
  EXPECT_THAT(actual, Not(HasSubstr("defaultObjectAcl")));
}

TEST(PatchBucketRequestTest, Builder) {
  PatchBucketRequest request("test-bucket", BucketMetadataPatchBuilder()
                                                .SetStorageClass("NEARLINE")
                                                .ResetDefaultAcl());
  request.set_multiple_options(IfMetagenerationNotMatch(7),
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

TEST(BucketRequestsTest, GetIamPolicy) {
  GetBucketIamPolicyRequest request("my-bucket");
  request.set_multiple_options(UserProject("project-for-billing"));
  EXPECT_EQ("my-bucket", request.bucket_name());
  std::ostringstream os;
  os << request;
  auto actual = os.str();
  EXPECT_THAT(actual, HasSubstr("my-bucket"));
  EXPECT_THAT(actual, HasSubstr("project-for-billing"));
}

TEST(BucketRequestsTest, ParseIamPolicyFromString) {
  nlohmann::json expected_payload{
      {"kind", "storage#policy"},
      {"etag", "XYZ="},
      {"bindings",
       {
           // The order of these elements matters, SetBucketIamPolicyRequest
           // generates them sorted by role. If we ever change that, we will
           // need to change this test, and it will be a bit more difficult to
           // write it.
           nlohmann::json{{"role", "roles/storage.admin"},
                          {"members", std::vector<std::string>{"test-user-1"}}},
           nlohmann::json{{"role", "roles/storage.objectViewer"},
                          {"members", std::vector<std::string>{"test-user-2",
                                                               "test-user-3"}}},
       }},
  };

  IamBindings expected_bindings;
  expected_bindings.AddMember("roles/storage.admin", "test-user-1");
  expected_bindings.AddMember("roles/storage.objectViewer", "test-user-2");
  expected_bindings.AddMember("roles/storage.objectViewer", "test-user-3");
  google::cloud::IamPolicy expected{0, expected_bindings, "XYZ="};

  auto actual = ParseIamPolicyFromString(expected_payload.dump()).value();
  EXPECT_EQ(expected, actual);
}

TEST(ListBucketsResponseTest, ParseIamPolicyFromStringFailure) {
  std::string text("{123");
  StatusOr<google::cloud::IamPolicy> actual = ParseIamPolicyFromString(text);
  EXPECT_FALSE(actual.ok());
}

TEST(BucketRequestsTest, ParseIamPolicyFromStringMissingRole) {
  nlohmann::json expected_payload{
      {"kind", "storage#policy"},
      {"etag", "XYZ="},
      {"bindings",
       std::vector<nlohmann::json>{
           nlohmann::json{{"members", std::vector<std::string>{"test-user-1"}}},

       }},
  };
  auto res = ParseIamPolicyFromString(expected_payload.dump());
  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kInvalidArgument, res.status().code());
  EXPECT_THAT(res.status().message(),
              HasSubstr("expected 'role' and 'members'"));
}

TEST(BucketRequestsTest, ParseIamPolicyFromStringMissingMembers) {
  nlohmann::json expected_payload{
      {"kind", "storage#policy"},
      {"etag", "XYZ="},
      {"bindings", std::vector<nlohmann::json>{nlohmann::json{
                       {"role", "roles/storage.objectViewer"},
                   }}},
  };

  auto res = ParseIamPolicyFromString(expected_payload.dump());
  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kInvalidArgument, res.status().code());
  EXPECT_THAT(res.status().message(),
              HasSubstr("expected 'role' and 'members'"));
}

TEST(BucketRequestsTest, ParseIamPolicyFromStringInvalidMembers) {
  nlohmann::json expected_payload{
      {"kind", "storage#policy"},
      {"etag", "XYZ="},
      {"bindings", std::vector<nlohmann::json>{nlohmann::json{
                       {"role", "roles/storage.objectViewer"},
                       {"members", "invalid"},
                   }}},
  };

  auto res = ParseIamPolicyFromString(expected_payload.dump());
  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kInvalidArgument, res.status().code());
  EXPECT_THAT(res.status().message(),
              HasSubstr("expected array for 'members'"));
}

TEST(BucketRequestsTest, ParseIamPolicyFromStringInvalidBindings) {
  nlohmann::json expected_payload{
      {"kind", "storage#policy"},
      {"etag", "XYZ="},
      {"bindings", "invalid"},
  };

  auto res = ParseIamPolicyFromString(expected_payload.dump());
  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kInvalidArgument, res.status().code());
  EXPECT_THAT(res.status().message(),
              HasSubstr("expected array for 'bindings'"));
}

TEST(BucketRequestsTest, ParseIamPolicyFromStringInvalidBindingsEntries) {
  nlohmann::json expected_payload{
      {"kind", "storage#policy"},
      {"etag", "XYZ="},
      {"bindings", std::vector<nlohmann::json>{"not_an_object"}},
  };

  auto res = ParseIamPolicyFromString(expected_payload.dump());
  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kInvalidArgument, res.status().code());
  EXPECT_THAT(res.status().message(),
              HasSubstr("expected objects for 'bindings' entries"));
}

TEST(BucketRequestsTest, ParseIamPolicyFromStringInvalidExtras) {
  nlohmann::json expected_payload{
      {"kind", "storage#policy"},
      {"etag", "XYZ="},
      {"bindings",
       {
           nlohmann::json{{"role", "roles/storage.admin"},
                          {"members", std::vector<std::string>{"test-user-1"}},
                          {"condition", "some_condition"}},
       }},
  };

  auto res = ParseIamPolicyFromString(expected_payload.dump());
  ASSERT_FALSE(res);
  EXPECT_EQ(StatusCode::kInvalidArgument, res.status().code());
  EXPECT_THAT(res.status().message(),
              HasSubstr("unexpected member 'condition' in element #0"));
}

TEST(BucketRequestsTest, SetIamPolicy) {
  google::cloud::IamBindings bindings;
  bindings.AddMember("roles/storage.admin", "test-user-1");
  bindings.AddMember("roles/storage.objectViewer", "test-user-2");
  bindings.AddMember("roles/storage.objectViewer", "test-user-3");
  google::cloud::IamPolicy policy{1, bindings, "XYZ="};

  SetBucketIamPolicyRequest request("my-bucket", policy);
  request.set_multiple_options(UserProject("project-for-billing"));
  EXPECT_EQ("my-bucket", request.bucket_name());

  nlohmann::json expected_payload{
      {"kind", "storage#policy"},
      {"etag", "XYZ="},
      {"bindings",
       {
           // The order of these elements matters, SetBucketIamPolicyRequest
           // generates them sorted by role. If we ever change that, we will
           // need to change this test, and it will be a bit more difficult to
           // write it.
           nlohmann::json{{"role", "roles/storage.admin"},
                          {"members", std::vector<std::string>{"test-user-1"}}},
           nlohmann::json{{"role", "roles/storage.objectViewer"},
                          {"members", std::vector<std::string>{"test-user-2",
                                                               "test-user-3"}}},
       }},
  };
  auto actual_payload = nlohmann::json::parse(request.json_payload());
  EXPECT_EQ(expected_payload, actual_payload)
      << nlohmann::json::diff(expected_payload, actual_payload);
  std::ostringstream os;
  os << request;
  auto actual = os.str();
  EXPECT_THAT(actual, HasSubstr("my-bucket"));
  EXPECT_THAT(actual, HasSubstr("project-for-billing"));
}

TEST(BucketRequestsTest, SetNativeIamPolicy) {
  auto policy =
      NativeIamPolicy({NativeIamBinding("roles/storage.admin", {"test-user-1"}),
                       NativeIamBinding("roles/storage.objectViewer",
                                        {"test-user-2", "test-user-3"})},
                      "XYZ=", 1);

  SetNativeBucketIamPolicyRequest request("my-bucket", policy);
  request.set_multiple_options(UserProject("project-for-billing"));
  EXPECT_EQ("my-bucket", request.bucket_name());

  nlohmann::json expected_payload{
      {"kind", "storage#policy"},
      {"version", 1},
      {"etag", "XYZ="},
      {"bindings",
       {
           // The order of these elements matters, SetBucketIamPolicyRequest
           // generates them sorted by role. If we ever change that, we will
           // need to change this test, and it will be a bit more difficult to
           // write it.
           nlohmann::json{{"role", "roles/storage.admin"},
                          {"members", std::vector<std::string>{"test-user-1"}}},
           nlohmann::json{{"role", "roles/storage.objectViewer"},
                          {"members", std::vector<std::string>{"test-user-2",
                                                               "test-user-3"}}},
       }},
  };
  EXPECT_TRUE(request.HasOption<IfMatchEtag>());
  auto actual_payload = nlohmann::json::parse(request.json_payload());
  EXPECT_EQ(expected_payload, actual_payload)
      << nlohmann::json::diff(expected_payload, actual_payload);
  std::ostringstream os;
  os << request;
  auto actual = os.str();
  EXPECT_THAT(actual, HasSubstr("my-bucket"));
  EXPECT_THAT(actual, HasSubstr("project-for-billing"));
}

TEST(BucketRequestsTest, SetNativeIamPolicyNoEtag) {
  auto policy =
      NativeIamPolicy({NativeIamBinding("roles/storage.admin", {"test-user-1"}),
                       NativeIamBinding("roles/storage.objectViewer",
                                        {"test-user-2", "test-user-3"})});

  SetNativeBucketIamPolicyRequest request("my-bucket", policy);
  EXPECT_FALSE(request.HasOption<IfMatchEtag>());
}

TEST(BucketRequestsTest, TestIamPermissions) {
  std::vector<std::string> permissions{"storage.buckets.get",
                                       "storage.buckets.setIamPolicy",
                                       "storage.objects.update"};
  TestBucketIamPermissionsRequest request("my-bucket", permissions);
  request.set_multiple_options(UserProject("project-for-billing"));
  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ(permissions, request.permissions());
  std::ostringstream os;
  os << request;
  auto actual = os.str();
  EXPECT_THAT(actual, HasSubstr("my-bucket"));
  EXPECT_THAT(actual, HasSubstr("project-for-billing"));
  EXPECT_THAT(actual, HasSubstr("storage.buckets.get"));
  EXPECT_THAT(actual, HasSubstr("storage.buckets.setIamPolicy"));
  EXPECT_THAT(actual, HasSubstr("storage.objects.update"));
}

TEST(BucketRequestsTest, TestIamPermissionsResponse) {
  std::string text = R"""({
      "permissions": [
          "storage.buckets.get",
          "storage.buckets.setIamPolicy",
          "storage.objects.update"
      ]})""";

  auto actual =
      TestBucketIamPermissionsResponse::FromHttpResponse(text).value();
  EXPECT_THAT(actual.permissions,
              ElementsAre("storage.buckets.get", "storage.buckets.setIamPolicy",
                          "storage.objects.update"));

  std::ostringstream os;
  os << actual;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("storage.buckets.get"));
  EXPECT_THAT(str, HasSubstr("storage.buckets.setIamPolicy"));
  EXPECT_THAT(str, HasSubstr("storage.objects.update"));
}

TEST(ListBucketsResponseTest, TestIamPermissionsResponseParseFailure) {
  std::string text("{123");
  StatusOr<TestBucketIamPermissionsResponse> actual =
      TestBucketIamPermissionsResponse::FromHttpResponse(text);
  EXPECT_FALSE(actual.ok());
}

TEST(BucketRequestsTest, TestIamPermissionsResponseEmpty) {
  std::string text = R"""({})""";

  auto actual =
      TestBucketIamPermissionsResponse::FromHttpResponse(text).value();
  EXPECT_TRUE(actual.permissions.empty());
}

TEST(BucketRequestsTest, LockBucketRetentionPolicyRequest) {
  LockBucketRetentionPolicyRequest request("test-bucket", 12345U);
  request.set_multiple_options(UserProject("project-for-billing"));
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ(12345, request.metageneration());

  std::ostringstream os;
  os << request;
  auto actual = os.str();
  EXPECT_THAT(actual, HasSubstr("test-bucket"));
  EXPECT_THAT(actual, HasSubstr("project-for-billing"));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

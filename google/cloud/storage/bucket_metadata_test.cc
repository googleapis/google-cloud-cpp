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

#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/internal/bucket_access_control_parser.h"
#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/storage_class.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Not;

BucketMetadata CreateBucketMetadataForTest() {
  // This metadata object has some impossible combination of fields in it. The
  // goal is to fully test the parsing, not to simulate valid objects.
  std::string text = R"""({
      "acl": [{
        "kind": "storage#bucketAccessControl",
        "id": "acl-id-0",
        "selfLink": "https://storage.googleapis.com/storage/v1/b/test-bucket/acl/user-test-user",
        "bucket": "test-bucket",
        "entity": "user-test-user",
        "role": "OWNER",
        "email": "test-user@example.com",
        "entityId": "user-test-user-id-123",
        "domain": "example.com",
        "projectTeam": {
          "projectNumber": "4567",
          "team": "owners"
        },
        "etag": "AYX="
      }, {
        "kind": "storage#objectAccessControl",
        "id": "acl-id-1",
        "selfLink": "https://storage.googleapis.com/storage/v1/b/test-bucket/acl/user-test-user2",
        "bucket": "test-bucket",
        "entity": "user-test-user2",
        "role": "READER",
        "email": "test-user2@example.com",
        "entityId": "user-test-user2-id-123",
        "domain": "example.com",
        "projectTeam": {
          "projectNumber": "4567",
          "team": "viewers"
        },
        "etag": "AYX="
      }
      ],
      "autoclass": {
        "enabled": true,
        "toggleTime": "2022-10-07T01:02:03Z",
        "terminalStorageClass": "NEARLINE",
        "terminalStorageClassUpdateTime": "2022-10-07T01:02:34Z"
      },
      "billing": {
        "requesterPays": true
      },
      "cors": [{
        "maxAgeSeconds": 3600,
        "method": ["GET", "HEAD"],
        "origin": ["cross-origin-example.com"]
      }, {
        "method": ["GET", "HEAD"],
        "origin": ["another-example.com"],
        "responseHeader": ["Content-Type"]
      }],
      "defaultEventBasedHold": true,
      "defaultObjectAcl": [{
        "kind": "storage#objectAccessControl",
        "id": "default-acl-id-0",
        "bucket": "test-bucket",
        "entity": "user-test-user-3",
        "role": "OWNER",
        "email": "test-user-1@example.com",
        "entityId": "user-test-user-1-id-123",
        "domain": "example.com",
        "projectTeam": {
          "projectNumber": "123456789",
          "team": "owners"
        },
        "etag": "AYX="
      }],
      "encryption": {
        "defaultKmsKeyName": "projects/test-project-name/locations/us-central1/keyRings/test-keyring-name/cryptoKeys/test-key-name"
      },
      "etag": "XYZ=",
      "iamConfiguration": {
        "uniformBucketLevelAccess": {
          "enabled": true,
          "lockedTime": "2020-01-02T03:04:05Z"
        },
        "publicAccessPrevention": "inherited"
      },
      "id": "test-bucket",
      "kind": "storage#bucket",
      "labels": {
        "label-key-1": "label-value-1",
        "label-key-2": "label-value-2"
      },
      "lifecycle": {
        "rule": [{
          "condition": {
            "age": 30,
            "matchesStorageClass": [ "STANDARD" ]
          },
          "action": {
            "type": "SetStorageClass",
            "storageClass": "NEARLINE"
          }
        }, {
          "condition": {
            "createdBefore": "2016-01-01",
            "matchesPrefix": [ "archive/", "obsolete/"],
            "matchesSuffix": [ ".bak", ".delete-me"]
          },
          "action": {
            "type": "Delete"
          }
        }]
      },
      "location": "US",
      "locationType": "regional",
      "logging": {
        "logBucket": "test-log-bucket",
        "logObjectPrefix": "test-log-prefix"
      },
      "metageneration": "4",
      "name": "test-bucket",
      "owner": {
        "entity": "project-owners-123456789",
        "entityId": "test-owner-id-123"
      },
      "projectNumber": "123456789",
      "retentionPolicy": {
          "effectiveTime": "2018-10-01T12:34:56Z",
          "isLocked": false,
          "retentionPeriod": 86400
      },
      "rpo": "DEFAULT",
      "selfLink": "https://storage.googleapis.com/storage/v1/b/test-bucket",
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z",
      "versioning": {
        "enabled": true
      },
      "website": {
        "mainPageSuffix": "index.html",
        "notFoundPage": "404.html"
      },
      "customPlacementConfig": {
        "dataLocations": ["us-central1", "us-east1"]
      }
})""";
  return internal::BucketMetadataParser::FromString(text).value();
}

/// @test Verify that we parse JSON objects into BucketMetadata objects.
TEST(BucketMetadataTest, Parse) {
  auto actual = CreateBucketMetadataForTest();

  EXPECT_EQ(2, actual.acl().size());
  EXPECT_EQ("acl-id-0", actual.acl().at(0).id());
  EXPECT_EQ("acl-id-1", actual.acl().at(1).id());

  auto const expected_autoclass_toggle =
      google::cloud::internal::ParseRfc3339("2022-10-07T01:02:03Z");
  ASSERT_STATUS_OK(expected_autoclass_toggle);
  ASSERT_TRUE(actual.has_autoclass());
  auto const expected_tscu =
      google::cloud::internal::ParseRfc3339("2022-10-07T01:02:34Z");
  EXPECT_EQ(actual.autoclass(),
            BucketAutoclass(true, *expected_autoclass_toggle, "NEARLINE",
                            *expected_tscu));

  EXPECT_TRUE(actual.billing().requester_pays);
  EXPECT_EQ(2, actual.cors().size());
  auto expected_cors_0 =
      CorsEntry{3600, {"GET", "HEAD"}, {"cross-origin-example.com"}, {}};
  EXPECT_EQ(expected_cors_0, actual.cors().at(0));
  auto expected_cors_1 =
      CorsEntry{{}, {"GET", "HEAD"}, {"another-example.com"}, {"Content-Type"}};
  EXPECT_EQ(expected_cors_1, actual.cors().at(1));
  EXPECT_TRUE(actual.default_event_based_hold());
  EXPECT_EQ(1, actual.default_acl().size());
  EXPECT_EQ("user-test-user-3", actual.default_acl().at(0).entity());
  EXPECT_EQ(
      "projects/test-project-name/locations/us-central1/keyRings/"
      "test-keyring-name/cryptoKeys/test-key-name",
      actual.encryption().default_kms_key_name);
  EXPECT_EQ("XYZ=", actual.etag());
  ASSERT_TRUE(actual.has_iam_configuration());
  ASSERT_TRUE(
      actual.iam_configuration().uniform_bucket_level_access.has_value());
  EXPECT_TRUE(actual.iam_configuration().uniform_bucket_level_access->enabled);
  EXPECT_EQ(
      "2020-01-02T03:04:05Z",
      google::cloud::internal::FormatRfc3339(
          actual.iam_configuration().uniform_bucket_level_access->locked_time));
  EXPECT_EQ(actual.iam_configuration().public_access_prevention.value_or(""),
            "inherited");
  EXPECT_EQ("test-bucket", actual.id());
  EXPECT_EQ("storage#bucket", actual.kind());
  EXPECT_EQ(2, actual.labels().size());
  EXPECT_TRUE(actual.has_label("label-key-1"));
  EXPECT_EQ("label-value-1", actual.label("label-key-1"));
  EXPECT_FALSE(actual.has_label("not-a-label-key"));
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(actual.label("not-a-label-key"), std::exception);
#else
  // We accept any output here because the actual output depends on the
  // C++ library implementation.
  EXPECT_DEATH_IF_SUPPORTED(actual.label("not-a-label-key"), "");
#endif  // GOOGLE_CLOUD_CPP_EXCEPTIONS

  EXPECT_TRUE(actual.has_lifecycle());
  EXPECT_EQ(2, actual.lifecycle().rule.size());

  EXPECT_THAT(
      actual.lifecycle().rule,
      ElementsAre(
          LifecycleRule{LifecycleRule::ConditionConjunction(
                            LifecycleRule::MaxAge(30),
                            LifecycleRule::MatchesStorageClassStandard()),
                        LifecycleRule::SetStorageClassNearline()},
          LifecycleRule{
              LifecycleRule::ConditionConjunction(
                  LifecycleRule::CreatedBefore(absl::CivilDay(2016, 1, 1)),
                  LifecycleRule::MatchesPrefixes({"archive/", "obsolete/"}),
                  LifecycleRule::MatchesSuffixes({".bak", ".delete-me"})),
              LifecycleRule::Delete()}));

  EXPECT_EQ("US", actual.location());
  EXPECT_EQ("regional", actual.location_type());

  // logging
  EXPECT_EQ("test-log-bucket", actual.logging().log_bucket);
  EXPECT_EQ("test-log-prefix", actual.logging().log_object_prefix);

  EXPECT_EQ(4, actual.metageneration());
  EXPECT_EQ("test-bucket", actual.name());

  // owner
  EXPECT_EQ("project-owners-123456789", actual.owner().entity);
  EXPECT_EQ("test-owner-id-123", actual.owner().entity_id);

  EXPECT_EQ(123456789, actual.project_number());

  // retentionPolicy
  ASSERT_TRUE(actual.has_retention_policy());
  EXPECT_EQ("2018-10-01T12:34:56Z",
            google::cloud::internal::FormatRfc3339(
                actual.retention_policy().effective_time));
  EXPECT_EQ(std::chrono::seconds(86400),
            actual.retention_policy().retention_period);
  EXPECT_FALSE(actual.retention_policy().is_locked);

  // rpo
  EXPECT_EQ("DEFAULT", actual.rpo());

  EXPECT_EQ("https://storage.googleapis.com/storage/v1/b/test-bucket",
            actual.self_link());
  EXPECT_EQ(storage_class::Standard(), actual.storage_class());
  // Use `date -u +%s --date='2018-05-19T19:31:14Z'` to get the magic number:
  auto magic_timestamp = 1526758274L;
  using std::chrono::duration_cast;
  EXPECT_EQ(magic_timestamp, duration_cast<std::chrono::seconds>(
                                 actual.time_created().time_since_epoch())
                                 .count());
  EXPECT_EQ(magic_timestamp + 10, duration_cast<std::chrono::seconds>(
                                      actual.updated().time_since_epoch())
                                      .count());

  // website
  ASSERT_TRUE(actual.has_website());
  EXPECT_EQ("index.html", actual.website().main_page_suffix);
  EXPECT_EQ("404.html", actual.website().not_found_page);

  // custom placement config
  ASSERT_TRUE(actual.has_custom_placement_config());
  EXPECT_THAT(actual.custom_placement_config().data_locations,
              ElementsAre("us-central1", "us-east1"));
  ASSERT_TRUE(actual.custom_placement_config_as_optional().has_value());
  EXPECT_THAT(actual.custom_placement_config_as_optional()->data_locations,
              ElementsAre("us-central1", "us-east1"));
}

/// @test Verify that the IOStream operator works as expected.
TEST(BucketMetadataTest, IOStream) {
  auto meta = CreateBucketMetadataForTest();

  std::ostringstream os;
  os << meta;
  auto actual = os.str();
  EXPECT_THAT(actual, HasSubstr("BucketMetadata"));

  // acl()
  EXPECT_THAT(actual, HasSubstr("acl-id-0"));
  EXPECT_THAT(actual, HasSubstr("acl-id-1"));

  // autoclass()
  EXPECT_THAT(
      actual,
      HasSubstr("autoclass={enabled=true, toggle_time=2022-10-07T01:02:03Z,"
                " terminal_storage_class=NEARLINE,"
                " terminal_storage_class_update=2022-10-07T01:02:34Z}"));

  // billing()
  EXPECT_THAT(actual, HasSubstr("enabled=true"));

  // bucket()
  EXPECT_THAT(actual, HasSubstr("bucket=test-bucket"));

  // labels()
  EXPECT_THAT(actual, HasSubstr("labels.label-key-1=label-value-1"));
  EXPECT_THAT(actual, HasSubstr("labels.label-key-2=label-value-2"));

  // default_event_based_hold()
  EXPECT_THAT(actual, HasSubstr("default_event_based_hold=true"));

  // default_acl()
  EXPECT_THAT(actual, HasSubstr("user-test-user-3"));

  // encryption()
  EXPECT_THAT(actual,
              HasSubstr("projects/test-project-name/locations/us-central1/"
                        "keyRings/test-keyring-name/cryptoKeys/test-key-name"));

  // iam_policy()
  EXPECT_THAT(actual, HasSubstr("BucketIamConfiguration={"));
  EXPECT_THAT(actual, HasSubstr("locked_time=2020-01-02T03:04:05Z"));
  EXPECT_THAT(actual, HasSubstr("public_access_prevention=inherited"));

  // lifecycle()
  EXPECT_THAT(actual, HasSubstr("age=30"));

  // location_type()
  EXPECT_THAT(actual, HasSubstr("location_type=regional"));

  // logging()
  EXPECT_THAT(actual, HasSubstr("test-log-bucket"));
  EXPECT_THAT(actual, HasSubstr("test-log-prefix"));

  // name()
  EXPECT_THAT(actual, HasSubstr("name=test-bucket"));

  // project_team()
  EXPECT_THAT(actual, HasSubstr("project-owners-123456789"));
  EXPECT_THAT(actual, HasSubstr("test-owner-id-123"));

  // retention_policy()
  EXPECT_THAT(actual, HasSubstr("retention_policy.retention_period=86400"));
  EXPECT_THAT(
      actual,
      HasSubstr("retention_policy.effective_time=2018-10-01T12:34:56Z"));
  EXPECT_THAT(actual, HasSubstr("retention_policy.is_locked=false"));

  // rpo()
  EXPECT_THAT(actual, HasSubstr("rpo=DEFAULT"));

  // versioning()
  EXPECT_THAT(actual, HasSubstr("versioning.enabled=true"));

  // website()
  EXPECT_THAT(actual, HasSubstr("index.html"));
  EXPECT_THAT(actual, HasSubstr("404.html"));

  // custom_placement_config()
  EXPECT_THAT(
      actual,
      HasSubstr(
          "custom_placement_config.data_locations=[us-central1, us-east1]"));
}

/// @test Verify we can convert a BucketMetadata object to a JSON string.
TEST(BucketMetadataTest, ToJsonString) {
  auto tested = CreateBucketMetadataForTest();
  auto actual_string = internal::BucketMetadataToJsonString(tested);
  // Verify that the produced string can be parsed as a JSON object.
  auto actual = nlohmann::json::parse(actual_string);

  // acl()
  ASSERT_EQ(1U, actual.count("acl")) << actual;
  EXPECT_TRUE(actual["acl"].is_array()) << actual;
  EXPECT_EQ(2, actual["acl"].size()) << actual;
  EXPECT_EQ("user-test-user", actual["acl"][0].value("entity", ""));
  EXPECT_EQ("user-test-user2", actual["acl"][1].value("entity", ""));

  // autoclass()
  ASSERT_TRUE(actual.contains("autoclass"));
  auto const expected_autoclass = nlohmann::json{
      {"enabled", true},
      // "toggleTime" is OUTPUT_ONLY and thus not included in the
      // JSON string for create/update.
      {"terminalStorageClass", "NEARLINE"},
      // "terminateStorageClassUpdateTime" is OUTPUT_ONLY too.
  };
  EXPECT_EQ(actual["autoclass"], expected_autoclass);

  // billing()
  ASSERT_EQ(1U, actual.count("billing")) << actual;
  EXPECT_TRUE(actual["billing"].value("requesterPays", false));

  // cors()
  ASSERT_EQ(1U, actual.count("cors")) << actual;
  EXPECT_TRUE(actual["cors"].is_array()) << actual;
  EXPECT_EQ(2, actual["cors"].size()) << actual;
  EXPECT_EQ(3600, actual["cors"][0].value("maxAgeSeconds", 0));

  // default_event_based_hold()
  ASSERT_EQ(1U, actual.count("defaultEventBasedHold"));
  ASSERT_EQ(true, actual.value("defaultEventBasedHold", false));

  // default_acl()
  ASSERT_EQ(1U, actual.count("defaultObjectAcl")) << actual;
  EXPECT_TRUE(actual["defaultObjectAcl"].is_array()) << actual;
  EXPECT_EQ(1, actual["defaultObjectAcl"].size()) << actual;
  EXPECT_EQ("user-test-user-3",
            actual["defaultObjectAcl"][0].value("entity", ""));

  // encryption()
  ASSERT_EQ(1U, actual.count("encryption"));
  EXPECT_EQ(
      "projects/test-project-name/locations/us-central1/keyRings/"
      "test-keyring-name/cryptoKeys/test-key-name",
      actual["encryption"].value("defaultKmsKeyName", ""));

  // iam_configuration()
  ASSERT_EQ(1U, actual.count("iamConfiguration"));
  nlohmann::json expected_iam_configuration{
      {"uniformBucketLevelAccess", nlohmann::json{{"enabled", true}}},
      {"publicAccessPrevention", "inherited"}};
  EXPECT_EQ(expected_iam_configuration, actual["iamConfiguration"]);

  // labels()
  ASSERT_EQ(1U, actual.count("labels")) << actual;
  EXPECT_TRUE(actual["labels"].is_object()) << actual;
  EXPECT_EQ("label-value-1", actual["labels"].value("label-key-1", ""));
  EXPECT_EQ("label-value-2", actual["labels"].value("label-key-2", ""));

  // lifecycle()
  ASSERT_EQ(1U, actual.count("lifecycle")) << actual;
  EXPECT_TRUE(actual["lifecycle"].is_object()) << actual;
  EXPECT_EQ(1, actual["lifecycle"].count("rule")) << actual["lifecycle"];
  EXPECT_TRUE(actual["lifecycle"]["rule"].is_array()) << actual["lifecycle"];
  ASSERT_EQ(2U, actual["lifecycle"]["rule"].size());
  auto rule = actual["lifecycle"]["rule"][0];
  ASSERT_TRUE(rule.is_object()) << rule;
  EXPECT_EQ(
      nlohmann::json({{"age", 30}, {"matchesStorageClass", {"STANDARD"}}}),
      rule.value("condition", nlohmann::json{}));
  EXPECT_EQ(nlohmann::json({
                {"type", "SetStorageClass"},
                {"storageClass", "NEARLINE"},
            }),
            rule.value("action", nlohmann::json{}));

  rule = actual["lifecycle"]["rule"][1];
  EXPECT_EQ(
      nlohmann::json({
          {"createdBefore", "2016-01-01"},
          {"matchesPrefix", std::vector<std::string>{"archive/", "obsolete/"}},
          {"matchesSuffix", std::vector<std::string>{".bak", ".delete-me"}},
      }),
      rule.value("condition", nlohmann::json{}));
  EXPECT_EQ(nlohmann::json({{"type", "Delete"}}),
            rule.value("action", nlohmann::json{}));

  // location()
  ASSERT_EQ(1U, actual.count("location")) << actual;
  EXPECT_EQ("US", actual.value("location", ""));

  // location_type()
  ASSERT_EQ(1U, actual.count("locationType")) << actual;
  EXPECT_EQ("regional", actual.value("locationType", ""));

  // logging()
  ASSERT_EQ(1U, actual.count("logging")) << actual;
  ASSERT_TRUE(actual["logging"].is_object()) << actual;
  EXPECT_EQ("test-log-bucket", actual["logging"].value("logBucket", ""));
  EXPECT_EQ("test-log-prefix", actual["logging"].value("logObjectPrefix", ""));

  // name()
  EXPECT_EQ("test-bucket", actual.value("name", ""));

  // retention_policy()
  ASSERT_EQ(1U, actual.count("retentionPolicy"));
  auto expected_retention_policy = nlohmann::json{
      {"retentionPeriod", 86400},
  };
  EXPECT_EQ(expected_retention_policy, actual["retentionPolicy"]);

  // rpo()
  EXPECT_TRUE(actual.contains("rpo"));
  EXPECT_EQ("DEFAULT", actual.value("rpo", ""));

  // storage_class()
  ASSERT_EQ("STANDARD", actual.value("storageClass", ""));

  // versioning()
  ASSERT_EQ(1U, actual.count("versioning")) << actual;
  ASSERT_EQ(1U, actual["versioning"].is_object()) << actual;
  ASSERT_TRUE(actual["versioning"].value("enabled", false));

  // website()
  ASSERT_EQ(1U, actual.count("website")) << actual;
  ASSERT_TRUE(actual["website"].is_object()) << actual;
  EXPECT_EQ("index.html", actual["website"].value("mainPageSuffix", ""));
  EXPECT_EQ("404.html", actual["website"].value("notFoundPage", ""));

  // custom_placement_config()
  ASSERT_TRUE(actual.contains("customPlacementConfig")) << actual;
  auto expected_custom_placement_config = nlohmann::json{
      {"dataLocations", std::vector<std::string>{"us-central1", "us-east1"}},
  };
  EXPECT_EQ(actual["customPlacementConfig"], expected_custom_placement_config);
}

TEST(BucketMetadataTest, ToJsonLifecycleRoundtrip) {
  auto input = CreateBucketMetadataForTest();
  auto const actual_string = internal::BucketMetadataToJsonString(input);
  auto actual = internal::BucketMetadataParser::FromString(actual_string);
  ASSERT_STATUS_OK(actual);
  // ToJsonString() drops many fields (it is used in requests that should only
  // send mutable fields). We want test that complex fields are preserved.  The
  // other fields are tested elsewhere.
  EXPECT_EQ(actual->billing(), input.billing());
  EXPECT_EQ(actual->cors(), input.cors());
  EXPECT_EQ(actual->lifecycle(), input.lifecycle());
  EXPECT_EQ(actual->website(), input.website());
}

/// @test Verify we can delete label fields.
TEST(BucketMetadataTest, DeleteLabels) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  EXPECT_TRUE(copy.has_label("label-key-1"));
  copy.delete_label("label-key-1");
  EXPECT_FALSE(copy.has_label("label-key-1"));
  EXPECT_FALSE(copy.has_label("not-there"));
  copy.delete_label("not-there");
  EXPECT_FALSE(copy.has_label("not-there"));
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change metadata existing label fields.
TEST(BucketMetadataTest, ChangeLabels) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  EXPECT_TRUE(copy.has_label("label-key-1"));
  copy.upsert_label("label-key-1", "some-new-value");
  EXPECT_EQ("some-new-value", copy.label("label-key-1"));
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change insert new label fields.
TEST(BucketMetadataTest, InsertLabels) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  EXPECT_FALSE(copy.has_label("not-there"));
  copy.upsert_label("not-there", "now-it-is");
  EXPECT_EQ("now-it-is", copy.label("not-there"));
  EXPECT_NE(expected, copy);
}

/// @test Verify we can make changes to one Acl in BucketMetadata.
TEST(BucketMetadataTest, MutableAcl) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  EXPECT_EQ(expected, copy);
  copy.mutable_acl().at(0).set_role(BucketAccessControl::ROLE_READER());
  copy.mutable_acl().at(1).set_role(BucketAccessControl::ROLE_OWNER());
  EXPECT_EQ("READER", copy.acl().at(0).role());
  EXPECT_EQ("OWNER", copy.acl().at(1).role());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change the full acl in BucketMetadata.
TEST(BucketMetadataTest, SetAcl) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  auto acl = expected.acl();
  acl.at(0).set_role(BucketAccessControl::ROLE_READER());
  acl.at(1).set_role(BucketAccessControl::ROLE_OWNER());
  copy.set_acl(std::move(acl));
  EXPECT_NE(expected, copy);
  EXPECT_EQ("READER", copy.acl().at(0).role());
}

/// @test Verify we can change the autoclass configuration in BucketMetadata.
TEST(BucketMetadataTest, SetAutoclass) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  ASSERT_TRUE(copy.has_autoclass());
  ASSERT_TRUE(copy.autoclass().enabled);
  copy.set_autoclass(
      BucketAutoclass{false, std::chrono::system_clock::time_point()});
  EXPECT_NE(expected, copy);
  ASSERT_TRUE(copy.has_autoclass());
  ASSERT_FALSE(copy.autoclass().enabled);
}

/// @test Verify we can reset the autoclass configuration in BucketMetadata.
TEST(BucketMetadataTest, ResetAutoclass) {
  auto expected = CreateBucketMetadataForTest();
  EXPECT_TRUE(expected.has_autoclass());
  auto copy = expected;
  copy.reset_autoclass();
  EXPECT_FALSE(copy.has_autoclass());
  EXPECT_NE(expected, copy);
  std::ostringstream os;
  os << copy;
  EXPECT_THAT(os.str(), Not(HasSubstr("autoclass")));
}

/// @test Verify we can change the billing configuration in BucketMetadata.
TEST(BucketMetadataTest, SetBilling) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  auto billing = copy.billing();
  billing.requester_pays = !billing.requester_pays;
  copy.set_billing(billing);
  EXPECT_NE(expected, copy);
}

/// @test Verify we can reset the billing configuration in BucketMetadata.
TEST(BucketMetadataTest, ResetBilling) {
  auto expected = CreateBucketMetadataForTest();
  EXPECT_TRUE(expected.has_billing());
  auto copy = expected;
  copy.reset_billing();
  EXPECT_FALSE(copy.has_billing());
  EXPECT_NE(expected, copy);
  std::ostringstream os;
  os << copy;
  EXPECT_THAT(os.str(), Not(HasSubstr("billing")));
}

/// @test Verify we can make changes to one CORS entry in BucketMetadata.
TEST(BucketMetadataTest, MutableCors) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  EXPECT_EQ(expected, copy);
  copy.mutable_cors().at(0).max_age_seconds = 3 * 3600;
  EXPECT_NE(expected, copy);
  EXPECT_EQ(3600, *expected.cors().at(0).max_age_seconds);
  EXPECT_EQ(3 * 3600, *copy.cors().at(0).max_age_seconds);
}

/// @test Verify we can change the full CORS configuration in BucketMetadata.
TEST(BucketMetadataTest, SetCors) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  auto cors = copy.cors();
  cors.at(0).response_header.emplace_back("Content-Encoding");
  copy.set_cors(std::move(cors));
  EXPECT_NE(expected, copy);
  EXPECT_EQ("Content-Encoding", copy.cors().at(0).response_header.back());
}

/// @test Verify we can change the default event based hold in BucketMetadata.
TEST(BucketMetadataTest, SetDefaultEventBasedHold) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  EXPECT_TRUE(copy.default_event_based_hold());
  copy.set_default_event_based_hold(!copy.default_event_based_hold());
  EXPECT_NE(expected, copy);
  std::ostringstream os;
  os << copy;
  EXPECT_THAT(os.str(), HasSubstr("default_event_based_hold"));
}

/// @test Verify we can make changes to one DefaultObjectAcl in
/// BucketMetadata.
TEST(BucketMetadataTest, MutableDefaultObjectAcl) {
  auto expected = CreateBucketMetadataForTest();
  EXPECT_EQ("OWNER", expected.default_acl().at(0).role());
  auto copy = expected;
  EXPECT_EQ(expected, copy);
  copy.mutable_default_acl().at(0).set_role(BucketAccessControl::ROLE_READER());
  EXPECT_EQ("READER", copy.default_acl().at(0).role());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change the full DefaultObjectAcl in BucketMetadata.
TEST(BucketMetadataTest, SetDefaultObjectAcl) {
  auto expected = CreateBucketMetadataForTest();
  EXPECT_FALSE(expected.default_acl().empty());
  auto copy = expected;
  auto default_acl = expected.default_acl();
  auto access = default_acl.at(0);
  access.set_entity("allAuthenticatedUsers");
  access.set_role("READER");
  default_acl.push_back(access);
  copy.set_default_acl(std::move(default_acl));
  EXPECT_EQ(2, copy.default_acl().size());
  EXPECT_EQ("allAuthenticatedUsers", copy.default_acl().at(1).entity());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change the IAM Configuration in BucketMetadata.
TEST(BucketMetadataTest, SetIamConfigurationUBLA) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  BucketIamConfiguration new_configuration;
  new_configuration.uniform_bucket_level_access = UniformBucketLevelAccess{
      true,
      google::cloud::internal::ParseRfc3339("2019-02-03T04:05:06Z").value()};
  copy.set_iam_configuration(new_configuration);
  ASSERT_TRUE(copy.has_iam_configuration());
  EXPECT_EQ(new_configuration, copy.iam_configuration());
  EXPECT_NE(expected, copy)
      << "expected = " << expected.iam_configuration()
      << "\n  actual = " << copy.iam_configuration() << "\n";
}

/// @test Verify we can change the IAM Configuration in BucketMetadata.
TEST(BucketMetadataTest, SetIamConfigurationPAP) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  BucketIamConfiguration new_configuration;
  new_configuration.public_access_prevention = "enforced";
  copy.set_iam_configuration(new_configuration);
  ASSERT_TRUE(copy.has_iam_configuration());
  EXPECT_EQ(new_configuration, copy.iam_configuration());
  EXPECT_NE(expected, copy)
      << "expected = " << expected.iam_configuration()
      << "\n  actual = " << copy.iam_configuration() << "\n";
}

/// @test Verify we can reset the IAM Configuration in BucketMetadata.
TEST(BucketMetadataTest, ResetIamConfiguration) {
  auto expected = CreateBucketMetadataForTest();
  EXPECT_TRUE(expected.has_encryption());
  auto copy = expected;
  copy.reset_iam_configuration();
  EXPECT_FALSE(copy.has_iam_configuration());
  EXPECT_NE(expected, copy);
  std::ostringstream os;
  os << copy;
  EXPECT_THAT(os.str(), Not(HasSubstr("iam_configuration=")));
}

/// @test Verify we can change the default encryption in BucketMetadata.
TEST(BucketMetadataTest, SetEncryption) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  std::string fake_key_name =
      "projects/test-project-name/locations/us-central1/keyRings/"
      "test-keyring-name/cryptoKeys/another-test-key-name";
  copy.set_encryption(BucketEncryption{fake_key_name});
  EXPECT_EQ(fake_key_name, copy.encryption().default_kms_key_name);
  EXPECT_NE(expected, copy);
}

/// @test Verify we can reset the default encryption in BucketMetadata.
TEST(BucketMetadataTest, ResetEncryption) {
  auto expected = CreateBucketMetadataForTest();
  EXPECT_TRUE(expected.has_encryption());
  auto copy = expected;
  copy.reset_encryption();
  EXPECT_FALSE(copy.has_encryption());
  EXPECT_NE(expected, copy);
  std::ostringstream os;
  os << copy;
  EXPECT_THAT(os.str(), Not(HasSubstr("encryption.")));
}

/// @test Verify we can reset the Object Lifecycle in BucketMetadata.
TEST(BucketMetadataTest, ResetLifecycle) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  EXPECT_TRUE(copy.has_lifecycle());
  copy.reset_lifecycle();
  EXPECT_FALSE(copy.has_lifecycle());
  EXPECT_NE(expected, copy);
  std::ostringstream os;
  os << copy;
  EXPECT_THAT(os.str(), Not(HasSubstr("lifecycle.")));
}

/// @test Verify we can change the Object Lifecycle in BucketMetadata.
TEST(BucketMetadataTest, SetLifecycle) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  EXPECT_TRUE(copy.has_lifecycle());
  auto updated = copy.lifecycle();
  updated.rule.emplace_back(LifecycleRule::MaxAge(365),
                            LifecycleRule::Delete());
  copy.set_lifecycle(std::move(updated));
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change the Logging configuration in BucketMetadata.
TEST(BucketMetadataTest, SetLogging) {
  auto expected = CreateBucketMetadataForTest();
  BucketLogging new_logging{"another-test-bucket", "another-test-prefix"};
  auto copy = expected;
  copy.set_logging(new_logging);
  EXPECT_EQ(new_logging, copy.logging());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change the Logging configuration in BucketMetadata.
TEST(BucketMetadataTest, ResetLogging) {
  auto expected = CreateBucketMetadataForTest();
  EXPECT_TRUE(expected.has_logging());
  auto copy = expected;
  copy.reset_logging();
  EXPECT_FALSE(copy.has_logging());
  EXPECT_NE(expected, copy);
  std::ostringstream os;
  os << copy;
  EXPECT_THAT(os.str(), Not(HasSubstr("logging.")));
}

/// @test Verify we can change the retention policy in BucketMetadata.
TEST(BucketMetadataTest, SetRetentionPolicy) {
  auto expected = CreateBucketMetadataForTest();
  BucketRetentionPolicy new_retention_policy{
      std::chrono::seconds(3600),
      google::cloud::internal::ParseRfc3339("2019-11-01T00:00:00Z").value(),
      true,
  };
  auto copy = expected;
  copy.set_retention_policy(new_retention_policy);
  ASSERT_TRUE(copy.has_retention_policy());
  EXPECT_EQ(new_retention_policy, copy.retention_policy());
  EXPECT_TRUE(copy.retention_policy_as_optional().has_value());
  EXPECT_NE(expected, copy);
}

/// @test Verify we can change the retention policy in BucketMetadata.
TEST(BucketMetadataTest, ResetRetentionPolicy) {
  auto expected = CreateBucketMetadataForTest();
  EXPECT_TRUE(expected.has_retention_policy());
  auto copy = expected;
  copy.reset_retention_policy();
  EXPECT_FALSE(copy.has_retention_policy());
  EXPECT_NE(expected, copy);
  std::ostringstream os;
  os << copy;
  EXPECT_THAT(os.str(), Not(HasSubstr("retention_policy.")));
}

/// @test Verify we can change the retention policy in BucketMetadata.
TEST(BucketMetadataTest, SetRPO) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  copy.set_rpo("ASYNC_TURBO");
  EXPECT_NE(expected, copy);
  std::ostringstream os;
  os << copy;
  EXPECT_THAT(os.str(), HasSubstr("rpo=ASYNC_TURBO"));
}

/// @test Verify we can clear the versioning field in BucketMetadata.
TEST(BucketMetadataTest, ClearVersioning) {
  auto expected = CreateBucketMetadataForTest();
  EXPECT_TRUE(expected.versioning().has_value());
  auto copy = expected;
  copy.reset_versioning();
  EXPECT_FALSE(copy.versioning().has_value());
  EXPECT_NE(copy, expected);
  std::ostringstream os;
  os << copy;
  EXPECT_THAT(os.str(), Not(HasSubstr("versioning.")));
}

/// @test Verify we can set the versioning field in BucketMetadata.
TEST(BucketMetadataTest, DisableVersioning) {
  auto expected = CreateBucketMetadataForTest();
  EXPECT_TRUE(expected.versioning().has_value());
  EXPECT_TRUE(expected.versioning()->enabled);
  auto copy = expected;
  copy.disable_versioning();
  EXPECT_TRUE(copy.versioning().has_value());
  EXPECT_FALSE(copy.versioning()->enabled);
  EXPECT_NE(copy, expected);
}

/// @test Verify we can set the versioning field in BucketMetadata.
TEST(BucketMetadataTest, EnableVersioning) {
  auto expected = CreateBucketMetadataForTest();
  EXPECT_TRUE(expected.versioning().has_value());
  EXPECT_TRUE(expected.versioning()->enabled);
  auto copy = expected;
  copy.reset_versioning();
  copy.enable_versioning();
  EXPECT_TRUE(copy.versioning().has_value());
  EXPECT_TRUE(copy.versioning()->enabled);
  EXPECT_EQ(copy, expected);
}

/// @test Verify we can set the versioning field in BucketMetadata.
TEST(BucketMetadataTest, SetVersioning) {
  auto expected = CreateBucketMetadataForTest();
  EXPECT_TRUE(expected.versioning().has_value());
  EXPECT_TRUE(expected.versioning()->enabled);
  auto copy = expected;
  copy.set_versioning(BucketVersioning{false});
  EXPECT_TRUE(copy.versioning().has_value());
  EXPECT_FALSE(copy.versioning()->enabled);
  EXPECT_NE(copy, expected);
}

/// @test Verify we can set the website field in BucketMetadata.
TEST(BucketMetadataTest, SetWebsite) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  copy.set_website(BucketWebsite{"main.html", "not-found.html"});
  EXPECT_EQ("main.html", copy.website().main_page_suffix);
  EXPECT_EQ("not-found.html", copy.website().not_found_page);
  EXPECT_NE(copy, expected);
}

/// @test Verify we can set the website field in BucketMetadata.
TEST(BucketMetadataTest, ResetWebsite) {
  auto expected = CreateBucketMetadataForTest();
  EXPECT_TRUE(expected.has_website());
  auto copy = expected;
  copy.reset_website();
  EXPECT_FALSE(copy.has_website());
  EXPECT_NE(copy, expected);
  std::ostringstream os;
  os << copy;
  EXPECT_THAT(os.str(), Not(HasSubstr("website.")));
}

/// @test Verify we can set the custom_placement_config field in BucketMetadata.
TEST(BucketMetadataTest, SetCustomPlacementConfig) {
  auto expected = CreateBucketMetadataForTest();
  auto copy = expected;
  copy.set_custom_placement_config(
      BucketCustomPlacementConfig{{"test-location-1", "test-location-2"}});
  ASSERT_TRUE(copy.has_custom_placement_config());
  EXPECT_THAT(copy.custom_placement_config().data_locations,
              ElementsAre("test-location-1", "test-location-2"));
  ASSERT_TRUE(copy.custom_placement_config_as_optional().has_value());
  EXPECT_THAT(copy.custom_placement_config_as_optional()->data_locations,
              ElementsAre("test-location-1", "test-location-2"));
  EXPECT_NE(copy, expected);
  std::ostringstream os;
  os << copy;
  EXPECT_THAT(os.str(), HasSubstr("custom_placement_config"));
}

/// @test Verify we can set the custom_placement_config field in BucketMetadata.
TEST(BucketMetadataTest, ResetCustomPlacementConfig) {
  auto expected = CreateBucketMetadataForTest();
  EXPECT_TRUE(expected.has_custom_placement_config());
  EXPECT_TRUE(expected.custom_placement_config_as_optional().has_value());
  auto copy = expected;
  copy.reset_custom_placement_config();
  EXPECT_FALSE(copy.has_custom_placement_config());
  EXPECT_FALSE(copy.custom_placement_config_as_optional().has_value());
  EXPECT_NE(copy, expected);
  std::ostringstream os;
  os << copy;
  EXPECT_THAT(os.str(), Not(HasSubstr("custom_placement_config")));
}

TEST(BucketMetadataPatchBuilder, SetAcl) {
  BucketMetadataPatchBuilder builder;
  builder.SetAcl({internal::BucketAccessControlParser::FromString(
                      R"""({"entity": "user-test-user", "role": "OWNER"})""")
                      .value()});

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("acl"));
  ASSERT_TRUE(json["acl"].is_array()) << json;
  ASSERT_EQ(1U, json["acl"].size()) << json;
  EXPECT_EQ("user-test-user", json["acl"][0].value("entity", "")) << json;
  EXPECT_EQ("OWNER", json["acl"][0].value("role", "")) << json;
}

TEST(BucketMetadataPatchBuilder, ResetAcl) {
  BucketMetadataPatchBuilder builder;
  builder.ResetAcl();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("acl")) << json;
  ASSERT_TRUE(json["acl"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetAutoclass) {
  BucketMetadataPatchBuilder builder;
  builder.SetAutoclass(BucketAutoclass(true, "ARCHIVE"));

  auto actual = builder.BuildPatch();
  auto const json = nlohmann::json::parse(actual);
  ASSERT_TRUE(json.contains("autoclass")) << json;
  auto const expected_autoclass =
      nlohmann::json{{"enabled", true}, {"terminalStorageClass", "ARCHIVE"}};
  EXPECT_EQ(expected_autoclass, json["autoclass"]);
}

TEST(BucketMetadataPatchBuilder, SetAutoclassNoTerminal) {
  BucketMetadataPatchBuilder builder;
  builder.SetAutoclass(BucketAutoclass(true));

  auto actual = builder.BuildPatch();
  auto const json = nlohmann::json::parse(actual);
  ASSERT_TRUE(json.contains("autoclass")) << json;
  auto const expected_autoclass = nlohmann::json{{"enabled", true}};
  EXPECT_EQ(expected_autoclass, json["autoclass"]);
}

TEST(BucketMetadataPatchBuilder, ResetAutoclass) {
  BucketMetadataPatchBuilder builder;
  builder.ResetAutoclass();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_TRUE(json.contains("autoclass")) << json;
  ASSERT_TRUE(json["autoclass"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetBilling) {
  BucketMetadataPatchBuilder builder;
  builder.SetBilling(BucketBilling{true});

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("billing")) << json;
  ASSERT_TRUE(json["billing"].is_object()) << json;
  EXPECT_TRUE(json["billing"].value("requesterPays", false)) << json;
}

TEST(BucketMetadataPatchBuilder, ResetBilling) {
  BucketMetadataPatchBuilder builder;
  builder.ResetBilling();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("billing")) << json;
  ASSERT_TRUE(json["billing"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetCors) {
  BucketMetadataPatchBuilder builder;
  std::vector<CorsEntry> v;
  // NOLINTNEXTLINE(modernize-use-emplace) - brace initialization
  v.push_back(CorsEntry{{}, {"method1", "method2"}, {}, {"header1"}});
  // NOLINTNEXTLINE(modernize-use-emplace) - brace initialization
  v.push_back(CorsEntry{86400, {}, {"origin1"}, {}});
  builder.SetCors(v);

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("cors")) << json;
  ASSERT_TRUE(json["cors"].is_array()) << json;
  ASSERT_EQ(2U, json["cors"].size()) << json;
  EXPECT_EQ(0, json["cors"][0].count("maxAgeSeconds")) << json;
  EXPECT_EQ(1, json["cors"][0].count("method")) << json;
  EXPECT_EQ(0, json["cors"][0].count("origin")) << json;
  EXPECT_EQ(1, json["cors"][0].count("responseHeader")) << json;

  EXPECT_EQ(1, json["cors"][1].count("maxAgeSeconds")) << json;
  EXPECT_EQ(0, json["cors"][1].count("method")) << json;
  EXPECT_EQ(1, json["cors"][1].count("origin")) << json;
  EXPECT_EQ(0, json["cors"][1].count("responseHeader")) << json;
}

TEST(BucketMetadataPatchBuilder, ResetCors) {
  BucketMetadataPatchBuilder builder;
  builder.ResetCors();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("cors")) << json;
  ASSERT_TRUE(json["cors"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetDefaultEventBasedHold) {
  BucketMetadataPatchBuilder builder;
  builder.SetDefaultEventBasedHold(true);

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("defaultEventBasedHold")) << json;
  ASSERT_TRUE(json["defaultEventBasedHold"].is_boolean()) << json;
  EXPECT_TRUE(json.value("defaultEventBasedHold", false)) << json;
}

TEST(BucketMetadataPatchBuilder, ResetDefaultEventBasedHold) {
  BucketMetadataPatchBuilder builder;
  builder.ResetDefaultEventBasedHold();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("defaultEventBasedHold")) << json;
  ASSERT_TRUE(json["defaultEventBasedHold"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetDefaultAcl) {
  BucketMetadataPatchBuilder builder;
  builder.SetDefaultAcl(
      {internal::ObjectAccessControlParser::FromString(
           R"""({"entity": "user-test-user", "role": "OWNER"})""")
           .value()});

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("defaultObjectAcl")) << json;
  ASSERT_TRUE(json["defaultObjectAcl"].is_array()) << json;
  ASSERT_EQ(1U, json["defaultObjectAcl"].size()) << json;
  EXPECT_EQ("user-test-user", json["defaultObjectAcl"][0].value("entity", ""))
      << json;
  EXPECT_EQ("OWNER", json["defaultObjectAcl"][0].value("role", "")) << json;
}

TEST(BucketMetadataPatchBuilder, ResetDefaultAcl) {
  BucketMetadataPatchBuilder builder;
  builder.ResetDefaultAcl();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("defaultObjectAcl")) << json;
  ASSERT_TRUE(json["defaultObjectAcl"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetIamConfiguration) {
  BucketMetadataPatchBuilder builder;
  BucketIamConfiguration config;
  config.public_access_prevention = "enforced";
  config.uniform_bucket_level_access = UniformBucketLevelAccess{true, {}};
  builder.SetIamConfiguration(config);

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("iamConfiguration")) << json;
  ASSERT_TRUE(json["iamConfiguration"].is_object()) << json;
  auto const expected = nlohmann::json{
      {"publicAccessPrevention", "enforced"},
      {"uniformBucketLevelAccess", {{"enabled", true}}},
  };
  EXPECT_EQ(json["iamConfiguration"], expected);
}

TEST(BucketMetadataPatchBuilder, ResetIamConfiguration) {
  BucketMetadataPatchBuilder builder;
  builder.ResetIamConfiguration();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("iamConfiguration")) << json;
  ASSERT_TRUE(json["iamConfiguration"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetEncryption) {
  BucketMetadataPatchBuilder builder;
  std::string expected =
      "projects/test-project-name/locations/us-central1/keyRings/"
      "test-keyring-name/cryptoKeys/test-key-name";
  builder.SetEncryption(BucketEncryption{expected});

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("encryption")) << json;
  ASSERT_TRUE(json["encryption"].is_object()) << json;
  EXPECT_EQ(expected, json["encryption"].value("defaultKmsKeyName", ""))
      << json;
}

TEST(BucketMetadataPatchBuilder, ResetEncryption) {
  BucketMetadataPatchBuilder builder;
  builder.ResetEncryption();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("encryption")) << json;
  ASSERT_TRUE(json["encryption"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetLabels) {
  BucketMetadataPatchBuilder builder;
  builder.SetLabel("test-label1", "v1");
  builder.SetLabel("test-label2", "v2");
  builder.ResetLabel("test-label3");

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("labels")) << json;
  ASSERT_TRUE(json["labels"].is_object()) << json;
  EXPECT_EQ("v1", json["labels"].value("test-label1", "")) << json;
  EXPECT_EQ("v2", json["labels"].value("test-label2", "")) << json;
  EXPECT_TRUE(json["labels"]["test-label3"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, ResetLabels) {
  BucketMetadataPatchBuilder builder;
  builder.ResetLabels();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("labels")) << json;
  ASSERT_TRUE(json["labels"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetLifecycle) {
  BucketMetadataPatchBuilder builder;
  BucketLifecycle lifecycle;
  LifecycleRule r1(LifecycleRule::MaxAge(365), LifecycleRule::Delete());
  LifecycleRule r2(LifecycleRule::ConditionConjunction(
                       LifecycleRule::MatchesStorageClassStandard(),
                       LifecycleRule::NumNewerVersions(3)),
                   LifecycleRule::SetStorageClassNearline());
  LifecycleRule r3(
      LifecycleRule::ConditionConjunction(
          LifecycleRule::MaxAge(1),
          LifecycleRule::CreatedBefore(absl::CivilDay(2022, 01, 01)),
          LifecycleRule::IsLive(true),
          LifecycleRule::MatchesStorageClassArchive(),
          LifecycleRule::NumNewerVersions(2),
          LifecycleRule::DaysSinceNoncurrentTime(3),
          LifecycleRule::NoncurrentTimeBefore(absl::CivilDay(2022, 01, 02)),
          LifecycleRule::DaysSinceCustomTime(4),
          LifecycleRule::CustomTimeBefore(absl::CivilDay(2022, 01, 03)),
          LifecycleRule::MatchesPrefixes({"p1/", "p2/"}),
          LifecycleRule::MatchesSuffixes({".exe", ".com"})),
      LifecycleRule::SetStorageClassColdline());
  lifecycle.rule.emplace_back(r1);
  lifecycle.rule.emplace_back(r2);
  lifecycle.rule.emplace_back(r3);
  builder.SetLifecycle(lifecycle);

  auto patch = builder.BuildPatch();
  auto const actual = nlohmann::json::parse(patch);
  auto const expected = nlohmann::json::parse(R"js({
    "lifecycle": {
      "rule": [
        {"action": {"type": "Delete"}, "condition": {"age": 365}},
        {"action": {"type": "SetStorageClass", "storageClass": "NEARLINE"},
         "condition": {"matchesStorageClass": ["STANDARD"], "numNewerVersions": 3}},
        {"action": {"type": "SetStorageClass", "storageClass": "COLDLINE"},
         "condition": {
            "age": 1,
            "createdBefore": "2022-01-01",
            "isLive": true,
            "matchesStorageClass": ["ARCHIVE"],
            "numNewerVersions": 2,
            "daysSinceNoncurrentTime": 3,
            "noncurrentTimeBefore": "2022-01-02",
            "daysSinceCustomTime": 4,
            "customTimeBefore": "2022-01-03",
            "matchesPrefix": [ "p1/", "p2/" ],
            "matchesSuffix": [ ".exe", ".com" ]
         }
        }
      ]
    }
  })js");
  EXPECT_EQ(expected, actual) << nlohmann::json::diff(actual, expected);
}

TEST(BucketMetadataPatchBuilder, ResetLifecycle) {
  BucketMetadataPatchBuilder builder;
  builder.ResetLifecycle();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("lifecycle")) << json;
  ASSERT_TRUE(json["lifecycle"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetLogging) {
  BucketMetadataPatchBuilder builder;
  builder.SetLogging(BucketLogging{"test-log-bucket", "test-log-prefix"});

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("logging")) << json;
  ASSERT_TRUE(json["logging"].is_object()) << json;
  EXPECT_EQ("test-log-bucket", json["logging"].value("logBucket", "")) << json;
  EXPECT_EQ("test-log-prefix", json["logging"].value("logObjectPrefix", ""))
      << json;
}

TEST(BucketMetadataPatchBuilder, ResetLogging) {
  BucketMetadataPatchBuilder builder;
  builder.ResetLogging();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("logging")) << json;
  ASSERT_TRUE(json["logging"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetName) {
  BucketMetadataPatchBuilder builder;
  builder.SetName("test-bucket-changed-name");

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("name")) << json;
  ASSERT_TRUE(json["name"].is_string()) << json;
  EXPECT_EQ("test-bucket-changed-name", json.value("name", "")) << json;
}

TEST(BucketMetadataPatchBuilder, ResetName) {
  BucketMetadataPatchBuilder builder;
  builder.ResetName();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("name")) << json;
  ASSERT_TRUE(json["name"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetRetentionPolicy) {
  BucketMetadataPatchBuilder builder;
  builder.SetRetentionPolicy(BucketRetentionPolicy{
      std::chrono::seconds(60),
      google::cloud::internal::ParseRfc3339("2018-01-01T00:00:00Z").value(),
      false});

  auto actual_patch = builder.BuildPatch();
  auto actual_json = nlohmann::json::parse(actual_patch);
  auto expected_json = nlohmann::json{
      {"retentionPolicy", nlohmann::json{{"retentionPeriod", 60}}}};
  EXPECT_EQ(expected_json, actual_json);
}

TEST(BucketMetadataPatchBuilder, ResetRetentionPolicy) {
  BucketMetadataPatchBuilder builder;
  builder.ResetRetentionPolicy();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("retentionPolicy")) << json;
  ASSERT_TRUE(json["retentionPolicy"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetRpo) {
  BucketMetadataPatchBuilder builder;
  builder.SetRpo("ASYNC_TURBO");

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_TRUE(json.contains("rpo")) << json;
  ASSERT_TRUE(json["rpo"].is_string()) << json;
  EXPECT_EQ("ASYNC_TURBO", json.value("rpo", "")) << json;
}

TEST(BucketMetadataPatchBuilder, ResetRpo) {
  BucketMetadataPatchBuilder builder;
  builder.ResetRpo();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_TRUE(json.contains("rpo")) << json;
  ASSERT_TRUE(json["rpo"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetStorageClass) {
  BucketMetadataPatchBuilder builder;
  builder.SetStorageClass("NEARLINE");

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("storageClass")) << json;
  ASSERT_TRUE(json["storageClass"].is_string()) << json;
  EXPECT_EQ("NEARLINE", json.value("storageClass", "")) << json;
}

TEST(BucketMetadataPatchBuilder, ResetStorageClass) {
  BucketMetadataPatchBuilder builder;
  builder.ResetStorageClass();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("storageClass")) << json;
  ASSERT_TRUE(json["storageClass"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetVersioning) {
  BucketMetadataPatchBuilder builder;
  builder.SetVersioning(BucketVersioning{true});

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("versioning")) << json;
  ASSERT_TRUE(json["versioning"].is_object()) << json;
  EXPECT_TRUE(json["versioning"].value("enabled", false)) << json;
}

TEST(BucketMetadataPatchBuilder, ResetVersioning) {
  BucketMetadataPatchBuilder builder;
  builder.ResetVersioning();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("versioning")) << json;
  ASSERT_TRUE(json["versioning"].is_null()) << json;
}

TEST(BucketMetadataPatchBuilder, SetWebsite) {
  BucketMetadataPatchBuilder builder;
  builder.SetWebsite(BucketWebsite{"index.htm", "404.htm"});

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("website")) << json;
  ASSERT_TRUE(json["website"].is_object()) << json;
  EXPECT_EQ("index.htm", json["website"].value("mainPageSuffix", "")) << json;
  EXPECT_EQ("404.htm", json["website"].value("notFoundPage", "")) << json;
}

TEST(BucketMetadataPatchBuilder, ResetWebsite) {
  BucketMetadataPatchBuilder builder;
  builder.ResetWebsite();

  auto actual = builder.BuildPatch();
  auto json = nlohmann::json::parse(actual);
  ASSERT_EQ(1U, json.count("website")) << json;
  ASSERT_TRUE(json["website"].is_null()) << json;
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

// Copyright 2021 Google LLC
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

#include "google/cloud/storage/internal/grpc_bucket_metadata_parser.h"
#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::ElementsAreArray;

TEST(GrpcBucketMetadataParser, BucketAllFieldsRoundtrip) {
  google::storage::v2::Bucket input;
  // Keep the proto fields in the order they show up in the proto file. It is
  // easier to add new fields and to inspect the test for missing fields
  auto constexpr kText = R"pb(
    name: "projects/_/buckets/test-bucket-id"
    bucket_id: "test-bucket-id"
    project: "projects/123456"
    metageneration: 1234567
    location: "test-location"
    location_type: "REGIONAL"
    storage_class: "test-storage-class"
    rpo: "test-rpo"
    acl: { role: "test-role1" entity: "test-entity1", etag: "test-etag1" }
    acl: { role: "test-role2" entity: "test-entity2", etag: "test-etag2" }
    default_object_acl: {
      role: "test-role3"
      entity: "test-entity3",
      etag: "test-etag3"
    }
    default_object_acl: {
      role: "test-role4"
      entity: "test-entity4",
      etag: "test-etag4"
    }
    lifecycle {
      rule {
        action { type: "Delete" }
        condition {
          age_days: 90
          is_live: false
          matches_storage_class: "NEARLINE"
        }
      }
      rule {
        action { type: "SetStorageClass" storage_class: "NEARLINE" }
        condition {
          age_days: 7
          is_live: true
          matches_storage_class: "STANDARD"
        }
      }
    }
    create_time: { seconds: 1565194924 nanos: 123456000 }
    cors: {
      origin: "test-origin-0"
      origin: "test-origin-1"
      method: "GET"
      method: "PUT"
      response_header: "test-header-0"
      response_header: "test-header-1"
      max_age_seconds: 1800
    }
    cors: {
      origin: "test-origin-2"
      origin: "test-origin-3"
      method: "POST"
      response_header: "test-header-3"
      max_age_seconds: 3600
    }
    update_time: { seconds: 1565194925 nanos: 123456000 }
    default_event_based_hold: true
    labels: { key: "test-key-1" value: "test-value-1" }
    labels: { key: "test-key-2" value: "test-value-2" }
    website { main_page_suffix: "index.html" not_found_page: "404.html" }
    custom_placement_config {
      data_locations: "us-central1"
      data_locations: "us-east4"
    }
    versioning { enabled: true }
    logging {
      log_bucket: "projects/_/buckets/test-log-bucket"
      log_object_prefix: "test-log-object-prefix"
    }
    owner { entity: "test-entity" entity_id: "test-entity-id" }
    encryption { default_kms_key: "test-default-kms-key-name" }
    billing { requester_pays: true }
    retention_policy {
      effective_time { seconds: 1565194926 nanos: 123456000 }
      is_locked: true
      retention_period: 86400
    }
    iam_config {
      uniform_bucket_level_access {
        enabled: true
        lock_time { seconds: 1565194927 nanos: 123456000 }
      }
      public_access_prevention: "inherited"
    }
    etag: "test-etag"
  )pb";
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &input));

  // To get the dates in RFC-3339 format I used:
  //     date --rfc-3339=seconds --date=@1565194924
  auto const expected = BucketMetadataParser::FromString(R"""({
    "acl": [{
      "kind": "storage#bucketAccessControl",
      "bucket": "test-bucket-id",
      "role": "test-role1",
      "entity": "test-entity1",
      "etag": "test-etag1"
    }, {
      "kind": "storage#bucketAccessControl",
      "bucket": "test-bucket-id",
      "role": "test-role2",
      "entity": "test-entity2",
      "etag": "test-etag2"
    }],
    "defaultObjectAcl": [{
      "kind": "storage#objectAccessControl",
      "bucket": "test-bucket-id",
      "role": "test-role3",
      "entity": "test-entity3",
      "etag": "test-etag3"
    }, {
      "kind": "storage#objectAccessControl",
      "bucket": "test-bucket-id",
      "role": "test-role4",
      "entity": "test-entity4",
      "etag": "test-etag4"
    }],
    "lifecycle": {
      "rule": [{
        "action": { "type": "Delete" },
        "condition": {
          "age": 90,
          "isLive": false,
          "matchesStorageClass": "NEARLINE"
        }
      },
      {
        "action": { "type": "SetStorageClass", "storageClass": "NEARLINE" },
        "condition": {
          "age": 7,
          "isLive": true,
          "matchesStorageClass": "STANDARD"
        }
      }]
    },
    "timeCreated": "2019-08-07T16:22:04.123456000Z",
    "id": "test-bucket-id",
    "kind": "storage#bucket",
    "name": "test-bucket-id",
    "projectNumber": 123456,
    "metageneration": "1234567",
    "cors": [{
      "origin": ["test-origin-0", "test-origin-1"],
      "method": ["GET", "PUT"],
      "responseHeader": ["test-header-0", "test-header-1"],
      "maxAgeSeconds": 1800
    }, {
      "origin": ["test-origin-2", "test-origin-3"],
      "method": ["POST"],
      "responseHeader": ["test-header-3"],
      "maxAgeSeconds": 3600
    }],
    "location": "test-location",
    "storageClass": "test-storage-class",
    "updated": "2019-08-07T16:22:05.123456000Z",
    "defaultEventBasedHold": true,
    "labels": {
        "test-key-1": "test-value-1",
        "test-key-2": "test-value-2"
    },
    "website": {
      "mainPageSuffix": "index.html",
      "notFoundPage": "404.html"
    },
    "customPlacementConfig": {
      "dataLocations": ["us-central1", "us-east4"]
    },
    "versioning": { "enabled": true },
    "logging": {
      "logBucket": "test-log-bucket",
      "logObjectPrefix": "test-log-object-prefix"
    },
    "owner": { "entity": "test-entity", "entityId": "test-entity-id" },
    "encryption": { "defaultKmsKeyName": "test-default-kms-key-name" },
    "billing": { "requesterPays": true },
    "retentionPolicy": {
      "effectiveTime": "2019-08-07T16:22:06.123456000Z",
      "isLocked": true,
      "retentionPeriod": 86400
    },
    "rpo": "test-rpo",
    "locationType": "REGIONAL",
    "iamConfiguration": {
      "uniformBucketLevelAccess": {
        "enabled": true,
        "lockedTime": "2019-08-07T16:22:07.123456000Z"
      },
      "publicAccessPrevention": "inherited"
    },
    "etag": "test-etag"
  })""");
  ASSERT_THAT(expected, IsOk());

  auto const middle = GrpcBucketMetadataParser::FromProto(input);
  EXPECT_EQ(middle, *expected);

  auto const p1 = GrpcBucketMetadataParser::ToProto(middle);
  auto const p2 = GrpcBucketMetadataParser::ToProto(*expected);
  EXPECT_THAT(p1, IsProtoEqual(p2));

  auto const actual = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(actual, IsProtoEqual(input));
}

TEST(GrpcBucketMetadataParser, BucketBillingRoundtrip) {
  auto constexpr kText = R"pb(
    requester_pays: true
  )pb";
  google::storage::v2::Bucket::Billing start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected = BucketBilling{true};
  auto const middle = GrpcBucketMetadataParser::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

TEST(GrpcBucketMetadataParser, BucketCorsRoundtrip) {
  auto constexpr kText = R"pb(
    origin: "test-origin-1"
    origin: "test-origin-2"
    method: "GET"
    method: "PUT"
    response_header: "test-header-1"
    response_header: "test-header-2"
    max_age_seconds: 3600
  )pb";
  google::storage::v2::Bucket::Cors start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected = CorsEntry{3600,
                                  {"GET", "PUT"},
                                  {"test-origin-1", "test-origin-2"},
                                  {"test-header-1", "test-header-2"}};
  auto const middle = GrpcBucketMetadataParser::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

TEST(GrpcBucketMetadataParser, BucketEncryptionRoundtrip) {
  auto constexpr kText = R"pb(
    default_kms_key: "projects/test-p/locations/us/keyRings/test-kr/cryptoKeys/test-key"
  )pb";
  google::storage::v2::Bucket::Encryption start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected = BucketEncryption{
      "projects/test-p/locations/us/keyRings/test-kr/cryptoKeys/test-key"};
  auto const middle = GrpcBucketMetadataParser::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

TEST(GrpcBucketMetadataParser, BucketIamConfigRoundtrip) {
  auto constexpr kText = R"pb(
    uniform_bucket_level_access {
      enabled: true
      lock_time { seconds: 1234 nanos: 5678000 }
    }
    public_access_prevention: "enforced"
  )pb";
  google::storage::v2::Bucket::IamConfig start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto tp = std::chrono::system_clock::time_point{} +
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::seconds(1234) + std::chrono::nanoseconds(5678000));
  auto const expected = BucketIamConfiguration{
      /*.uniform_bucket_level_access=*/UniformBucketLevelAccess{true, tp},
      /*.public_access_prevention=*/PublicAccessPreventionEnforced()};
  auto const middle = GrpcBucketMetadataParser::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

TEST(GrpcBucketMetadataParser, LifecycleRuleActionRoundtrip) {
  auto constexpr kText = R"pb(
    type: "SetStorageClass" storage_class: "COLDLINE"
  )pb";
  google::storage::v2::Bucket::Lifecycle::Rule::Action start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected = LifecycleRule::SetStorageClass("COLDLINE");
  auto const middle = GrpcBucketMetadataParser::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

TEST(GrpcBucketMetadataParser, LifecycleRuleConditionRoundtrip) {
  auto constexpr kText = R"pb(
    age_days: 7
    created_before { year: 2021 month: 12 day: 20 }
    is_live: true
    num_newer_versions: 3
    matches_storage_class: "STANDARD"
    matches_storage_class: "NEARLINE"
    days_since_custom_time: 4
    custom_time_before { year: 2022 month: 2 day: 15 }
    days_since_noncurrent_time: 5
    noncurrent_time_before { year: 2022 month: 1 day: 2 }
    matches_prefix: "p1/"
    matches_prefix: "p2/"
    matches_suffix: ".txt"
    matches_suffix: ".html"
  )pb";
  google::storage::v2::Bucket::Lifecycle::Rule::Condition start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected = LifecycleRule::ConditionConjunction(
      LifecycleRule::MaxAge(7),
      LifecycleRule::CreatedBefore(absl::CivilDay(2021, 12, 20)),
      LifecycleRule::IsLive(true), LifecycleRule::NumNewerVersions(3),
      LifecycleRule::MatchesStorageClasses({"STANDARD", "NEARLINE"}),
      LifecycleRule::DaysSinceCustomTime(4),
      LifecycleRule::CustomTimeBefore(absl::CivilDay(2022, 2, 15)),
      LifecycleRule::DaysSinceNoncurrentTime(5),
      LifecycleRule::NoncurrentTimeBefore(absl::CivilDay(2022, 1, 2)),
      LifecycleRule::MatchesPrefixes({"p1/", "p2/"}),
      LifecycleRule::MatchesSuffixes({".txt", ".html"}));
  auto const middle = GrpcBucketMetadataParser::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

TEST(GrpcBucketMetadataParser, LifecycleRuleRoundtrip) {
  auto constexpr kText = R"pb(
    action { type: "Delete" }
    condition {
      age_days: 7
      created_before { year: 2021 month: 12 day: 20 }
      is_live: true
      num_newer_versions: 3
      matches_storage_class: "STANDARD"
      matches_storage_class: "NEARLINE"
      matches_prefix: "p1/"
      matches_prefix: "p2/"
      matches_suffix: ".txt"
      matches_suffix: ".html"
    }
  )pb";
  google::storage::v2::Bucket::Lifecycle::Rule start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected = LifecycleRule(
      LifecycleRule::ConditionConjunction(
          LifecycleRule::MaxAge(7),
          LifecycleRule::CreatedBefore(absl::CivilDay(2021, 12, 20)),
          LifecycleRule::IsLive(true), LifecycleRule::NumNewerVersions(3),
          LifecycleRule::MatchesStorageClasses({"STANDARD", "NEARLINE"}),
          LifecycleRule::MatchesPrefixes({"p1/", "p2/"}),
          LifecycleRule::MatchesSuffixes({".txt", ".html"})),
      LifecycleRule::Delete());
  auto const middle = GrpcBucketMetadataParser::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

TEST(GrpcBucketMetadataParser, BucketLifecycleRoundtrip) {
  auto constexpr kText = R"pb(
    rule {
      action { type: "SetStorageClass" storage_class: "NEARLINE" }
      condition { age_days: 7 is_live: true matches_storage_class: "STANDARD" }
    }
    rule {
      action { type: "Delete" }
      condition { age_days: 180 matches_storage_class: "NEARLINE" }
    }
  )pb";
  google::storage::v2::Bucket::Lifecycle start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected = BucketLifecycle{{
      LifecycleRule(LifecycleRule::ConditionConjunction(
                        LifecycleRule::MaxAge(7), LifecycleRule::IsLive(true),
                        LifecycleRule::MatchesStorageClassStandard()),
                    LifecycleRule::SetStorageClassNearline()),
      LifecycleRule(LifecycleRule::ConditionConjunction(
                        LifecycleRule::MaxAge(180),
                        LifecycleRule::MatchesStorageClassNearline()),
                    LifecycleRule::Delete()),
  }};
  auto const middle = GrpcBucketMetadataParser::FromProto(start);
  EXPECT_THAT(expected.rule, ElementsAreArray(middle.rule));
  auto const end = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

TEST(GrpcBucketMetadataParser, BucketLoggingRoundtrip) {
  auto constexpr kText = R"pb(
    log_bucket: "projects/_/buckets/test-bucket-name"
    log_object_prefix: "test-object-prefix/"
  )pb";
  google::storage::v2::Bucket::Logging start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected =
      BucketLogging{"test-bucket-name", "test-object-prefix/"};
  auto const middle = GrpcBucketMetadataParser::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

TEST(GrpcBucketMetadataParser, BucketRetentionPolicyRoundtrip) {
  auto constexpr kText = R"pb(
    retention_period: 3600
    effective_time { seconds: 1234 nanos: 5678000 }
    is_locked: true
  )pb";
  google::storage::v2::Bucket::RetentionPolicy start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto tp = std::chrono::system_clock::time_point{} +
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::seconds(1234) + std::chrono::nanoseconds(5678000));
  auto const expected =
      BucketRetentionPolicy{std::chrono::seconds(3600), tp, true};
  auto const middle = GrpcBucketMetadataParser::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

TEST(GrpcBucketMetadataParser, BucketVersioningRoundtrip) {
  auto constexpr kText = R"pb(
    enabled: true
  )pb";
  google::storage::v2::Bucket::Versioning start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected = BucketVersioning{true};
  auto const middle = GrpcBucketMetadataParser::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

TEST(GrpcBucketMetadataParser, BucketWebsiteRoundtrip) {
  auto constexpr kText = R"pb(
    main_page_suffix: "index.html"
    not_found_page: "404.html"
  )pb";
  google::storage::v2::Bucket::Website start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected = BucketWebsite{"index.html", "404.html"};
  auto const middle = GrpcBucketMetadataParser::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

TEST(GrpcBucketMetadataParser, BucketCustomPlacementConfigRoundtrip) {
  auto constexpr kText = R"pb(
    data_locations: "us-central1"
    data_locations: "us-east4"
  )pb";
  google::storage::v2::Bucket::CustomPlacementConfig start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected =
      BucketCustomPlacementConfig{{"us-central1", "us-east4"}};
  auto const middle = GrpcBucketMetadataParser::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcBucketMetadataParser::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

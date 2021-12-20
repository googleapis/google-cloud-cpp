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
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::ElementsAreArray;

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
  )pb";
  google::storage::v2::Bucket::IamConfig start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto tp = std::chrono::system_clock::time_point{} +
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                std::chrono::seconds(1234) + std::chrono::nanoseconds(5678000));
  auto const expected = BucketIamConfiguration{
      /*.uniform_bucket_level_access=*/UniformBucketLevelAccess{true, tp},
      /*.public_access_prevention=*/{}};
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
  )pb";
  google::storage::v2::Bucket::Lifecycle::Rule::Condition start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected = LifecycleRule::ConditionConjunction(
      LifecycleRule::MaxAge(7),
      LifecycleRule::CreatedBefore(absl::CivilDay(2021, 12, 20)),
      LifecycleRule::IsLive(true), LifecycleRule::NumNewerVersions(3),
      LifecycleRule::MatchesStorageClasses({"STANDARD", "NEARLINE"}));
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
    }
  )pb";
  google::storage::v2::Bucket::Lifecycle::Rule start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected = LifecycleRule(
      LifecycleRule::ConditionConjunction(
          LifecycleRule::MaxAge(7),
          LifecycleRule::CreatedBefore(absl::CivilDay(2021, 12, 20)),
          LifecycleRule::IsLive(true), LifecycleRule::NumNewerVersions(3),
          LifecycleRule::MatchesStorageClasses({"STANDARD", "NEARLINE"})),
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
    log_bucket: "test-bucket-name"
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

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

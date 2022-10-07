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

#include "google/cloud/storage/internal/grpc_bucket_request_parser.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace v2 = ::google::storage::v2;
using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

TEST(GrpcBucketRequestParser, DeleteBucketMetadataAllOptions) {
  v2::DeleteBucketRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "projects/_/buckets/test-bucket"
        if_metageneration_match: 1
        if_metageneration_not_match: 2
      )pb",
      &expected));

  storage::internal::DeleteBucketRequest req("test-bucket");
  req.set_multiple_options(
      storage::IfMetagenerationMatch(1), storage::IfMetagenerationNotMatch(2),
      storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"));

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, GetBucketMetadataAllOptions) {
  v2::GetBucketRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "projects/_/buckets/test-bucket"
        if_metageneration_match: 1
        if_metageneration_not_match: 2
        read_mask { paths: "*" }
      )pb",
      &expected));

  storage::internal::GetBucketMetadataRequest req("test-bucket");
  req.set_multiple_options(
      storage::IfMetagenerationMatch(1), storage::IfMetagenerationNotMatch(2),
      storage::Projection("full"), storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"));

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, CreateBucketMetadataAllOptions) {
  v2::CreateBucketRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parent: "projects/test-project"
        bucket_id: "test-bucket"
        bucket {
          location: "us-central1"
          storage_class: "NEARLINE"
          rpo: "ASYNC_TURBO"
          acl { entity: "allUsers" role: "READER" }
          autoclass {
            enabled: true
            toggle_time {}
          }
          billing { requester_pays: true }
          default_object_acl {
            entity: "user:test-user@example.com"
            role: "READER"
          }
          lifecycle {
            rule {
              action { type: "Delete" }
              condition {
                age_days: 90
                is_live: false
                matches_storage_class: "NEARLINE"
                matches_prefix: "p1/"
                matches_suffix: ".txt"
              }
            }
          }
          cors {
            origin: "test-origin-0"
            origin: "test-origin-1"
            method: "GET"
            method: "PUT"
            response_header: "test-header-0"
            response_header: "test-header-1"
            max_age_seconds: 1800
          }
          default_event_based_hold: true
          labels { key: "k0" value: "v0" }
          logging {
            log_bucket: "projects/_/buckets/test-log-bucket"
            log_object_prefix: "test-log-object-prefix"
          }
          versioning { enabled: true }
          website { main_page_suffix: "index.html" not_found_page: "404.html" }
        }
        predefined_acl: "projectPrivate"
        predefined_default_object_acl: "private"
      )pb",
      &expected));

  storage::internal::CreateBucketRequest req(
      "test-project",
      storage::BucketMetadata{}
          .set_name("test-bucket")
          .set_storage_class("NEARLINE")
          .set_location("us-central1")
          .set_autoclass(storage::BucketAutoclass{true})
          .set_billing(storage::BucketBilling(true))
          .set_rpo(storage::RpoAsyncTurbo())
          .set_versioning(storage::BucketVersioning(true))
          .set_website(storage::BucketWebsite{"index.html", "404.html"})
          .upsert_label("k0", "v0")
          .set_default_event_based_hold(true)
          .set_cors({storage::CorsEntry{
              /*.max_age_seconds=*/static_cast<std::uint64_t>(1800),
              /*.method=*/{"GET", "PUT"},
              /*.origin=*/{"test-origin-0", "test-origin-1"},
              /*.response_header=*/{"test-header-0", "test-header-1"},
          }})
          .set_acl({storage::BucketAccessControl()
                        .set_entity("allUsers")
                        .set_role("READER")})
          .set_default_acl({storage::ObjectAccessControl()
                                .set_entity("user:test-user@example.com")
                                .set_role("READER")})
          .set_lifecycle(storage::BucketLifecycle{{storage::LifecycleRule(
              storage::LifecycleRule::ConditionConjunction(
                  storage::LifecycleRule::MaxAge(90),
                  storage::LifecycleRule::IsLive(false),
                  storage::LifecycleRule::MatchesStorageClassNearline(),
                  storage::LifecycleRule::MatchesPrefix("p1/"),
                  storage::LifecycleRule::MatchesSuffix(".txt")),
              storage::LifecycleRule::Delete())}})
          .set_logging(storage::BucketLogging{
              /*.log_bucket=*/"test-log-bucket",
              /*.log_object_prefix=*/"test-log-object-prefix"}));
  req.set_multiple_options(
      storage::PredefinedAcl::ProjectPrivate(),
      storage::PredefinedDefaultObjectAcl::Private(),
      storage::Projection("full"), storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"));

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, ListBucketsRequestAllOptions) {
  google::storage::v2::ListBucketsRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parent: "projects/test-project"
        page_size: 123
        page_token: "test-token"
        prefix: "test-prefix"
        read_mask { paths: [ "*" ] }
      )pb",
      &expected));

  storage::internal::ListBucketsRequest req("test-project");
  req.set_page_token("test-token");
  req.set_multiple_options(
      storage::MaxResults(123), storage::Prefix("test-prefix"),
      storage::Projection("full"), storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"));

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, ListBucketsResponse) {
  google::storage::v2::ListBucketsResponse input;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        buckets {
          name: "projects/_/buckets/test-bucket-1"
          bucket_id: "test-bucket-1"
        }
        buckets {
          name: "projects/_/buckets/test-bucket-2"
          bucket_id: "test-bucket-2"
        }
        next_page_token: "test-token"
      )pb",
      &input));

  auto const actual = FromProto(input);
  EXPECT_EQ(actual.next_page_token, "test-token");
  std::vector<std::string> names;
  std::transform(actual.items.begin(), actual.items.end(),
                 std::back_inserter(names),
                 [](storage::BucketMetadata const& b) { return b.name(); });
  EXPECT_THAT(names, ElementsAre("test-bucket-1", "test-bucket-2"));
}

TEST(GrpcBucketRequestParser, LockBucketRetentionPolicyRequestAllOptions) {
  google::storage::v2::LockBucketRetentionPolicyRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket" if_metageneration_match: 7
      )pb",
      &expected));

  storage::internal::LockBucketRetentionPolicyRequest req("test-bucket",
                                                          /*metageneration=*/7);
  req.set_multiple_options(storage::UserProject("test-user-project"),
                           storage::QuotaUser("test-quota-user"),
                           storage::UserIp("test-user-ip"));

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, GetIamPolicyRequest) {
  google::iam::v1::GetIamPolicyRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        resource: "projects/_/buckets/test-bucket"
        options { requested_policy_version: 3 }
      )pb",
      &expected));

  storage::internal::GetBucketIamPolicyRequest req("test-bucket");
  req.set_multiple_options(storage::RequestedPolicyVersion(3),
                           storage::UserProject("test-user-project"),
                           storage::QuotaUser("test-quota-user"),
                           storage::UserIp("test-user-ip"));
  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, NativeIamPolicy) {
  google::iam::v1::Policy input;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        version: 7
        bindings {
          role: "role/test.only"
          members: "test-1"
          members: "test-2"
          condition {
            expression: "test-expression"
            title: "test-title"
            description: "test-description"
            location: "test-location"
          }
        }
        bindings { role: "role/another.test.only" members: "test-3" }
        etag: "test-etag"
      )pb",
      &input));

  auto const actual = FromProto(input);
  EXPECT_EQ(7, actual.version());
  EXPECT_EQ("test-etag", actual.etag());
  storage::NativeIamBinding b0(
      "role/test.only", {"test-1", "test-2"},
      {"test-expression", "test-title", "test-description", "test-location"});
  storage::NativeIamBinding b1("role/another.test.only", {"test-3"});

  auto match_expr = [](storage::NativeExpression const& e) {
    return AllOf(
        Property(&storage::NativeExpression::expression, e.expression()),
        Property(&storage::NativeExpression::title, e.title()),
        Property(&storage::NativeExpression::description, e.description()),
        Property(&storage::NativeExpression::location, e.location()));
  };
  auto match_binding = [&](storage::NativeIamBinding const& b) {
    return AllOf(
        Property(&storage::NativeIamBinding::role, b.role()),
        Property(&storage::NativeIamBinding::members,
                 ElementsAreArray(b.members())),
        Property(&storage::NativeIamBinding::has_condition, b.has_condition()));
  };
  ASSERT_THAT(actual.bindings(),
              ElementsAre(match_binding(b0), match_binding(b1)));
  ASSERT_TRUE(actual.bindings()[0].has_condition());
  ASSERT_THAT(actual.bindings()[0].condition(), match_expr(b0.condition()));
}

TEST(GrpcBucketRequestParser, SetNativeBucketIamPolicyRequest) {
  google::iam::v1::SetIamPolicyRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        resource: "projects/_/buckets/test-bucket"
        policy {
          version: 3
          bindings {
            role: "role/test.only"
            members: "test-1"
            members: "test-2"
            condition {
              expression: "test-expression"
              title: "test-title"
              description: "test-description"
              location: "test-location"
            }
          }
          bindings { role: "role/another.test.only" members: "test-3" }
          etag: "test-etag"
        }
      )pb",
      &expected));

  storage::NativeIamBinding b0(
      "role/test.only", {"test-1", "test-2"},
      {"test-expression", "test-title", "test-description", "test-location"});
  storage::NativeIamBinding b1("role/another.test.only", {"test-3"});
  storage::NativeIamPolicy policy({b0, b1}, "test-etag", 3);

  storage::internal::SetNativeBucketIamPolicyRequest req("test-bucket", policy);
  req.set_multiple_options(storage::UserProject("test-user-project"),
                           storage::QuotaUser("test-quota-user"),
                           storage::UserIp("test-user-ip"));
  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, TestBucketIamPermissionsRequest) {
  google::iam::v1::TestIamPermissionsRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        resource: "projects/_/buckets/test-bucket"
        permissions: "test-only.permission.1"
        permissions: "test-only.permission.2"
      )pb",
      &expected));

  storage::internal::TestBucketIamPermissionsRequest req(
      "test-bucket", {"test-only.permission.1", "test-only.permission.2"});
  req.set_multiple_options(storage::UserProject("test-user-project"),
                           storage::QuotaUser("test-quota-user"),
                           storage::UserIp("test-user-ip"));
  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, TestBucketIamPermissionsResponse) {
  google::iam::v1::TestIamPermissionsResponse input;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        permissions: "test-only.permission.1"
        permissions: "test-only.permission.2"
      )pb",
      &input));

  auto const actual = FromProto(input);
  EXPECT_THAT(actual.permissions,
              ElementsAre("test-only.permission.1", "test-only.permission.2"));
}

TEST(GrpcBucketRequestParser, PatchBucketRequestAllOptions) {
  google::storage::v2::UpdateBucketRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        bucket {
          name: "projects/_/buckets/bucket-name"
          storage_class: "NEARLINE"
          rpo: "ASYNC_TURBO"
          acl { entity: "allUsers" role: "READER" }
          default_object_acl { entity: "user:test@example.com" role: "WRITER" }
          lifecycle {
            rule {
              action { type: "Delete" }
              condition {
                age_days: 90
                created_before { year: 2022 month: 2 day: 2 }
                is_live: true
                num_newer_versions: 7
                matches_storage_class: "STANDARD"
                days_since_custom_time: 42
                days_since_noncurrent_time: 84
                noncurrent_time_before { year: 2022 month: 2 day: 15 }
                matches_prefix: "p1/"
                matches_prefix: "p2/"
                matches_suffix: ".html"
              }
            }
          }
          cors {
            origin: "test-origin-0"
            origin: "test-origin-1"
            method: "GET"
            method: "PUT"
            response_header: "test-header-0"
            response_header: "test-header-1"
            max_age_seconds: 1800
          }
          cors { origin: "test-origin-0" origin: "test-origin-1" }
          cors { method: "GET" method: "PUT" }
          cors {
            response_header: "test-header-0"
            response_header: "test-header-1"
          }
          cors { max_age_seconds: 1800 }
          default_event_based_hold: true
          labels { key: "key0" value: "value0" }
          website { main_page_suffix: "index.html" not_found_page: "404.html" }
          versioning { enabled: true }
          logging {
            log_bucket: "projects/_/buckets/test-log-bucket"
            log_object_prefix: "test-log-prefix"
          }
          encryption { default_kms_key: "test-only-kms-key" }
          autoclass { enabled: true }
          billing { requester_pays: true }
          retention_policy { retention_period: 123000 }
          iam_config {
            uniform_bucket_level_access { enabled: true }
            public_access_prevention: "enforced"
          }
        }
        predefined_acl: "projectPrivate"
        predefined_default_object_acl: "projectPrivate"
        if_metageneration_match: 3
        if_metageneration_not_match: 4
        update_mask {}
      )pb",
      &expected));

  storage::internal::PatchBucketRequest req(
      "bucket-name",
      storage::BucketMetadataPatchBuilder{}
          .SetStorageClass("NEARLINE")
          .SetRpo(storage::RpoAsyncTurbo())
          .SetAcl({storage::BucketAccessControl{}
                       .set_entity("allUsers")
                       .set_role("READER")})
          .SetDefaultAcl({storage::ObjectAccessControl{}
                              .set_entity("user:test@example.com")
                              .set_role("WRITER")})
          .SetLifecycle(storage::BucketLifecycle{{storage::LifecycleRule(
              storage::LifecycleRule::ConditionConjunction(
                  storage::LifecycleRule::MaxAge(90),
                  storage::LifecycleRule::CreatedBefore(
                      absl::CivilDay(2022, 2, 2)),
                  storage::LifecycleRule::IsLive(true),
                  storage::LifecycleRule::NumNewerVersions(7),
                  storage::LifecycleRule::MatchesStorageClassStandard(),
                  storage::LifecycleRule::DaysSinceCustomTime(42),
                  storage::LifecycleRule::DaysSinceNoncurrentTime(84),
                  storage::LifecycleRule::NoncurrentTimeBefore(
                      absl::CivilDay(2022, 2, 15)),
                  storage::LifecycleRule::MatchesPrefixes({"p1/", "p2/"}),
                  storage::LifecycleRule::MatchesSuffix(".html")),
              storage::LifecycleRule::Delete())}})
          .SetCors({
              storage::CorsEntry{
                  /*.max_age_seconds=*/static_cast<std::uint64_t>(1800),
                  /*.method=*/{"GET", "PUT"},
                  /*.origin=*/{"test-origin-0", "test-origin-1"},
                  /*.response_header=*/{"test-header-0", "test-header-1"}},
              storage::CorsEntry{/*.max_age_seconds=*/absl::nullopt,
                                 /*.method=*/{},
                                 /*.origin=*/{"test-origin-0", "test-origin-1"},
                                 /*.response_header=*/{}},
              storage::CorsEntry{/*.max_age_seconds=*/absl::nullopt,
                                 /*.method=*/{"GET", "PUT"},
                                 /*.origin=*/{},
                                 /*.response_header=*/{}},
              storage::CorsEntry{
                  /*.max_age_seconds=*/absl::nullopt,
                  /*.method=*/{},
                  /*.origin=*/{},
                  /*.response_header=*/{"test-header-0", "test-header-1"}},
              storage::CorsEntry{
                  /*.max_age_seconds=*/static_cast<std::uint64_t>(1800),
                  /*.method=*/{},
                  /*.origin=*/{},
                  /*.response_header=*/{}},
          })
          .SetDefaultEventBasedHold(true)
          .SetLabel("key0", "value0")
          .SetWebsite(storage::BucketWebsite{"index.html", "404.html"})
          .SetVersioning(storage::BucketVersioning{true})
          .SetLogging(
              storage::BucketLogging{"test-log-bucket", "test-log-prefix"})
          .SetEncryption(storage::BucketEncryption{
              /*.default_kms_key=*/"test-only-kms-key"})
          .SetAutoclass(storage::BucketAutoclass{true})
          .SetBilling(storage::BucketBilling{/*.requester_pays=*/true})
          .SetRetentionPolicy(std::chrono::seconds(123000))
          .SetIamConfiguration(storage::BucketIamConfiguration{
              /*.uniform_bucket_level_access=*/storage::
                  UniformBucketLevelAccess{true, {}},
              /*.public_access_prevention=*/storage::
                  PublicAccessPreventionEnforced()}));
  req.set_multiple_options(
      storage::IfMetagenerationMatch(3), storage::IfMetagenerationNotMatch(4),
      storage::PredefinedAcl("projectPrivate"),
      storage::PredefinedDefaultObjectAcl("projectPrivate"),
      storage::Projection("full"), storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"));

  auto actual = ToProto(req);
  ASSERT_STATUS_OK(actual);
  // First check the paths. We do not care about their order, so checking them
  // with IsProtoEqual does not work.
  EXPECT_THAT(actual->update_mask().paths(),
              UnorderedElementsAre(
                  "storage_class", "rpo", "acl", "default_object_acl",
                  "lifecycle", "cors", "default_event_based_hold", "labels",
                  "website", "versioning", "logging", "encryption", "autoclass",
                  "billing", "retention_policy", "iam_config"));

  // Clear the paths, which we already compared, and compare the proto.
  actual->mutable_update_mask()->clear_paths();
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, PatchBucketRequestAllResets) {
  google::storage::v2::UpdateBucketRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        bucket { name: "projects/_/buckets/bucket-name" }
        update_mask {}
      )pb",
      &expected));

  storage::internal::PatchBucketRequest req(
      "bucket-name", storage::BucketMetadataPatchBuilder{}
                         .SetStorageClass("NEARLINE")
                         .ResetAcl()
                         .ResetAutoclass()
                         .ResetBilling()
                         .ResetCors()
                         .ResetDefaultEventBasedHold()
                         .ResetDefaultAcl()
                         .ResetIamConfiguration()
                         .ResetEncryption()
                         .ResetLabels()
                         .ResetLifecycle()
                         .ResetLogging()
                         .ResetRetentionPolicy()
                         .ResetRpo()
                         .ResetStorageClass()
                         .ResetVersioning()
                         .ResetWebsite());

  auto actual = ToProto(req);
  ASSERT_STATUS_OK(actual);
  // First check the paths. We do not care about their order, so checking them
  // with IsProtoEqual does not work.
  EXPECT_THAT(actual->update_mask().paths(),
              UnorderedElementsAre(
                  "storage_class", "rpo", "acl", "default_object_acl",
                  "lifecycle", "cors", "default_event_based_hold", "labels",
                  "website", "versioning", "logging", "encryption", "autoclass",
                  "billing", "retention_policy", "iam_config"));

  // Clear the paths, which we already compared, and compare the proto.
  actual->mutable_update_mask()->clear_paths();
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, UpdateBucketRequestAllOptions) {
  google::storage::v2::UpdateBucketRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        bucket {
          name: "projects/_/buckets/bucket-name"
          storage_class: "NEARLINE"
          rpo: "ASYNC_TURBO"
          acl { entity: "allUsers" role: "READER" }
          default_object_acl { entity: "user:test@example.com" role: "WRITER" }
          lifecycle {
            rule {
              action { type: "Delete" }
              condition {
                age_days: 90
                created_before { year: 2022 month: 2 day: 2 }
                is_live: true
                num_newer_versions: 7
                matches_storage_class: "STANDARD"
                days_since_custom_time: 42
                days_since_noncurrent_time: 84
                noncurrent_time_before { year: 2022 month: 2 day: 15 }
              }
            }
          }
          cors {
            origin: "test-origin-0"
            origin: "test-origin-1"
            method: "GET"
            method: "PUT"
            response_header: "test-header-0"
            response_header: "test-header-1"
            max_age_seconds: 1800
          }
          cors { origin: "test-origin-0" origin: "test-origin-1" }
          cors { method: "GET" method: "PUT" }
          cors {
            response_header: "test-header-0"
            response_header: "test-header-1"
          }
          cors { max_age_seconds: 1800 }
          default_event_based_hold: true
          labels { key: "key0" value: "value0" }
          website { main_page_suffix: "index.html" not_found_page: "404.html" }
          versioning { enabled: true }
          logging {
            log_bucket: "test-log-bucket"
            log_object_prefix: "test-log-prefix"
          }
          encryption { default_kms_key: "test-only-kms-key" }
          autoclass { enabled: true }
          billing { requester_pays: true }
          retention_policy { retention_period: 123000 }
          iam_config {
            uniform_bucket_level_access { enabled: true }
            public_access_prevention: "enforced"
          }
        }
        predefined_acl: "projectPrivate"
        predefined_default_object_acl: "projectPrivate"
        if_metageneration_match: 3
        if_metageneration_not_match: 4
        update_mask {}
      )pb",
      &expected));

  storage::internal::UpdateBucketRequest req(
      storage::BucketMetadata{}
          .set_name("bucket-name")
          .set_storage_class("NEARLINE")
          .set_rpo(storage::RpoAsyncTurbo())
          .set_acl({storage::BucketAccessControl{}
                        .set_entity("allUsers")
                        .set_role("READER")})
          .set_default_acl({storage::ObjectAccessControl{}
                                .set_entity("user:test@example.com")
                                .set_role("WRITER")})
          .set_lifecycle(storage::BucketLifecycle{{storage::LifecycleRule(
              storage::LifecycleRule::ConditionConjunction(
                  storage::LifecycleRule::MaxAge(90),
                  storage::LifecycleRule::CreatedBefore(
                      absl::CivilDay(2022, 2, 2)),
                  storage::LifecycleRule::IsLive(true),
                  storage::LifecycleRule::NumNewerVersions(7),
                  storage::LifecycleRule::MatchesStorageClassStandard(),
                  storage::LifecycleRule::DaysSinceCustomTime(42),
                  storage::LifecycleRule::DaysSinceNoncurrentTime(84),
                  storage::LifecycleRule::NoncurrentTimeBefore(
                      absl::CivilDay(2022, 2, 15))),
              storage::LifecycleRule::Delete())}})
          .set_cors({
              storage::CorsEntry{
                  /*.max_age_seconds=*/static_cast<std::uint64_t>(1800),
                  /*.method=*/{"GET", "PUT"},
                  /*.origin=*/{"test-origin-0", "test-origin-1"},
                  /*.response_header=*/{"test-header-0", "test-header-1"}},
              storage::CorsEntry{/*.max_age_seconds=*/absl::nullopt,
                                 /*.method=*/{},
                                 /*.origin=*/{"test-origin-0", "test-origin-1"},
                                 /*.response_header=*/{}},
              storage::CorsEntry{/*.max_age_seconds=*/absl::nullopt,
                                 /*.method=*/{"GET", "PUT"},
                                 /*.origin=*/{},
                                 /*.response_header=*/{}},
              storage::CorsEntry{
                  /*.max_age_seconds=*/absl::nullopt,
                  /*.method=*/{},
                  /*.origin=*/{},
                  /*.response_header=*/{"test-header-0", "test-header-1"}},
              storage::CorsEntry{
                  /*.max_age_seconds=*/static_cast<std::uint64_t>(1800),
                  /*.method=*/{},
                  /*.origin=*/{},
                  /*.response_header=*/{}},
          })
          .set_default_event_based_hold(true)
          .upsert_label("key0", "value0")
          .set_website(storage::BucketWebsite{"index.html", "404.html"})
          .set_versioning(storage::BucketVersioning{true})
          .set_logging(
              storage::BucketLogging{"test-log-bucket", "test-log-prefix"})
          .set_encryption(storage::BucketEncryption{
              /*.default_kms_key=*/"test-only-kms-key"})
          .set_autoclass(storage::BucketAutoclass{true})
          .set_billing(storage::BucketBilling{/*.requester_pays=*/true})
          .set_retention_policy(std::chrono::seconds(123000))
          .set_iam_configuration(storage::BucketIamConfiguration{
              /*.uniform_bucket_level_access=*/storage::
                  UniformBucketLevelAccess{true, {}},
              /*.public_access_prevention=*/storage::
                  PublicAccessPreventionEnforced()}));
  req.set_multiple_options(
      storage::IfMetagenerationMatch(3), storage::IfMetagenerationNotMatch(4),
      storage::PredefinedAcl("projectPrivate"),
      storage::PredefinedDefaultObjectAcl("projectPrivate"),
      storage::Projection("full"), storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"));

  auto actual = ToProto(req);
  // First check the paths, we do not care about their order, so checking them
  // with IsProtoEqual does not work.
  EXPECT_THAT(actual.update_mask().paths(),
              UnorderedElementsAre(
                  "storage_class", "rpo", "acl", "default_object_acl",
                  "lifecycle", "cors", "default_event_based_hold", "labels",
                  "website", "versioning", "logging", "encryption", "autoclass",
                  "billing", "retention_policy", "iam_config"));

  // Clear the paths, which we already compared, and test the rest
  actual.mutable_update_mask()->clear_paths();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

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
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
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
        common_request_params: { user_project: "test-user-project" }
      )pb",
      &expected));

  DeleteBucketRequest req("test-bucket");
  req.set_multiple_options(
      IfMetagenerationMatch(1), IfMetagenerationNotMatch(2),
      UserProject("test-user-project"), QuotaUser("test-quota-user"),
      UserIp("test-user-ip"));

  auto const actual = GrpcBucketRequestParser::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, GetBucketMetadataAllOptions) {
  v2::GetBucketRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "projects/_/buckets/test-bucket"
        if_metageneration_match: 1
        if_metageneration_not_match: 2
        common_request_params: { user_project: "test-user-project" }
        read_mask { paths: "*" }
      )pb",
      &expected));

  GetBucketMetadataRequest req("test-bucket");
  req.set_multiple_options(
      IfMetagenerationMatch(1), IfMetagenerationNotMatch(2), Projection("full"),
      UserProject("test-user-project"), QuotaUser("test-quota-user"),
      UserIp("test-user-ip"));

  auto const actual = GrpcBucketRequestParser::ToProto(req);
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
            log_bucket: "test-log-bucket"
            log_object_prefix: "test-log-object-prefix"
          }
          versioning { enabled: true }
          website { main_page_suffix: "index.html" not_found_page: "404.html" }
        }
        predefined_acl: BUCKET_ACL_PROJECT_PRIVATE
        predefined_default_object_acl: OBJECT_ACL_PRIVATE
      )pb",
      &expected));

  CreateBucketRequest req(
      "test-project",
      BucketMetadata{}
          .set_name("test-bucket")
          .set_storage_class("NEARLINE")
          .set_location("us-central1")
          .set_billing(BucketBilling(true))
          .set_rpo(RpoAsyncTurbo())
          .set_versioning(BucketVersioning(true))
          .set_website(BucketWebsite{"index.html", "404.html"})
          .upsert_label("k0", "v0")
          .set_default_event_based_hold(true)
          .set_cors({CorsEntry{
              /*.max_age_seconds=*/absl::make_optional(std::uint64_t(1800)),
              /*.method=*/{"GET", "PUT"},
              /*.origin=*/{"test-origin-0", "test-origin-1"},
              /*.response_header=*/{"test-header-0", "test-header-1"},
          }})
          .set_acl(
              {BucketAccessControl().set_entity("allUsers").set_role("READER")})
          .set_default_acl({ObjectAccessControl()
                                .set_entity("user:test-user@example.com")
                                .set_role("READER")})
          .set_lifecycle(BucketLifecycle{{LifecycleRule(
              LifecycleRule::ConditionConjunction(
                  LifecycleRule::MaxAge(90), LifecycleRule::IsLive(false),
                  LifecycleRule::MatchesStorageClassNearline()),
              LifecycleRule::Delete())}})
          .set_logging(
              BucketLogging{/*.log_bucket=*/"test-log-bucket",
                            /*.log_object_prefix=*/"test-log-object-prefix"}));
  req.set_multiple_options(
      PredefinedAcl::ProjectPrivate(), PredefinedDefaultObjectAcl::Private(),
      Projection("full"), UserProject("test-user-project"),
      QuotaUser("test-quota-user"), UserIp("test-user-ip"));

  auto const actual = GrpcBucketRequestParser::ToProto(req);
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
        common_request_params: { user_project: "test-user-project" }
      )pb",
      &expected));

  ListBucketsRequest req("test-project");
  req.set_page_token("test-token");
  req.set_multiple_options(MaxResults(123), Prefix("test-prefix"),
                           Projection("full"), UserProject("test-user-project"),
                           QuotaUser("test-quota-user"),
                           UserIp("test-user-ip"));

  auto const actual = GrpcBucketRequestParser::ToProto(req);
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

  auto const actual = GrpcBucketRequestParser::FromProto(input);
  EXPECT_EQ(actual.next_page_token, "test-token");
  std::vector<std::string> names;
  std::transform(actual.items.begin(), actual.items.end(),
                 std::back_inserter(names),
                 [](BucketMetadata const& b) { return b.name(); });
  EXPECT_THAT(names, ElementsAre("test-bucket-1", "test-bucket-2"));
}

TEST(GrpcBucketRequestParser, LockBucketRetentionPolicyRequestAllOptions) {
  google::storage::v2::LockBucketRetentionPolicyRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        if_metageneration_match: 7
        common_request_params: { user_project: "test-user-project" }
      )pb",
      &expected));

  LockBucketRetentionPolicyRequest req("test-bucket", /*metageneration=*/7);
  req.set_multiple_options(UserProject("test-user-project"),
                           QuotaUser("test-quota-user"),
                           UserIp("test-user-ip"));

  auto const actual = GrpcBucketRequestParser::ToProto(req);
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

  GetBucketIamPolicyRequest req("test-bucket");
  req.set_multiple_options(
      RequestedPolicyVersion(3), UserProject("test-user-project"),
      QuotaUser("test-quota-user"), UserIp("test-user-ip"));
  auto const actual = GrpcBucketRequestParser::ToProto(req);
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

  auto const actual = GrpcBucketRequestParser::FromProto(input);
  EXPECT_EQ(7, actual.version());
  EXPECT_EQ("test-etag", actual.etag());
  NativeIamBinding b0(
      "role/test.only", {"test-1", "test-2"},
      {"test-expression", "test-title", "test-description", "test-location"});
  NativeIamBinding b1("role/another.test.only", {"test-3"});

  auto match_expr = [](NativeExpression const& e) {
    return AllOf(Property(&NativeExpression::expression, e.expression()),
                 Property(&NativeExpression::title, e.title()),
                 Property(&NativeExpression::description, e.description()),
                 Property(&NativeExpression::location, e.location()));
  };
  auto match_binding = [&](NativeIamBinding const& b) {
    return AllOf(
        Property(&NativeIamBinding::role, b.role()),
        Property(&NativeIamBinding::members, ElementsAreArray(b.members())),
        Property(&NativeIamBinding::has_condition, b.has_condition()));
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

  NativeIamBinding b0(
      "role/test.only", {"test-1", "test-2"},
      {"test-expression", "test-title", "test-description", "test-location"});
  NativeIamBinding b1("role/another.test.only", {"test-3"});
  NativeIamPolicy policy({b0, b1}, "test-etag", 3);

  SetNativeBucketIamPolicyRequest req("test-bucket", policy);
  req.set_multiple_options(UserProject("test-user-project"),
                           QuotaUser("test-quota-user"),
                           UserIp("test-user-ip"));
  auto const actual = GrpcBucketRequestParser::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

// TODO(#5929) - remove this test and `.inc` includes
#include "google/cloud/internal/disable_deprecation_warnings.inc"

TEST(GrpcBucketRequestParser, SetBucketIamPolicyRequest) {
  google::iam::v1::SetIamPolicyRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        resource: "projects/_/buckets/test-bucket"
        policy {
          version: 1
          bindings { role: "role/another.test.only" members: "test-3" }
          bindings {
            role: "role/test.only"
            members: "test-1"
            members: "test-2"
          }
          etag: "test-etag"
        }
      )pb",
      &expected));

  IamBinding b0("role/test.only", {"test-1", "test-2"});
  IamBinding b1("role/another.test.only", {"test-3"});
  // google::cloud::IamPolicy sorts the bindings alphabetically. This test is
  // a little too aware of this fact. Since this feature is deprecated and
  // (probably) going away soon, we avoid a more sophisticated test.
  IamPolicy policy{1, IamBindings({b0, b1}), "test-etag"};

  SetBucketIamPolicyRequest req("test-bucket", policy);
  req.set_multiple_options(UserProject("test-user-project"),
                           QuotaUser("test-quota-user"),
                           UserIp("test-user-ip"));
  auto const actual = GrpcBucketRequestParser::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

#include "google/cloud/internal/diagnostics_pop.inc"

TEST(GrpcBucketRequestParser, TestBucketIamPermissionsRequest) {
  google::iam::v1::TestIamPermissionsRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        resource: "projects/_/buckets/test-bucket"
        permissions: "test-only.permission.1"
        permissions: "test-only.permission.2"
      )pb",
      &expected));

  TestBucketIamPermissionsRequest req(
      "test-bucket", {"test-only.permission.1", "test-only.permission.2"});
  req.set_multiple_options(UserProject("test-user-project"),
                           QuotaUser("test-quota-user"),
                           UserIp("test-user-ip"));
  auto const actual = GrpcBucketRequestParser::ToProto(req);
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

  auto const actual = GrpcBucketRequestParser::FromProto(input);
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
          billing { requester_pays: true }
          retention_policy { retention_period: 123000 }
          iam_config {
            uniform_bucket_level_access { enabled: true }
            public_access_prevention: ENFORCED
          }
        }
        predefined_acl: BUCKET_ACL_PROJECT_PRIVATE
        predefined_default_object_acl: OBJECT_ACL_PROJECT_PRIVATE
        if_metageneration_match: 3
        if_metageneration_not_match: 4
        common_request_params: { user_project: "test-user-project" }
        update_mask {}
      )pb",
      &expected));

  PatchBucketRequest req(
      "bucket-name",
      BucketMetadataPatchBuilder{}
          .SetStorageClass("NEARLINE")
          .SetRpo(RpoAsyncTurbo())
          .SetAcl(
              {BucketAccessControl{}.set_entity("allUsers").set_role("READER")})
          .SetDefaultAcl({ObjectAccessControl{}
                              .set_entity("user:test@example.com")
                              .set_role("WRITER")})
          .SetLifecycle(BucketLifecycle{{LifecycleRule(
              LifecycleRule::ConditionConjunction(
                  LifecycleRule::MaxAge(90),
                  LifecycleRule::CreatedBefore(absl::CivilDay(2022, 2, 2)),
                  LifecycleRule::IsLive(true),
                  LifecycleRule::NumNewerVersions(7),
                  LifecycleRule::MatchesStorageClassStandard(),
                  LifecycleRule::DaysSinceCustomTime(42),
                  LifecycleRule::DaysSinceNoncurrentTime(84),
                  LifecycleRule::NoncurrentTimeBefore(
                      absl::CivilDay(2022, 2, 15))),
              LifecycleRule::Delete())}})
          .SetCors({
              CorsEntry{
                  /*.max_age_seconds=*/absl::make_optional(std::uint64_t(1800)),
                  /*.method=*/{"GET", "PUT"},
                  /*.origin=*/{"test-origin-0", "test-origin-1"},
                  /*.response_header=*/{"test-header-0", "test-header-1"}},
              CorsEntry{/*.max_age_seconds=*/absl::nullopt,
                        /*.method=*/{},
                        /*.origin=*/{"test-origin-0", "test-origin-1"},
                        /*.response_header=*/{}},
              CorsEntry{/*.max_age_seconds=*/absl::nullopt,
                        /*.method=*/{"GET", "PUT"},
                        /*.origin=*/{},
                        /*.response_header=*/{}},
              CorsEntry{
                  /*.max_age_seconds=*/absl::nullopt,
                  /*.method=*/{},
                  /*.origin=*/{},
                  /*.response_header=*/{"test-header-0", "test-header-1"}},
              CorsEntry{
                  /*.max_age_seconds=*/absl::make_optional(std::uint64_t(1800)),
                  /*.method=*/{},
                  /*.origin=*/{},
                  /*.response_header=*/{}},
          })
          .SetDefaultEventBasedHold(true)
          .SetLabel("key0", "value0")
          .SetWebsite(BucketWebsite{"index.html", "404.html"})
          .SetVersioning(BucketVersioning{true})
          .SetLogging(BucketLogging{"test-log-bucket", "test-log-prefix"})
          .SetEncryption(
              BucketEncryption{/*.default_kms_key=*/"test-only-kms-key"})
          .SetBilling(BucketBilling{/*.requester_pays=*/true})
          .SetRetentionPolicy(std::chrono::seconds(123000))
          .SetIamConfiguration(BucketIamConfiguration{
              /*.uniform_bucket_level_access=*/UniformBucketLevelAccess{true,
                                                                        {}},
              /*.public_access_prevention=*/PublicAccessPreventionEnforced()}));
  req.set_multiple_options(
      IfMetagenerationMatch(3), IfMetagenerationNotMatch(4),
      PredefinedAcl("projectPrivate"),
      PredefinedDefaultObjectAcl("projectPrivate"), Projection("full"),
      UserProject("test-user-project"), QuotaUser("test-quota-user"),
      UserIp("test-user-ip"));

  auto actual = GrpcBucketRequestParser::ToProto(req);
  ASSERT_STATUS_OK(actual);
  // First check the paths, we do not care about their order, so checking them
  // with IsProtoEqual does not work.
  EXPECT_THAT(actual->update_mask().paths(),
              UnorderedElementsAre(
                  "storage_class", "rpo", "acl", "default_object_acl",
                  "lifecycle", "cors", "default_event_based_hold", "labels",
                  "website", "versioning", "logging", "encryption", "billing",
                  "retention_policy", "iam_config"));

  // Clear the paths, which we already compared, and test the rest
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
          billing { requester_pays: true }
          retention_policy { retention_period: 123000 }
          iam_config {
            uniform_bucket_level_access { enabled: true }
            public_access_prevention: ENFORCED
          }
        }
        predefined_acl: BUCKET_ACL_PROJECT_PRIVATE
        predefined_default_object_acl: OBJECT_ACL_PROJECT_PRIVATE
        if_metageneration_match: 3
        if_metageneration_not_match: 4
        common_request_params: { user_project: "test-user-project" }
        update_mask {}
      )pb",
      &expected));

  UpdateBucketRequest req(
      BucketMetadata{}
          .set_name("bucket-name")
          .set_storage_class("NEARLINE")
          .set_rpo(RpoAsyncTurbo())
          .set_acl(
              {BucketAccessControl{}.set_entity("allUsers").set_role("READER")})
          .set_default_acl({ObjectAccessControl{}
                                .set_entity("user:test@example.com")
                                .set_role("WRITER")})
          .set_lifecycle(BucketLifecycle{{LifecycleRule(
              LifecycleRule::ConditionConjunction(
                  LifecycleRule::MaxAge(90),
                  LifecycleRule::CreatedBefore(absl::CivilDay(2022, 2, 2)),
                  LifecycleRule::IsLive(true),
                  LifecycleRule::NumNewerVersions(7),
                  LifecycleRule::MatchesStorageClassStandard(),
                  LifecycleRule::DaysSinceCustomTime(42),
                  LifecycleRule::DaysSinceNoncurrentTime(84),
                  LifecycleRule::NoncurrentTimeBefore(
                      absl::CivilDay(2022, 2, 15))),
              LifecycleRule::Delete())}})
          .set_cors({
              CorsEntry{
                  /*.max_age_seconds=*/absl::make_optional(std::uint64_t(1800)),
                  /*.method=*/{"GET", "PUT"},
                  /*.origin=*/{"test-origin-0", "test-origin-1"},
                  /*.response_header=*/{"test-header-0", "test-header-1"}},
              CorsEntry{/*.max_age_seconds=*/absl::nullopt,
                        /*.method=*/{},
                        /*.origin=*/{"test-origin-0", "test-origin-1"},
                        /*.response_header=*/{}},
              CorsEntry{/*.max_age_seconds=*/absl::nullopt,
                        /*.method=*/{"GET", "PUT"},
                        /*.origin=*/{},
                        /*.response_header=*/{}},
              CorsEntry{
                  /*.max_age_seconds=*/absl::nullopt,
                  /*.method=*/{},
                  /*.origin=*/{},
                  /*.response_header=*/{"test-header-0", "test-header-1"}},
              CorsEntry{
                  /*.max_age_seconds=*/absl::make_optional(std::uint64_t(1800)),
                  /*.method=*/{},
                  /*.origin=*/{},
                  /*.response_header=*/{}},
          })
          .set_default_event_based_hold(true)
          .upsert_label("key0", "value0")
          .set_website(BucketWebsite{"index.html", "404.html"})
          .set_versioning(BucketVersioning{true})
          .set_logging(BucketLogging{"test-log-bucket", "test-log-prefix"})
          .set_encryption(
              BucketEncryption{/*.default_kms_key=*/"test-only-kms-key"})
          .set_billing(BucketBilling{/*.requester_pays=*/true})
          .set_retention_policy(std::chrono::seconds(123000))
          .set_iam_configuration(BucketIamConfiguration{
              /*.uniform_bucket_level_access=*/UniformBucketLevelAccess{true,
                                                                        {}},
              /*.public_access_prevention=*/PublicAccessPreventionEnforced()}));
  req.set_multiple_options(
      IfMetagenerationMatch(3), IfMetagenerationNotMatch(4),
      PredefinedAcl("projectPrivate"),
      PredefinedDefaultObjectAcl("projectPrivate"), Projection("full"),
      UserProject("test-user-project"), QuotaUser("test-quota-user"),
      UserIp("test-user-ip"));

  auto actual = GrpcBucketRequestParser::ToProto(req);
  // First check the paths, we do not care about their order, so checking them
  // with IsProtoEqual does not work.
  EXPECT_THAT(actual.update_mask().paths(),
              UnorderedElementsAre(
                  "storage_class", "rpo", "acl", "default_object_acl",
                  "lifecycle", "cors", "default_event_based_hold", "labels",
                  "website", "versioning", "logging", "encryption", "billing",
                  "retention_policy", "iam_config"));

  // Clear the paths, which we already compared, and test the rest
  actual.mutable_update_mask()->clear_paths();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

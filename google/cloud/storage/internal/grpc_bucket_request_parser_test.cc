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

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/storage/internal/grpc_hmac_key_request_parser.h"
#include "google/cloud/internal/format_time_point.h"
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
using ::google::cloud::internal::FormatRfc3339;
using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Property;
using ::testing::ResultOf;

TEST(GrpcBucketRequestParser, CreateHmacKeyRequestAllOptions) {
  v2::CreateHmacKeyRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        project: "projects/test-project-id"
        service_account_email: "test-service-account-email"
        common_request_params: { user_project: "test-user-project" }
      )pb",
      &expected));

  CreateHmacKeyRequest req("test-project-id", "test-service-account-email");
  req.set_multiple_options(UserProject("test-user-project"));

  auto const actual = GrpcHmacKeyRequestParser::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, CreateHmacKeyResponseFull) {
  v2::CreateHmacKeyResponse input;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        metadata {
          id: "test-id"
          access_id: "test-access-id"
          project: "projects/test-project-id"
          service_account_email: "test-service-account-email"
          state: "ACTIVE"
          create_time { seconds: 1652099696 nanos: 789000000 }
          update_time { seconds: 1652186096 nanos: 789000000 }
        }
        secret_key_bytes: "0123456789"
      )pb",
      &input));

  auto actual = GrpcHmacKeyRequestParser::FromProto(input);
  // Obtained the magic base64 string using:
  //     /bin/echo  -n "0123456789" | openssl base64 -e
  EXPECT_EQ(actual.secret, "MDEyMzQ1Njc4OQ==");
  auto const& metadata = actual.metadata;
  EXPECT_EQ(metadata.id(), "test-id");
  EXPECT_EQ(metadata.access_id(), "test-access-id");
  EXPECT_EQ(metadata.state(), "ACTIVE");
  // To get the dates in RFC-3339 format I used:
  //     date --rfc-3339=seconds --date=@1652099696  # Create
  //     date --rfc-3339=seconds --date=@1652186096  # Update
  EXPECT_EQ(FormatRfc3339(metadata.time_created()), "2022-05-09T12:34:56.789Z");
  EXPECT_EQ(FormatRfc3339(metadata.updated()), "2022-05-10T12:34:56.789Z");
}

TEST(GrpcBucketRequestParser, DeleteHmacKeyRequestAllOptions) {
  v2::DeleteHmacKeyRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        access_id: "test-access-id"
        project: "projects/test-project-id"
        common_request_params: { user_project: "test-user-project" }
      )pb",
      &expected));

  DeleteHmacKeyRequest req("test-project-id", "test-access-id");
  req.set_multiple_options(UserProject("test-user-project"));

  auto const actual = GrpcHmacKeyRequestParser::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, GetHmacKeyRequestAllOptions) {
  v2::GetHmacKeyRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        access_id: "test-access-id"
        project: "projects/test-project-id"
        common_request_params: { user_project: "test-user-project" }
      )pb",
      &expected));

  GetHmacKeyRequest req("test-project-id", "test-access-id");
  req.set_multiple_options(UserProject("test-user-project"));

  auto const actual = GrpcHmacKeyRequestParser::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, ListHmacKeysRequestAllOptions) {
  v2::ListHmacKeysRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        project: "projects/test-project-id"
        page_size: 42
        page_token: "test-page-token"
        service_account_email: "test-service-account-email"
        show_deleted_keys: true
        common_request_params: { user_project: "test-user-project" }
      )pb",
      &expected));

  ListHmacKeysRequest req("test-project-id");
  req.set_page_token("test-page-token");
  req.set_multiple_options(Deleted(true), MaxResults(42),
                           ServiceAccountFilter("test-service-account-email"),
                           UserProject("test-user-project"));

  auto const actual = GrpcHmacKeyRequestParser::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketRequestParser, ListHmacKeysResponseFull) {
  v2::ListHmacKeysResponse input;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        hmac_keys {
          id: "test-id-1"
          access_id: "test-access-id-1"
          project: "projects/test-project-id"
          service_account_email: "test-service-account-email"
          state: "ACTIVE"
          create_time { seconds: 1652099696 nanos: 789000000 }
          update_time { seconds: 1652186096 nanos: 789000000 }
        }
        hmac_keys {
          id: "test-id-2"
          access_id: "test-access-id-2"
          project: "projects/test-project-id"
          service_account_email: "test-service-account-email"
          state: "ACTIVE"
          create_time { seconds: 1652099696 nanos: 789000000 }
          update_time { seconds: 1652186096 nanos: 789000000 }
        }
        next_page_token: "test-next-page-token"
      )pb",
      &input));

  auto actual = GrpcHmacKeyRequestParser::FromProto(input);
  // Obtained the magic base64 string using:
  //     /bin/echo -n "0123456789" | openssl base64 -e
  EXPECT_EQ(actual.next_page_token, "test-next-page-token");

  auto make_matcher = [](std::string const& expected_id,
                         std::string const& expected_access_id) {
    // To get the dates in RFC-3339 format I used:
    //     date --rfc-3339=seconds --date=@1652099696  # Create
    //     date --rfc-3339=seconds --date=@1652186096  # Update
    auto format_time_created = [](HmacKeyMetadata const& metadata) {
      return FormatRfc3339(metadata.time_created());
    };
    auto format_updated = [](HmacKeyMetadata const& metadata) {
      return FormatRfc3339(metadata.updated());
    };
    return AllOf(Property(&HmacKeyMetadata::id, expected_id),
                 Property(&HmacKeyMetadata::access_id, expected_access_id),
                 Property(&HmacKeyMetadata::project_id, "test-project-id"),
                 Property(&HmacKeyMetadata::service_account_email,
                          "test-service-account-email"),
                 Property(&HmacKeyMetadata::state, "ACTIVE"),
                 ResultOf(format_time_created, "2022-05-09T12:34:56.789Z"),
                 ResultOf(format_updated, "2022-05-10T12:34:56.789Z"));
  };
  EXPECT_THAT(actual.items,
              ElementsAre(make_matcher("test-id-1", "test-access-id-1"),
                          make_matcher("test-id-2", "test-access-id-2")));
}

TEST(GrpcBucketRequestParser, UpdateHmacKeyRequestAllOptions) {
  v2::UpdateHmacKeyRequest expected;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        hmac_key {
          access_id: "test-access-id"
          project: "projects/test-project-id"
          state: "INACTIVE"
        }
        common_request_params: { user_project: "test-user-project" }
        update_mask { paths: [ 'state' ] }
      )pb",
      &expected));

  UpdateHmacKeyRequest req("test-project-id", "test-access-id",
                           HmacKeyMetadata()
                               .set_state(HmacKeyMetadata::state_inactive())
                               .set_etag("test-only-etag"));
  req.set_multiple_options(UserProject("test-user-project"));

  auto const actual = GrpcHmacKeyRequestParser::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

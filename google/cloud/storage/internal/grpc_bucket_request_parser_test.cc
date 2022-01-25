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

namespace storage_proto = ::google::storage::v2;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;

TEST(GrpcBucketRequestParser, PredefinedAclObject) {
  EXPECT_EQ(storage_proto::BUCKET_ACL_AUTHENTICATED_READ,
            GrpcBucketRequestParser::ToProtoBucket(
                PredefinedAcl::AuthenticatedRead()));
  EXPECT_EQ(storage_proto::BUCKET_ACL_PRIVATE,
            GrpcBucketRequestParser::ToProtoBucket(PredefinedAcl::Private()));
  EXPECT_EQ(
      storage_proto::BUCKET_ACL_PROJECT_PRIVATE,
      GrpcBucketRequestParser::ToProtoBucket(PredefinedAcl::ProjectPrivate()));
  EXPECT_EQ(
      storage_proto::BUCKET_ACL_PUBLIC_READ,
      GrpcBucketRequestParser::ToProtoBucket(PredefinedAcl::PublicRead()));
  EXPECT_EQ(
      storage_proto::BUCKET_ACL_PUBLIC_READ_WRITE,
      GrpcBucketRequestParser::ToProtoBucket(PredefinedAcl::PublicReadWrite()));
  EXPECT_EQ(storage_proto::PREDEFINED_BUCKET_ACL_UNSPECIFIED,
            GrpcBucketRequestParser::ToProtoBucket(
                PredefinedAcl::BucketOwnerFullControl()));
}

TEST(GrpcBucketRequestParser, GetBucketMetadataAllOptions) {
  google::storage::v2::GetBucketRequest expected;
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

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

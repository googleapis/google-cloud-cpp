// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/bucket_metadata_parser.h"
#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

namespace storage_proto = ::google::storage::v1;
using ::google::cloud::testing_util::IsProtoEqual;

TEST(GrpcClientBucketRequest, CreateBucketRequestSimple) {
  storage_proto::InsertBucketRequest expected;
  auto constexpr kText = R"pb(
    project: "test-project-id"
    bucket: {
      name: "test-bucket"
      time_created {}
      updated {}
    }
  )pb";
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &expected));

  auto metadata = BucketMetadataParser::FromString(R"""({
    "name": "test-bucket"
})""");
  EXPECT_STATUS_OK(metadata);
  CreateBucketRequest request("test-project-id", *std::move(metadata));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, CreateBucketRequestAllOptions) {
  storage_proto::InsertBucketRequest expected;
  auto constexpr kText = R"pb(
    predefined_acl: BUCKET_ACL_PRIVATE
    predefined_default_object_acl: OBJECT_ACL_PRIVATE
    project: "test-project-id"
    projection: FULL
    bucket: {
      name: "test-bucket"
      time_created {}
      updated {}
    }
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
  )pb";
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &expected));

  auto metadata = BucketMetadataParser::FromString(R"""({
    "name": "test-bucket"
})""");
  EXPECT_STATUS_OK(metadata);
  CreateBucketRequest request("test-project-id", *std::move(metadata));
  request.set_multiple_options(
      PredefinedAcl::Private(), PredefinedDefaultObjectAcl::Private(),
      Projection::Full(), UserProject("test-user-project"),
      QuotaUser("test-quota-user"), UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, ListBucketsRequestSimple) {
  storage_proto::ListBucketsRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    project: "test-project-id"
)""",
                                                            &expected));

  ListBucketsRequest request("test-project-id");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, ListBucketsRequestAllFields) {
  storage_proto::ListBucketsRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    max_results: 42
    page_token: "test-token"
    prefix: "test-prefix/"
    project: "test-project-id"
    projection: FULL
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  ListBucketsRequest request("test-project-id");
  request.set_page_token("test-token");
  request.set_multiple_options(
      MaxResults(42), Prefix("test-prefix/"), Projection::Full(),
      UserProject("test-user-project"), QuotaUser("test-quota-user"),
      UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, GetBucketRequestSimple) {
  storage_proto::GetBucketRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
)""",
                                                            &expected));

  GetBucketMetadataRequest request("test-bucket-name");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, GetBucketRequestAllFields) {
  storage_proto::GetBucketRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    if_metageneration_match: { value: 42 }
    if_metageneration_not_match: { value: 7 }
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  GetBucketMetadataRequest request("test-bucket-name");
  request.set_multiple_options(
      IfMetagenerationMatch(42), IfMetagenerationNotMatch(7),
      UserProject("test-user-project"), QuotaUser("test-quota-user"),
      UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, DeleteBucketRequestSimple) {
  storage_proto::DeleteBucketRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
)""",
                                                            &expected));

  DeleteBucketRequest request("test-bucket-name");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, DeleteBucketRequestAllFields) {
  storage_proto::DeleteBucketRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    if_metageneration_match: { value: 42 }
    if_metageneration_not_match: { value: 7 }
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  DeleteBucketRequest request("test-bucket-name");
  request.set_multiple_options(
      IfMetagenerationMatch(42), IfMetagenerationNotMatch(7),
      UserProject("test-user-project"), QuotaUser("test-quota-user"),
      UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

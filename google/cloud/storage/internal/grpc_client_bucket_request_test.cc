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
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
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

TEST(GrpcClientBucketRequest, UpdateBucketRequestSimple) {
  storage_proto::UpdateBucketRequest expected;
  auto constexpr kText = R"pb(
    bucket: "test-bucket-name"
    metadata: {
      name: "test-bucket-name"
      time_created {}
      updated {}
    }
  )pb";
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &expected));

  auto metadata = BucketMetadataParser::FromString(R"""({
    "name": "test-bucket-name"
})""");
  EXPECT_STATUS_OK(metadata);
  UpdateBucketRequest request(*std::move(metadata));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, UpdateBucketRequestAllFields) {
  storage_proto::UpdateBucketRequest expected;
  auto constexpr kText = R"pb(
    bucket: "test-bucket-name"
    if_metageneration_match: { value: 42 }
    if_metageneration_not_match: { value: 7 }
    predefined_acl: BUCKET_ACL_PRIVATE
    predefined_default_object_acl: OBJECT_ACL_PRIVATE
    metadata: {
      name: "test-bucket-name"
      time_created {}
      updated {}
    }
    projection: FULL
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
  )pb";
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &expected));

  auto metadata = BucketMetadataParser::FromString(R"""({
    "name": "test-bucket-name"
})""");
  EXPECT_STATUS_OK(metadata);
  UpdateBucketRequest request(*std::move(metadata));
  request.set_multiple_options(
      IfMetagenerationMatch(42), IfMetagenerationNotMatch(7),
      PredefinedAcl::Private(), PredefinedDefaultObjectAcl::Private(),
      Projection::Full(), UserProject("test-user-project"),
      QuotaUser("test-quota-user"), UserIp("test-user-ip"));

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

TEST(GrpcClientBucketRequest, CreateBucketAclRequestSimple) {
  storage_proto::InsertBucketAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    bucket_access_control: {
      role: "READER"
      entity: "user-testuser"
    }
)""",
                                                            &expected));

  CreateBucketAclRequest request("test-bucket-name", "user-testuser", "READER");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, CreateBucketAclRequestAllFields) {
  storage_proto::InsertBucketAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    bucket_access_control: {
      role: "READER"
      entity: "user-testuser"
    }
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  CreateBucketAclRequest request("test-bucket-name", "user-testuser", "READER");
  request.set_multiple_options(UserProject("test-user-project"),
                               QuotaUser("test-quota-user"),
                               UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, ListBucketAclRequestSimple) {
  storage_proto::ListBucketAccessControlsRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
)""",
                                                            &expected));

  ListBucketAclRequest request("test-bucket-name");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, ListBucketAclRequestAllFields) {
  storage_proto::ListBucketAccessControlsRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  ListBucketAclRequest request("test-bucket-name");
  request.set_multiple_options(UserProject("test-user-project"),
                               QuotaUser("test-quota-user"),
                               UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, GetBucketAclRequestSimple) {
  storage_proto::GetBucketAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    entity: "user-testuser"
)""",
                                                            &expected));

  GetBucketAclRequest request("test-bucket-name", "user-testuser");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, GetBucketAclRequestAllFields) {
  storage_proto::GetBucketAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    entity: "user-testuser"
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  GetBucketAclRequest request("test-bucket-name", "user-testuser");
  request.set_multiple_options(UserProject("test-user-project"),
                               QuotaUser("test-quota-user"),
                               UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, UpdateBucketAclRequestSimple) {
  storage_proto::UpdateBucketAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    entity: "user-testuser"
    bucket_access_control: {
      role: "READER"
    }
)""",
                                                            &expected));

  UpdateBucketAclRequest request("test-bucket-name", "user-testuser", "READER");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, UpdateBucketAclRequestAllFields) {
  storage_proto::UpdateBucketAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    entity: "user-testuser"
    bucket_access_control: {
      role: "READER"
    }
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  UpdateBucketAclRequest request("test-bucket-name", "user-testuser", "READER");
  request.set_multiple_options(UserProject("test-user-project"),
                               QuotaUser("test-quota-user"),
                               UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, DeleteBucketAclRequestSimple) {
  storage_proto::DeleteBucketAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    entity: "user-testuser"
)""",
                                                            &expected));

  DeleteBucketAclRequest request("test-bucket-name", "user-testuser");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, DeleteBucketAclRequestAllFields) {
  storage_proto::DeleteBucketAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    entity: "user-testuser"
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  DeleteBucketAclRequest request("test-bucket-name", "user-testuser");
  request.set_multiple_options(UserProject("test-user-project"),
                               QuotaUser("test-quota-user"),
                               UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, CreateDefaultObjectAclRequestSimple) {
  storage_proto::InsertDefaultObjectAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    object_access_control: {
      role: "READER"
      entity: "user-testuser"
    }
)""",
                                                            &expected));

  CreateDefaultObjectAclRequest request("test-bucket-name", "user-testuser",
                                        "READER");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, CreateDefaultObjectAclRequestAllFields) {
  storage_proto::InsertDefaultObjectAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    object_access_control: {
      role: "READER"
      entity: "user-testuser"
    }
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  CreateDefaultObjectAclRequest request("test-bucket-name", "user-testuser",
                                        "READER");
  request.set_multiple_options(UserProject("test-user-project"),
                               QuotaUser("test-quota-user"),
                               UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, ListDefaultObjectAclRequestSimple) {
  storage_proto::ListDefaultObjectAccessControlsRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
)""",
                                                            &expected));

  ListDefaultObjectAclRequest request("test-bucket-name");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, ListDefaultObjectAclRequestAllFields) {
  storage_proto::ListDefaultObjectAccessControlsRequest expected;
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

  ListDefaultObjectAclRequest request("test-bucket-name");
  request.set_multiple_options(
      IfMetagenerationMatch(42), IfMetagenerationNotMatch(7),
      UserProject("test-user-project"), QuotaUser("test-quota-user"),
      UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, GetDefaultObjectAclRequestSimple) {
  storage_proto::GetDefaultObjectAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    entity: "user-testuser"
)""",
                                                            &expected));

  GetDefaultObjectAclRequest request("test-bucket-name", "user-testuser");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, GetDefaultObjectAclRequestAllFields) {
  storage_proto::GetDefaultObjectAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    entity: "user-testuser"
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  GetDefaultObjectAclRequest request("test-bucket-name", "user-testuser");
  request.set_multiple_options(UserProject("test-user-project"),
                               QuotaUser("test-quota-user"),
                               UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, UpdateDefaultObjectAclRequestSimple) {
  storage_proto::UpdateDefaultObjectAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    entity: "user-testuser"
    object_access_control: {
      role: "READER"
    }
)""",
                                                            &expected));

  UpdateDefaultObjectAclRequest request("test-bucket-name", "user-testuser",
                                        "READER");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, UpdateDefaultObjectAclRequestAllFields) {
  storage_proto::UpdateDefaultObjectAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    entity: "user-testuser"
    object_access_control: {
      role: "READER"
    }
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  UpdateDefaultObjectAclRequest request("test-bucket-name", "user-testuser",
                                        "READER");
  request.set_multiple_options(UserProject("test-user-project"),
                               QuotaUser("test-quota-user"),
                               UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, DeleteDefaultObjectAclRequestSimple) {
  storage_proto::DeleteDefaultObjectAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    entity: "user-testuser"
)""",
                                                            &expected));

  DeleteDefaultObjectAclRequest request("test-bucket-name", "user-testuser");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, DeleteDefaultObjectAclRequestAllFields) {
  storage_proto::DeleteDefaultObjectAccessControlRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    entity: "user-testuser"
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  DeleteDefaultObjectAclRequest request("test-bucket-name", "user-testuser");
  request.set_multiple_options(UserProject("test-user-project"),
                               QuotaUser("test-quota-user"),
                               UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, CreateNotificationRequestSimple) {
  storage_proto::InsertNotificationRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    notification {
      topic: "test-topic"
      event_types: "OBJECT_FINALIZE"
      event_types: "OBJECT_METADATA_UPDATE"
      custom_attributes: { key: "test-ca-1" value: "value1" }
      custom_attributes: { key: "test-ca-2" value: "value2" }
      object_name_prefix: "test-object-prefix-"
      payload_format: "JSON_API_V1"
    }
)""",
                                                            &expected));

  auto notification = NotificationMetadata()
                          .set_topic("test-topic")
                          .append_event_type("OBJECT_FINALIZE")
                          .append_event_type("OBJECT_METADATA_UPDATE")
                          .upsert_custom_attributes("test-ca-1", "value1")
                          .upsert_custom_attributes("test-ca-2", "value2")
                          .set_object_name_prefix("test-object-prefix-")
                          .set_payload_format("JSON_API_V1");
  CreateNotificationRequest request("test-bucket-name", notification);

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, CreateNotificationRequestAllFields) {
  storage_proto::InsertNotificationRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    notification {
      topic: "test-topic"
      event_types: "OBJECT_FINALIZE"
      event_types: "OBJECT_METADATA_UPDATE"
      custom_attributes: { key: "test-ca-1" value: "value1" }
      custom_attributes: { key: "test-ca-2" value: "value2" }
      object_name_prefix: "test-object-prefix-"
      payload_format: "JSON_API_V1"
    }
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  auto notification = NotificationMetadata()
                          .set_topic("test-topic")
                          .append_event_type("OBJECT_FINALIZE")
                          .append_event_type("OBJECT_METADATA_UPDATE")
                          .upsert_custom_attributes("test-ca-1", "value1")
                          .upsert_custom_attributes("test-ca-2", "value2")
                          .set_object_name_prefix("test-object-prefix-")
                          .set_payload_format("JSON_API_V1");
  CreateNotificationRequest request("test-bucket-name", notification);
  request.set_multiple_options(UserProject("test-user-project"),
                               QuotaUser("test-quota-user"),
                               UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, ListNotificationsRequestSimple) {
  storage_proto::ListNotificationsRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
)""",
                                                            &expected));

  ListNotificationsRequest request("test-bucket-name");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, ListNotificationsRequestAllFields) {
  storage_proto::ListNotificationsRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  ListNotificationsRequest request("test-bucket-name");
  request.set_multiple_options(UserProject("test-user-project"),
                               QuotaUser("test-quota-user"),
                               UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, GetNotificationRequestSimple) {
  storage_proto::GetNotificationRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    notification: "test-notification-id"
)""",
                                                            &expected));

  GetNotificationRequest request("test-bucket-name", "test-notification-id");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, GetNotificationRequestAllFields) {
  storage_proto::GetNotificationRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    notification: "test-notification-id"
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  GetNotificationRequest request("test-bucket-name", "test-notification-id");
  request.set_multiple_options(UserProject("test-user-project"),
                               QuotaUser("test-quota-user"),
                               UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, DeleteNotificationRequestSimple) {
  storage_proto::DeleteNotificationRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    notification: "test-notification-id"
)""",
                                                            &expected));

  DeleteNotificationRequest request("test-bucket-name", "test-notification-id");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, DeleteNotificationRequestAllFields) {
  storage_proto::DeleteNotificationRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    notification: "test-notification-id"
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  DeleteNotificationRequest request("test-bucket-name", "test-notification-id");
  request.set_multiple_options(UserProject("test-user-project"),
                               QuotaUser("test-quota-user"),
                               UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, GetBucketIamPolicySimple) {
  storage_proto::GetIamPolicyRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
      iam_request: {
        resource: "test-bucket-name",
        options: {
          requested_policy_version: 3
        }
      }
)""",
                                                            &expected));

  GetBucketIamPolicyRequest request("test-bucket-name");
  request.set_option(RequestedPolicyVersion(3));
  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, SetBucketIamPolicySimple) {
  storage_proto::SetIamPolicyRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    iam_request: {
      resource: "test-bucket-name"
      policy: {
        bindings: {
          role: "test-role"
          members: "user:test@example.com"
        }
        etag: "test-etag"
        version: 3
      }
    }
)""",
                                                            &expected));
  SetNativeBucketIamPolicyRequest request(
      "test-bucket-name",
      NativeIamPolicy{std::vector<NativeIamBinding>{NativeIamBinding{
                          "test-role", {"user:test@example.com"}}},
                      "test-etag", 3});
  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientBucketRequest, TestBucketIamPermissionsSimple) {
  storage_proto::TestIamPermissionsRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    iam_request: {
      resource: "test-bucket-name"
      permissions: "storage.buckets.get"
    }
)""",
                                                            &expected));

  TestBucketIamPermissionsRequest request("test-bucket-name",
                                          {"storage.buckets.get"});
  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

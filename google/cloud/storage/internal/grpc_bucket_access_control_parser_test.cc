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

#include "google/cloud/storage/internal/grpc_bucket_access_control_parser.h"
#include "google/cloud/storage/internal/bucket_access_control_parser.h"
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

TEST(GrpcBucketAccessControlParser, FromProto) {
  storage_proto::BucketAccessControl input;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
     role: "test-role"
     id: "test-id"
     entity: "test-entity"
     entity_id: "test-entity-id"
     email: "test-email"
     domain: "test-domain"
     project_team: {
       project_number: "test-project-number"
       team: "test-team"
     }
     etag: "test-etag")""",
                                                            &input));

  auto const expected = BucketAccessControlParser::FromString(R"""({
     "role": "test-role",
     "id": "test-id",
     "kind": "storage#bucketAccessControl",
     "bucket": "test-bucket",
     "entity": "test-entity",
     "entityId": "test-entity-id",
     "email": "test-email",
     "domain": "test-domain",
     "projectTeam": {
       "projectNumber": "test-project-number",
       "team": "test-team"
     },
     "etag": "test-etag"
  })""");
  ASSERT_STATUS_OK(expected);

  auto actual = GrpcBucketAccessControlParser::FromProto(input, "test-bucket");
  EXPECT_EQ(*expected, actual);
}

TEST(GrpcBucketAccessControlParser, ToProtoSimple) {
  auto acl = BucketAccessControlParser::FromString(R"""({
     "role": "test-role",
     "id": "test-id",
     "kind": "storage#bucketAccessControl",
     "bucket": "test-bucket",
     "entity": "test-entity",
     "entityId": "test-entity-id",
     "email": "test-email",
     "domain": "test-domain",
     "projectTeam": {
       "projectNumber": "test-project-number",
       "team": "test-team"
     },
     "etag": "test-etag"
  })""");
  ASSERT_STATUS_OK(acl);
  auto actual = GrpcBucketAccessControlParser::ToProto(*acl);

  storage_proto::BucketAccessControl expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
     role: "test-role"
     id: "test-id"
     entity: "test-entity"
     entity_id: "test-entity-id"
     email: "test-email"
     domain: "test-domain"
     project_team: {
       project_number: "test-project-number"
       team: "test-team"
     }
     etag: "test-etag")""",
                                                            &expected));

  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketAccessControlParser, MinimalFields) {
  BucketAccessControl acl;
  acl.set_role("test-role");
  acl.set_entity("test-entity");
  auto actual = GrpcBucketAccessControlParser::ToProto(acl);

  storage_proto::BucketAccessControl expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
     role: "test-role"
     entity: "test-entity"
     )""",
                                                            &expected));

  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcBucketAccessControlParser, Role) {
  auto const patch = BucketAccessControlPatchBuilder().set_role("test-role");
  EXPECT_EQ("test-role", GrpcBucketAccessControlParser::Role(patch));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

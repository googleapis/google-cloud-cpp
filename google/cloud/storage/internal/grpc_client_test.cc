// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/internal/notification_metadata_parser.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
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

namespace storage_proto = ::google::storage::v2;
using ::google::cloud::testing_util::IsProtoEqual;

TEST(GrpcClientFromProto, ObjectSimple) {
  storage_proto::Object input;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    acl: {
      role: "OWNER"
      entity: "user:test-user@gmail.com"
    }
    content_encoding: "test-content-encoding"
    content_disposition: "test-content-disposition"
    cache_control: "test-cache-control"
    content_language: "test-content-language"
    metageneration: 42
    delete_time: {
      seconds: 1565194924
      nanos: 123456789
    }
    content_type: "test-content-type"
    size: 123456
    create_time: {
      seconds: 1565194924
      nanos: 234567890
    }
    # These magic numbers can be obtained using `gsutil hash` and then
    # transforming the output from base64 to binary using tools like xxd(1).
    checksums {
      crc32c: 576848900
      md5_hash: "9e107d9d372bb6826bd81d3542a419d6"
    }
    component_count: 7
    update_time: {
      seconds: 1565194924
      nanos: 345678901
    }
    storage_class: "test-storage-class"
    kms_key: "test-kms-key-name"
    update_storage_class_time: {
      seconds: 1565194924
      nanos: 456789012
    }
    temporary_hold: true
    retention_expire_time: {
      seconds: 1565194924
      nanos: 567890123
    }
    metadata: { key: "test-key-1" value: "test-value-1" }
    metadata: { key: "test-key-2" value: "test-value-2" }
    event_based_hold: true
    name: "test-object-name"
    bucket: "test-bucket"
    generation: 2345
    owner: {
      entity: "test-entity"
      entity_id: "test-entity-id"
    }
    customer_encryption: {
      encryption_algorithm: "test-encryption-algorithm"
      key_sha256: "test-key-sha256"
    }
)""",
                                                            &input));

  // To get the dates in RFC-3339 format I used:
  //     date --rfc-3339=seconds --date=@1565194924
  // The magical values for the CRC32C and MD5 are documented in
  //     hash_validator_test.cc
  auto expected = ObjectMetadataParser::FromString(R"""({
    "acl": [
      {"bucket": "test-bucket",
       "object": "test-object-name",
       "generation": 2345,
       "kind": "storage#objectAccessControl",
       "role": "OWNER", "entity": "user:test-user@gmail.com"}
    ],
    "contentEncoding": "test-content-encoding",
    "contentDisposition": "test-content-disposition",
    "cacheControl": "test-cache-control",
    "contentLanguage": "test-content-language",
    "metageneration": 42,
    "timeDeleted": "2019-08-07T16:22:04.123456789Z",
    "contentType": "test-content-type",
    "size": 123456,
    "timeCreated": "2019-08-07T16:22:04.234567890Z",
    "crc32c": "ImIEBA==",
    "componentCount": 7,
    "md5Hash": "nhB9nTcrtoJr2B01QqQZ1g==",
    "updated": "2019-08-07T16:22:04.345678901Z",
    "storageClass": "test-storage-class",
    "kmsKeyName": "test-kms-key-name",
    "timeStorageClassUpdated": "2019-08-07T16:22:04.456789012Z",
    "temporaryHold": true,
    "retentionExpirationTime": "2019-08-07T16:22:04.567890123Z",
    "metadata": {
        "test-key-1": "test-value-1",
        "test-key-2": "test-value-2"
    },
    "eventBasedHold": true,
    "name": "test-object-name",
    "id": "test-bucket/test-object-name#2345",
    "kind": "storage#object",
    "bucket": "test-bucket",
    "generation": 2345,
    "owner": {
      "entity": "test-entity",
      "entityId": "test-entity-id"
    },
    "customerEncryption": {
      "encryptionAlgorithm": "test-encryption-algorithm",
      "keySha256": "test-key-sha256"
    }
})""");
  EXPECT_STATUS_OK(expected);

  auto actual = GrpcClient::FromProto(input);
  EXPECT_EQ(actual, *expected);
}

TEST(GrpcClientFromProto, ObjectCustomerEncryptionRoundtrip) {
  auto constexpr kText = R"pb(
    encryption_algorithm: "test-encryption-algorithm"
    key_sha256: "test-key-sha256"
  )pb";
  google::storage::v2::Object::CustomerEncryption start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected =
      CustomerEncryption{"test-encryption-algorithm", "test-key-sha256"};
  auto const middle = GrpcClient::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcClient::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

TEST(GrpcClientFromProto, OwnerRoundtrip) {
  auto constexpr kText = R"pb(
    entity: "test-entity" entity_id: "test-entity-id"
  )pb";
  google::storage::v2::Owner start;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &start));
  auto const expected = Owner{"test-entity", "test-entity-id"};
  auto const middle = GrpcClient::FromProto(start);
  EXPECT_EQ(middle, expected);
  auto const end = GrpcClient::ToProto(middle);
  EXPECT_THAT(end, IsProtoEqual(start));
}

TEST(GrpcClientFromProto, Crc32cRoundtrip) {
  std::string const values[] = {
      "AAAAAA==",
      "ImIEBA==",
      "6Y46Mg==",
  };
  for (auto const& start : values) {
    auto const end =
        GrpcClient::Crc32cFromProto(GrpcClient::Crc32cToProto(start).value());
    EXPECT_EQ(start, end);
  }
}

TEST(GrpcClientFromProto, MD5Roundtrip) {
  // As usual, get the values using openssl, e.g.,
  //  TEXT="The quick brown fox jumps over the lazy dog"
  //  /bin/echo -n $TEXT | openssl md5 -binary | openssl base64
  //  /bin/echo -n $TEXT | openssl md5
  struct Test {
    std::string rest;
    std::string proto;
  } cases[] = {
      {"1B2M2Y8AsgTpgAmY7PhCfg==", "d41d8cd98f00b204e9800998ecf8427e"},
      {"nhB9nTcrtoJr2B01QqQZ1g==", "9e107d9d372bb6826bd81d3542a419d6"},
      {"96HF9K981B+JfoQuTVnyCg==", "f7a1c5f4af7cd41f897e842e4d59f20a"},
  };
  for (auto const& test : cases) {
    EXPECT_EQ(GrpcClient::MD5FromProto(test.proto), test.rest);
    EXPECT_EQ(GrpcClient::MD5ToProto(test.rest).value(), test.proto);
  }
}

TEST(GrpcClientFromProto, ObjectAccessControlSimple) {
  google::storage::v2::ObjectAccessControl input;
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
     )""",
                                                            &input));

  auto const expected = ObjectAccessControlParser::FromString(R"""({
     "role": "test-role",
     "id": "test-id",
     "kind": "storage#objectAccessControl",
     "bucket": "test-bucket",
     "object": "test-object",
     "generation": "42",
     "entity": "test-entity",
     "entityId": "test-entity-id",
     "email": "test-email",
     "domain": "test-domain",
     "projectTeam": {
       "projectNumber": "test-project-number",
       "team": "test-team"
     }
  })""");
  ASSERT_STATUS_OK(expected);

  auto actual = GrpcClient::FromProto(input, "test-bucket", "test-object", 42);
  EXPECT_EQ(*expected, actual);
}

TEST(GrpcClientToProto, PredefinedAclObject) {
  EXPECT_EQ(storage_proto::OBJECT_ACL_AUTHENTICATED_READ,
            GrpcClient::ToProtoObject(PredefinedAcl::AuthenticatedRead()));
  EXPECT_EQ(storage_proto::OBJECT_ACL_PRIVATE,
            GrpcClient::ToProtoObject(PredefinedAcl::Private()));
  EXPECT_EQ(storage_proto::OBJECT_ACL_PROJECT_PRIVATE,
            GrpcClient::ToProtoObject(PredefinedAcl::ProjectPrivate()));
  EXPECT_EQ(storage_proto::OBJECT_ACL_PUBLIC_READ,
            GrpcClient::ToProtoObject(PredefinedAcl::PublicRead()));
  EXPECT_EQ(storage_proto::PREDEFINED_OBJECT_ACL_UNSPECIFIED,
            GrpcClient::ToProtoObject(PredefinedAcl::PublicReadWrite()));
  EXPECT_EQ(google::storage::v2::OBJECT_ACL_BUCKET_OWNER_FULL_CONTROL,
            GrpcClient::ToProtoObject(PredefinedAcl::BucketOwnerFullControl()));
  EXPECT_EQ(google::storage::v2::OBJECT_ACL_BUCKET_OWNER_READ,
            GrpcClient::ToProtoObject(PredefinedAcl::BucketOwnerRead()));
}

TEST(GrpcClientToProto, ObjectAccessControlSimple) {
  auto acl = ObjectAccessControlParser::FromString(R"""({
     "role": "test-role",
     "id": "test-id",
     "kind": "storage#objectAccessControl",
     "bucket": "test-bucket",
     "object": "test-object",
     "generation": "42",
     "entity": "test-entity",
     "entityId": "test-entity-id",
     "email": "test-email",
     "domain": "test-domain",
     "projectTeam": {
       "projectNumber": "test-project-number",
       "team": "test-team"
     }
  })""");
  ASSERT_STATUS_OK(acl);
  auto actual = GrpcClient::ToProto(*acl);

  google::storage::v2::ObjectAccessControl expected;
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
     )""",
                                                            &expected));

  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, ObjectAccessControlMinimalFields) {
  ObjectAccessControl acl;
  acl.set_role("test-role");
  acl.set_entity("test-entity");
  auto actual = GrpcClient::ToProto(acl);

  google::storage::v2::ObjectAccessControl expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
     role: "test-role"
     entity: "test-entity"
     )""",
                                                            &expected));

  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

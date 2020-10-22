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
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/testing_util/assert_ok.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_environment.h"
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
using ::google::cloud::testing_util::ScopedEnvironment;

TEST(GrpcClientDirectPath, NotSet) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH", {});
  EXPECT_FALSE(DirectPathEnabled());
}

TEST(GrpcClientDirectPath, Enabled) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH", "foo,storage,bar");
  EXPECT_TRUE(DirectPathEnabled());
}

TEST(GrpcClientDirectPath, NotEnabled) {
  ScopedEnvironment env("GOOGLE_CLOUD_ENABLE_DIRECT_PATH", "not-quite-storage");
  EXPECT_FALSE(DirectPathEnabled());
}

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
# TODO(#4217) - convert acl() field.
    content_language: "test-content-language"
    metageneration: 42
    time_deleted: {
      seconds: 1565194924
      nanos: 123456789
    }
    content_type: "test-content-type"
    size: 123456
    time_created: {
      seconds: 1565194924
      nanos: 234567890
    }
#       This magical value is from:
#       (echo 'ibase=16'; echo  'ImIEBA==' | openssl base64 -d | xxd -p) | bc
#       and 'ImIEBA==' is the CRC32C checksum for (see hash_validator_test.cc)
#       'The quick brown fox jumps over the lazy dog'
    crc32c: { value: 576848900 }
    component_count: 7
#       See hash_validator_test.cc for this magic value.
    md5_hash: "nhB9nTcrtoJr2B01QqQZ1g=="
    etag: "test-etag"
    updated: {
      seconds: 1565194924
      nanos: 345678901
    }
    storage_class: "test-storage-class"
    kms_key_name: "test-kms-key-name"
    time_storage_class_updated: {
      seconds: 1565194924
      nanos: 456789012
    }
    temporary_hold: true
    retention_expiration_time: {
      seconds: 1565194924
      nanos: 567890123
    }
    metadata: { key: "test-key-1" value: "test-value-1" }
    metadata: { key: "test-key-2" value: "test-value-2" }
    event_based_hold: { value: true }
    name: "test-object-name"
    id: "test-object-id"
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
      {"role": "OWNER", "entity": "user:test-user@gmail.com",
       "kind": "storage#objectAccessControl"}
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
    "etag": "test-etag",
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
    "id": "test-object-id",
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
  google::storage::v1::Object::CustomerEncryption start;
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
  google::storage::v1::Owner start;
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
    google::protobuf::UInt32Value v;
    v.set_value(GrpcClient::Crc32cToProto(start));
    auto end = GrpcClient::Crc32cFromProto(v);
    EXPECT_EQ(start, end) << " value=" << v.value();
  }
}

TEST(GrpcClientFromProto, MD5Roundtrip) {
  std::string const values[] = {
      "1B2M2Y8AsgTpgAmY7PhCfg==",
      "nhB9nTcrtoJr2B01QqQZ1g==",
      "96HF9K981B+JfoQuTVnyCg==",
  };
  for (auto const& start : values) {
    auto proto_form = GrpcClient::MD5ToProto(start);
    auto end = GrpcClient::MD5FromProto(proto_form);
    EXPECT_EQ(start, end) << " proto_form=" << proto_form;
  }
}

TEST(GrpcClientFromProto, ObjectAccessControlSimple) {
  google::storage::v1::ObjectAccessControl input;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
     role: "test-role"
     etag: "test-etag"
     id: "test-id"
     bucket: "test-bucket"
     object: "test-object"
     generation: 42
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
     "etag": "test-etag",
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

  auto actual = GrpcClient::FromProto(input);
  EXPECT_EQ(*expected, actual);
}

TEST(GrpcClientToProto, Projection) {
  EXPECT_EQ(storage_proto::CommonEnums::NO_ACL,
            GrpcClient::ToProto(Projection::NoAcl()));
  EXPECT_EQ(storage_proto::CommonEnums::FULL,
            GrpcClient::ToProto(Projection::Full()));
}

TEST(GrpcClientToProto, PredefinedAclBucket) {
  EXPECT_EQ(storage_proto::CommonEnums::BUCKET_ACL_AUTHENTICATED_READ,
            GrpcClient::ToProtoBucket(PredefinedAcl::AuthenticatedRead()));
  EXPECT_EQ(storage_proto::CommonEnums::BUCKET_ACL_PRIVATE,
            GrpcClient::ToProtoBucket(PredefinedAcl::Private()));
  EXPECT_EQ(storage_proto::CommonEnums::BUCKET_ACL_PROJECT_PRIVATE,
            GrpcClient::ToProtoBucket(PredefinedAcl::ProjectPrivate()));
  EXPECT_EQ(storage_proto::CommonEnums::BUCKET_ACL_PUBLIC_READ,
            GrpcClient::ToProtoBucket(PredefinedAcl::PublicRead()));
  EXPECT_EQ(storage_proto::CommonEnums::BUCKET_ACL_PUBLIC_READ_WRITE,
            GrpcClient::ToProtoBucket(PredefinedAcl::PublicReadWrite()));
}

TEST(GrpcClientToProto, PredefinedAclObject) {
  EXPECT_EQ(storage_proto::CommonEnums::OBJECT_ACL_AUTHENTICATED_READ,
            GrpcClient::ToProtoObject(PredefinedAcl::AuthenticatedRead()));
  EXPECT_EQ(storage_proto::CommonEnums::OBJECT_ACL_PRIVATE,
            GrpcClient::ToProtoObject(PredefinedAcl::Private()));
  EXPECT_EQ(storage_proto::CommonEnums::OBJECT_ACL_PROJECT_PRIVATE,
            GrpcClient::ToProtoObject(PredefinedAcl::ProjectPrivate()));
  EXPECT_EQ(storage_proto::CommonEnums::OBJECT_ACL_PUBLIC_READ,
            GrpcClient::ToProtoObject(PredefinedAcl::PublicRead()));
  EXPECT_EQ(storage_proto::CommonEnums::PREDEFINED_OBJECT_ACL_UNSPECIFIED,
            GrpcClient::ToProtoObject(PredefinedAcl::PublicReadWrite()));
  EXPECT_EQ(
      google::storage::v1::CommonEnums::OBJECT_ACL_BUCKET_OWNER_FULL_CONTROL,
      GrpcClient::ToProtoObject(PredefinedAcl::BucketOwnerFullControl()));
  EXPECT_EQ(google::storage::v1::CommonEnums::OBJECT_ACL_BUCKET_OWNER_READ,
            GrpcClient::ToProtoObject(PredefinedAcl::BucketOwnerRead()));
}

TEST(GrpcClientToProto, PredefinedDefaultObjectAcl) {
  EXPECT_EQ(
      storage_proto::CommonEnums::OBJECT_ACL_AUTHENTICATED_READ,
      GrpcClient::ToProto(PredefinedDefaultObjectAcl::AuthenticatedRead()));

  EXPECT_EQ(storage_proto::CommonEnums::OBJECT_ACL_BUCKET_OWNER_FULL_CONTROL,
            GrpcClient::ToProto(
                PredefinedDefaultObjectAcl::BucketOwnerFullControl()));

  EXPECT_EQ(storage_proto::CommonEnums::OBJECT_ACL_PRIVATE,
            GrpcClient::ToProto(PredefinedDefaultObjectAcl::Private()));

  EXPECT_EQ(storage_proto::CommonEnums::OBJECT_ACL_PROJECT_PRIVATE,
            GrpcClient::ToProto(PredefinedDefaultObjectAcl::ProjectPrivate()));

  EXPECT_EQ(storage_proto::CommonEnums::OBJECT_ACL_PUBLIC_READ,
            GrpcClient::ToProto(PredefinedDefaultObjectAcl::PublicRead()));
}

TEST(GrpcClientToProto, ObjectAccessControlSimple) {
  auto acl = ObjectAccessControlParser::FromString(R"""({
     "role": "test-role",
     "etag": "test-etag",
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

  google::storage::v1::ObjectAccessControl expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
     role: "test-role"
     etag: "test-etag"
     id: "test-id"
     bucket: "test-bucket"
     object: "test-object"
     generation: 42
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

  google::storage::v1::ObjectAccessControl expected;
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

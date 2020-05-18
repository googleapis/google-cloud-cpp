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

TEST(GrpcClientFromProto, BucketAllFields) {
  storage_proto::Bucket input;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
# TODO(#4174) - convert acl() field.
# TODO(#4173) - convert default_object_acl() field.
# TODO(#4165) - convert lifecycle
    time_created: {
      seconds: 1565194924
      nanos: 123456789
    }
    id: "test-bucket-id"
    name: "test-bucket"
    project_number: 123456
    metageneration: 1234567
# TODO(#4169) - convert cors() field.
    location: "test-location"
    storage_class: "test-storage-class"
    etag: "test-etag"
    updated: {
      seconds: 1565194924
      nanos: 123456789
    }
    default_event_based_hold: true
    labels: { key: "test-key-1" value: "test-value-1" }
    labels: { key: "test-key-2" value: "test-value-2" }
# TODO(#4168) - convert website() field.
# TODO(#4167) - convert versioning() field.
# TODO(#4172) - convert logging() field.
# TODO(#4170) - convert owner() field.
# TODO(#4171) - convert encryption() field.
# TODO(#4164) - convert billing() field.
# TODO(#4166) - convert retention_policy() field.
)""",
                                                            &input));

  // To get the dates in RFC-3339 format I used:
  //     date --rfc-3339=seconds --date=@1565194924
  auto expected = BucketMetadataParser::FromString(R"""({
    "timeCreated": "2019-08-07T16:22:04.123456789Z",
    "id": "test-bucket-id",
    "kind": "storage#bucket",
    "name": "test-bucket",
    "projectNumber": 123456,
    "metageneration": 1234567,
    "location": "test-location",
    "storageClass": "test-storage-class",
    "etag": "test-etag",
    "updated": "2019-08-07T16:22:04.123456789Z",
    "defaultEventBasedHold": true,
    "labels": {
        "test-key-1": "test-value-1",
        "test-key-2": "test-value-2"
    }
})""");
  EXPECT_STATUS_OK(expected);

  auto actual = GrpcClient::FromProto(input);
  EXPECT_EQ(actual, *expected);
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

TEST(GrpcClientToProto, BucketMetadata) {
  auto input = BucketMetadataParser::FromString(R"""({
    "name": "test-bucket"
})""");
  EXPECT_STATUS_OK(input);

  storage_proto::Bucket expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    name: "test-bucket"
# TODO(#4173) - convert the other fields.
)""",
                                                            &expected));

  auto actual = GrpcClient::ToProto(*input);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, CreateBucketRequestSimple) {
  storage_proto::InsertBucketRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    project: "test-project-id"
    bucket: {
      name: "test-bucket"
# TODO(#4173) - convert the other fields.
    }
)""",
                                                            &expected));

  auto metadata = BucketMetadataParser::FromString(R"""({
    "name": "test-bucket"
})""");
  EXPECT_STATUS_OK(metadata);
  CreateBucketRequest request("test-project-id", *std::move(metadata));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, CreateBucketRequestAllOptions) {
  storage_proto::InsertBucketRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    predefined_acl: BUCKET_ACL_PRIVATE
    predefined_default_object_acl: OBJECT_ACL_PRIVATE
    project: "test-project-id"
    projection: FULL
    bucket: {
      name: "test-bucket"
# TODO(#4173) - convert the other fields.
    }
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

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

TEST(GrpcClientToProto, ListBucketsRequestSimple) {
  storage_proto::ListBucketsRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    project: "test-project-id"
)""",
                                                            &expected));

  ListBucketsRequest request("test-project-id");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, ListBucketsRequestAllFields) {
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

TEST(GrpcClientToProto, GetBucketRequestSimple) {
  storage_proto::GetBucketRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
)""",
                                                            &expected));

  GetBucketMetadataRequest request("test-bucket-name");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, GetBucketRequestAllFields) {
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

TEST(GrpcClientToProto, DeleteBucketRequestSimple) {
  storage_proto::DeleteBucketRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
)""",
                                                            &expected));

  DeleteBucketRequest request("test-bucket-name");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, DeleteBucketRequestAllFields) {
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

TEST(GrpcClientToProto, InsertObjectMediaRequestSimple) {
  storage_proto::InsertObjectRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insert_object_spec: {
          resource: { bucket: "test-bucket-name" name: "test-object-name" }
        }
        # See hash_validator_test.cc for how these magic numbers are obtained.
        object_checksums: {
          crc32c { value: 576848900 }
          md5_hash: "9e107d9d372bb6826bd81d3542a419d6"
        }
      )pb",
      &expected));

  InsertObjectMediaRequest request(
      "test-bucket-name", "test-object-name",
      "The quick brown fox jumps over the lazy dog");
  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, InsertObjectMediaRequestAllOptions) {
  storage_proto::InsertObjectRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insert_object_spec: {
          resource: {
            bucket: "test-bucket-name"
            name: "test-object-name"
            content_type: "test-content-type"
            content_encoding: "test-content-encoding"
            crc32c: { value: 576848900 }
            # See hash_validator_test.cc for how it was obtained.
            md5_hash: "nhB9nTcrtoJr2B01QqQZ1g=="
            kms_key_name: "test-kms-key-name"
          }
          predefined_acl: OBJECT_ACL_PRIVATE
          if_generation_match: { value: 0 }
          if_generation_not_match: { value: 7 }
          if_metageneration_match: { value: 42 }
          if_metageneration_not_match: { value: 84 }
          projection: FULL
        }
        common_object_request_params: {
          encryption_algorithm: "AES256"
          # to get the key value use:
          #   /bin/echo -n "01234567" | openssl base64
          # to get the key hash use (note this command goes over two lines):
          #   /bin/echo -n "01234567" | sha256sum | awk '{printf("%s", $1);}' |
          #     xxd -r -p | openssl base64
          encryption_key: "MDEyMzQ1Njc="
          encryption_key_sha256: "kkWSubED8U+DP6r7Z/SAaR8BmIqkV8AGF2n1jNRzEbw="
        }
        common_request_params: {
          user_project: "test-user-project"
          quota_user: "test-quota-user"
        }
        # See hash_validator_test.cc for how these magic numbers are obtained.
        object_checksums: {
          crc32c { value: 576848900 }
          md5_hash: "9e107d9d372bb6826bd81d3542a419d6"
        }
      )pb",
      &expected));

  auto constexpr kContents = "The quick brown fox jumps over the lazy dog";

  InsertObjectMediaRequest request("test-bucket-name", "test-object-name",
                                   kContents);
  request.set_multiple_options(
      ContentType("test-content-type"),
      ContentEncoding("test-content-encoding"),
      Crc32cChecksumValue(ComputeCrc32cChecksum(kContents)),
      MD5HashValue(ComputeMD5Hash(kContents)), PredefinedAcl("private"),
      IfGenerationMatch(0), IfGenerationNotMatch(7), IfMetagenerationMatch(42),
      IfMetagenerationNotMatch(84), Projection::Full(),
      UserProject("test-user-project"), QuotaUser("test-quota-user"),
      UserIp("test-user-ip"), EncryptionKey::FromBinaryKey("01234567"),
      KmsKeyName("test-kms-key-name"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, InsertObjectMediaRequestWithObjectMetadata) {
  storage_proto::InsertObjectRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insert_object_spec: {
          resource: {
            bucket: "test-bucket-name"
            name: "test-object-name"
            acl: { role: "test-role1" entity: "test-entity1" }
            acl: { role: "test-role2" entity: "test-entity2" }
            cache_control: "test-cache-control"
            content_disposition: "test-content-disposition"
            content_encoding: "test-content-encoding"
            content_language: "test-content-language"
            content_type: "test-content-type"
            event_based_hold: { value: true }
            metadata: { key: "test-key-1" value: "test-value-1" }
            metadata: { key: "test-key-2" value: "test-value-2" }
            storage_class: "test-storage-class"
            temporary_hold: true
          }
        }
        # See hash_validator_test.cc for how these magic numbers are obtained.
        object_checksums: {
          crc32c { value: 576848900 }
          md5_hash: "9e107d9d372bb6826bd81d3542a419d6"
        }
      )pb",
      &expected));

  auto constexpr kContents = "The quick brown fox jumps over the lazy dog";

  std::vector<ObjectAccessControl> acls{
      ObjectAccessControl().set_role("test-role1").set_entity("test-entity1"),
      ObjectAccessControl().set_role("test-role2").set_entity("test-entity2")};

  InsertObjectMediaRequest request("test-bucket-name", "test-object-name",
                                   kContents);
  request.set_multiple_options(WithObjectMetadata(
      ObjectMetadata()
          .set_acl(acls)
          .set_cache_control("test-cache-control")
          .set_content_disposition("test-content-disposition")
          .set_content_encoding("test-content-encoding")
          .set_content_language("test-content-language")
          .set_content_type("test-content-type")
          .set_event_based_hold(true)
          .upsert_metadata("test-key-1", "test-value-1")
          .upsert_metadata("test-key-2", "test-value-2")
          .set_storage_class("test-storage-class")
          .set_temporary_hold(true)));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, DeleteObjectRequestSimple) {
  storage_proto::DeleteObjectRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    object: "test-object-name"
)""",
                                                            &expected));

  DeleteObjectRequest request("test-bucket-name", "test-object-name");

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, DeleteObjectRequestAllFields) {
  storage_proto::DeleteObjectRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
    bucket: "test-bucket-name"
    object: "test-object-name"
    generation: 1234
    if_generation_match: { value: 2345 }
    if_generation_not_match: { value: 3456 }
    if_metageneration_match: { value: 42 }
    if_metageneration_not_match: { value: 7 }
    common_request_params: {
      quota_user: "test-quota-user"
      user_project: "test-user-project"
    }
)""",
                                                            &expected));

  DeleteObjectRequest request("test-bucket-name", "test-object-name");
  request.set_multiple_options(
      Generation(1234), IfGenerationMatch(2345), IfGenerationNotMatch(3456),
      IfMetagenerationMatch(42), IfMetagenerationNotMatch(7),
      UserProject("test-user-project"), QuotaUser("test-quota-user"),
      UserIp("test-user-ip"));

  auto actual = GrpcClient::ToProto(request);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, ResumableUploadRequestSimple) {
  google::storage::v1::StartResumableWriteRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
      insert_object_spec: {
          resource: {
            name: "test-object"
            bucket: "test-bucket"
          }
      })""",
                                                            &expected));

  ResumableUploadRequest req("test-bucket", "test-object");

  auto actual = GrpcClient::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, ResumableUploadRequestAllFields) {
  google::storage::v1::StartResumableWriteRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
      insert_object_spec: {
          resource: {
            name: "test-object"
            bucket: "test-bucket"
            content_encoding: "test-content-encoding"
            content_type: "test-content-type"
            crc32c: { value: 576848900 }
            md5_hash: "nhB9nTcrtoJr2B01QqQZ1g=="
            kms_key_name: "test-kms-key-name"
          }
          predefined_acl: OBJECT_ACL_PRIVATE
          if_generation_match: { value: 0 }
          if_generation_not_match: { value: 7 }
          if_metageneration_match: { value: 42 }
          if_metageneration_not_match: { value: 84 }
          projection: FULL
      }
      common_request_params: {
        user_project: "test-user-project"
        quota_user: "test-quota-user"
      }

      common_object_request_params: {
        encryption_algorithm: "AES256"
# to get the key value use:
#   /bin/echo -n "01234567" | openssl base64
# to get the key hash use (note this command goes over two lines):
#   /bin/echo -n "01234567" | sha256sum | awk '{printf("%s", $1);}' |
#     xxd -r -p | openssl base64
        encryption_key: "MDEyMzQ1Njc="
        encryption_key_sha256: "kkWSubED8U+DP6r7Z/SAaR8BmIqkV8AGF2n1jNRzEbw="
      })""",
                                                            &expected));

  ResumableUploadRequest req("test-bucket", "test-object");
  req.set_multiple_options(
      ContentType("test-content-type"),
      ContentEncoding("test-content-encoding"),
      Crc32cChecksumValue(
          ComputeCrc32cChecksum("The quick brown fox jumps over the lazy dog")),
      MD5HashValue(
          ComputeMD5Hash("The quick brown fox jumps over the lazy dog")),
      PredefinedAcl("private"), IfGenerationMatch(0), IfGenerationNotMatch(7),
      IfMetagenerationMatch(42), IfMetagenerationNotMatch(84),
      Projection::Full(), UserProject("test-user-project"),
      QuotaUser("test-quota-user"), UserIp("test-user-ip"),
      EncryptionKey::FromBinaryKey("01234567"),
      KmsKeyName("test-kms-key-name"));

  auto actual = GrpcClient::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, ResumableUploadRequestWithObjectMetadataFields) {
  google::storage::v1::StartResumableWriteRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
      insert_object_spec: {
          resource: {
            name: "test-object"
            bucket: "test-bucket"
            content_encoding: "test-content-encoding"
            content_disposition: "test-content-disposition"
            cache_control: "test-cache-control"
            content_language: "test-content-language"
            content_type: "test-content-type"
            storage_class: "REGIONAL"
            event_based_hold: { value: true }
            metadata: { key: "test-metadata-key1" value: "test-value1" }
            metadata: { key: "test-metadata-key2" value: "test-value2" }
            temporary_hold: true
            acl: { role: "test-role1" entity: "test-entity1" }
            acl: { role: "test-role2" entity: "test-entity2" }
          }
      })""",
                                                            &expected));

  ResumableUploadRequest req("test-bucket", "test-object");
  std::vector<ObjectAccessControl> acls{
      ObjectAccessControl().set_role("test-role1").set_entity("test-entity1"),
      ObjectAccessControl().set_role("test-role2").set_entity("test-entity2")};
  req.set_multiple_options(WithObjectMetadata(
      ObjectMetadata()
          .set_storage_class(storage_class::Regional())
          .set_content_encoding("test-content-encoding")
          .set_content_disposition("test-content-disposition")
          .set_cache_control("test-cache-control")
          .set_content_language("test-content-language")
          .set_content_type("test-content-type")
          .set_event_based_hold(true)
          .upsert_metadata("test-metadata-key1", "test-value1")
          .upsert_metadata("test-metadata-key2", "test-value2")
          .set_storage_class(storage_class::Regional())
          .set_temporary_hold(true)
          .set_acl(std::move(acls))));

  auto actual = GrpcClient::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, QueryResumableUploadRequestSimple) {
  google::storage::v1::QueryWriteStatusRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        upload_id: "test-upload-id"
      )pb",
      &expected));

  QueryResumableUploadRequest req("test-upload-id");

  auto actual = GrpcClient::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, ReadObjectRangeRequestSimple) {
  google::storage::v1::GetObjectMediaRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        bucket: "test-bucket" object: "test-object"
      )pb",
      &expected));

  ReadObjectRangeRequest req("test-bucket", "test-object");

  auto const actual = GrpcClient::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientToProto, ReadObjectRangeRequestAllFields) {
  google::storage::v1::GetObjectMediaRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        bucket: "test-bucket"
        object: "test-object"
        generation: 7
        read_offset: 2000
        read_limit: 1000
        if_generation_match: { value: 1 }
        if_generation_not_match: { value: 2 }
        if_metageneration_match: { value: 3 }
        if_metageneration_not_match: { value: 4 }
        common_request_params: {
          user_project: "test-user-project"
          quota_user: "test-quota-user"
        }
        common_object_request_params: {
          encryption_algorithm: "AES256"
          # to get the key value use:
          #   /bin/echo -n "01234567" | openssl base64
          # to get the key hash use (note this command goes over two lines):
          #   /bin/echo -n "01234567" | sha256sum | awk '{printf("%s", $1);}' |
          #     xxd -r -p | openssl base64
          encryption_key: "MDEyMzQ1Njc="
          encryption_key_sha256: "kkWSubED8U+DP6r7Z/SAaR8BmIqkV8AGF2n1jNRzEbw="
        }
      )pb",
      &expected));

  ReadObjectRangeRequest req("test-bucket", "test-object");
  req.set_multiple_options(
      Generation(7), ReadFromOffset(2000), ReadRange(1000, 3000),
      IfGenerationMatch(1), IfGenerationNotMatch(2), IfMetagenerationMatch(3),
      IfMetagenerationNotMatch(4), UserProject("test-user-project"),
      UserProject("test-user-project"), QuotaUser("test-quota-user"),
      UserIp("test-user-ip"), EncryptionKey::FromBinaryKey("01234567"));

  auto const actual = GrpcClient::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/storage/internal/grpc_client.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_environment.h"
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
using ::google::cloud::testing_util::StatusIs;

// Use gsutil to obtain the CRC32C checksum (in base64):
//    TEXT="The quick brown fox jumps over the lazy dog"
//    /bin/echo -n $TEXT > /tmp/fox.txt
//    gsutil hash /tmp/fox.txt
// Hashes [base64] for /tmp/fox.txt:
//    Hash (crc32c): ImIEBA==
//    Hash (md5)   : nhB9nTcrtoJr2B01QqQZ1g==
//
// Then convert the base64 values to hex
//
//     echo "ImIEBA==" | openssl base64 -d | od -t x1
//     echo "nhB9nTcrtoJr2B01QqQZ1g==" | openssl base64 -d | od -t x1
//
// Which yields (in proto format):
//
//     CRC32C      : 0x22620404
//     MD5         : 9e107d9d372bb6826bd81d3542a419d6
auto constexpr kText = "The quick brown fox jumps over the lazy dog";

// Doing something similar for an alternative text yields:
// Hashes [base64] for /tmp/alt.txt:
//    Hash (crc32c): StZ/gA==
//    Hash (md5)   : StEvo2V/qoDCuaktZSw3IQ==
// In proto format
//     CRC32C      : 0x4ad67f80
//     MD5         : 4ad12fa3657faa80c2b9a92d652c3721
auto constexpr kAlt = "How vexingly quick daft zebras jump!";

TEST(GrpcClientObjectRequest, InsertObjectMediaRequestSimple) {
  storage_proto::InsertObjectRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insert_object_spec: {
          resource: { bucket: "test-bucket-name" name: "test-object-name" }
        }
        object_checksums: {
          # See top-of-file comments for details on the magic numbers
          crc32c { value: 0x22620404 }
          # MD5 hashes are disabled by default
          # md5_hash: "9e107d9d372bb6826bd81d3542a419d6"
        }
      )pb",
      &expected));

  InsertObjectMediaRequest request(
      "test-bucket-name", "test-object-name",
      "The quick brown fox jumps over the lazy dog");
  auto actual = GrpcClient::ToProto(request).value();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientObjectRequest, InsertObjectMediaRequestHashOptions) {
  // See top-of-file comments for details on the magic numbers
  struct Test {
    std::function<void(InsertObjectMediaRequest&)> apply_options;
    std::string expected_checksums;
  } cases[] = {
      // These tests provide the "wrong" hashes. This is what would happen if
      // one was (for example) reading a GCS file, obtained the expected hashes
      // from GCS, and then uploaded to another GCS destination *but*
      // the data was somehow corrupted locally (say a bad disk). In that case,
      // we don't want to recompute the hashes in the upload.
      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(MD5HashValue(ComputeMD5Hash(kText)));
            r.set_option(DisableCrc32cChecksum(true));
          },
          R"pb(
            md5_hash: "9e107d9d372bb6826bd81d3542a419d6")pb",
      },
      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(MD5HashValue(ComputeMD5Hash(kText)));
            r.set_option(DisableCrc32cChecksum(false));
          },
          R"pb(
            md5_hash: "9e107d9d372bb6826bd81d3542a419d6"
            crc32c { value: 0x4ad67f80 })pb",
      },
      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(MD5HashValue(ComputeMD5Hash(kText)));
            r.set_option(Crc32cChecksumValue(ComputeCrc32cChecksum(kText)));
          },
          R"pb(
            md5_hash: "9e107d9d372bb6826bd81d3542a419d6"
            crc32c { value: 0x22620404 })pb",
      },

      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(DisableMD5Hash(false));
            r.set_option(DisableCrc32cChecksum(true));
          },
          R"pb(
            md5_hash: "4ad12fa3657faa80c2b9a92d652c3721")pb",
      },
      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(DisableMD5Hash(false));
            r.set_option(DisableCrc32cChecksum(false));
          },
          R"pb(
            md5_hash: "4ad12fa3657faa80c2b9a92d652c3721"
            crc32c { value: 0x4ad67f80 })pb",
      },
      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(DisableMD5Hash(false));
            r.set_option(Crc32cChecksumValue(ComputeCrc32cChecksum(kText)));
          },
          R"pb(
            md5_hash: "4ad12fa3657faa80c2b9a92d652c3721"
            crc32c { value: 0x22620404 })pb",
      },

      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(DisableMD5Hash(true));
            r.set_option(DisableCrc32cChecksum(true));
          },
          R"pb(
          )pb",
      },
      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(DisableMD5Hash(true));
            r.set_option(DisableCrc32cChecksum(false));
          },
          R"pb(
            crc32c { value: 0x4ad67f80 })pb",
      },
      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(DisableMD5Hash(true));
            r.set_option(Crc32cChecksumValue(ComputeCrc32cChecksum(kText)));
          },
          R"pb(
            crc32c { value: 0x22620404 })pb",
      },
  };

  for (auto const& test : cases) {
    SCOPED_TRACE("Expected outcome " + test.expected_checksums);
    storage_proto::ObjectChecksums expected;
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
        test.expected_checksums, &expected));

    InsertObjectMediaRequest request("test-bucket-name", "test-object-name",
                                     kAlt);
    test.apply_options(request);
    auto actual = GrpcClient::ToProto(request);
    ASSERT_STATUS_OK(actual) << "expected=" << test.expected_checksums;
    EXPECT_THAT(actual->object_checksums(), IsProtoEqual(expected));
  }
}

TEST(GrpcClientObjectRequest, InsertObjectMediaRequestAllOptions) {
  storage_proto::InsertObjectRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        insert_object_spec: {
          resource: {
            bucket: "test-bucket-name"
            name: "test-object-name"
            content_type: "test-content-type"
            content_encoding: "test-content-encoding"
            # Should not be set, the proto file says these values should
            # not be included in the upload
            #     crc32c:
            #     md5_hash:
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
        object_checksums: {
          # See top-of-file comments for details on the magic numbers
          crc32c { value: 0x22620404 }
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

  auto actual = GrpcClient::ToProto(request).value();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientObjectRequest, InsertObjectMediaRequestWithObjectMetadata) {
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
        # See top-of-file comments for details on the magic numbers
        object_checksums: { crc32c { value: 0x22620404 } }
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

  auto actual = GrpcClient::ToProto(request).value();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientObjectRequest, ResumableUploadRequestSimple) {
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

TEST(GrpcClientObjectRequest, ResumableUploadRequestAllFields) {
  google::storage::v1::StartResumableWriteRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(R"""(
      insert_object_spec: {
          resource: {
            name: "test-object"
            bucket: "test-bucket"
            content_encoding: "test-content-encoding"
            content_type: "test-content-type"
            # Should not be set, the proto file says these values should
            # not be included in the upload
            #     crc32c:
            #     md5_hash:
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

TEST(GrpcClientObjectRequest, ResumableUploadRequestWithObjectMetadataFields) {
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

TEST(GrpcClientObjectRequest, QueryResumableUploadRequestSimple) {
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

TEST(GrpcClientObjectRequest, ReadObjectRangeRequestSimple) {
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

TEST(GrpcClientObjectRequest, ReadObjectRangeRequestAllFields) {
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

TEST(GrpcClientObjectRequest, ReadObjectRangeRequestReadLast) {
  google::storage::v1::GetObjectMediaRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        bucket: "test-bucket" object: "test-object" read_offset: -2000
      )pb",
      &expected));

  ReadObjectRangeRequest req("test-bucket", "test-object");
  req.set_multiple_options(ReadLast(2000));

  auto const actual = GrpcClient::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcClientObjectRequest, ReadObjectRangeRequestReadLastZero) {
  google::storage::v1::GetObjectMediaRequest expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        bucket: "test-bucket" object: "test-object"
      )pb",
      &expected));

  ReadObjectRangeRequest req("test-bucket", "test-object");
  req.set_multiple_options(ReadLast(0));

  auto const actual = GrpcClient::ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));

  auto client = GrpcClient::Create(DefaultOptionsGrpc(
      Options{}
          .set<GrpcCredentialOption>(grpc::InsecureChannelCredentials())
          .set<EndpointOption>("localhost:1")));
  StatusOr<std::unique_ptr<ObjectReadSource>> reader = client->ReadObject(req);
  EXPECT_THAT(reader, StatusIs(StatusCode::kOutOfRange));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

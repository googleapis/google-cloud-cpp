// Copyright 2020 Google LLC
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

#include "google/cloud/storage/internal/grpc/object_request_parser.h"
#include "google/cloud/storage/internal/grpc/stub.h"
#include "google/cloud/storage/internal/hash_function_impl.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/grpc_options.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/scoped_environment.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

namespace storage_proto = ::google::storage::v2;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::google::protobuf::TextFormat;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;

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

// Many of the tests need to verify that all fields can be set when creating
// or updating objects. The next two functions provide most of the values for
// such objects. There are a few edge conditions:
// - Some fields, like `storage_class`, an only be set in create operations,
//   we leave those undefined here, and explicitly set them in each test
// - Some fields, like the object name and bucket, are required in some gRPC
//   requests, but not others. We also leave those undefined here.
// - Some fields, like `kms_key`, can be set via an option or via the object
//   metadata. We leave those undefined here too.
google::storage::v2::Object ExpectedFullObjectMetadata() {
  // The fields are sorted as they appear in the .proto file.
  auto constexpr kProto = R"pb(
    # storage_class: "REGIONAL" ## set only where applicable
    content_encoding: "test-content-encoding"
    content_disposition: "test-content-disposition"
    cache_control: "test-cache-control"
    acl: { role: "test-role1" entity: "test-entity1" }
    acl: { role: "test-role2" entity: "test-entity2" }
    content_language: "test-content-language"
    content_type: "test-content-type"
    temporary_hold: true
    metadata: { key: "test-metadata-key1" value: "test-value1" }
    metadata: { key: "test-metadata-key2" value: "test-value2" }
    event_based_hold: true
    custom_time { seconds: 1643126687 nanos: 123000000 }
  )pb";
  google::storage::v2::Object proto;
  if (TextFormat::ParseFromString(kProto, &proto)) return proto;
  ADD_FAILURE() << "Parsing text proto for " << __func__ << " failed";
  return proto;
}

storage::ObjectMetadata FullObjectMetadata() {
  return storage::ObjectMetadata{}
      .set_content_encoding("test-content-encoding")
      .set_content_disposition("test-content-disposition")
      .set_cache_control("test-cache-control")
      .set_acl({storage::ObjectAccessControl()
                    .set_role("test-role1")
                    .set_entity("test-entity1"),
                storage::ObjectAccessControl()
                    .set_role("test-role2")
                    .set_entity("test-entity2")})
      .set_content_language("test-content-language")
      .set_content_type("test-content-type")
      .set_temporary_hold(true)
      .upsert_metadata("test-metadata-key1", "test-value1")
      .upsert_metadata("test-metadata-key2", "test-value2")
      .set_event_based_hold(true)
      .set_custom_time(std::chrono::system_clock::time_point{} +
                       std::chrono::seconds(1643126687) +
                       std::chrono::milliseconds(123));
}

google::storage::v2::CommonObjectRequestParams
ExpectedCommonObjectRequestParams() {
  // To get the magic values use:
  //  /bin/echo -n "01234567" | sha256sum
  auto constexpr kProto = R"pb(
    encryption_algorithm: "AES256"
    encryption_key_bytes: "01234567"
    encryption_key_sha256_bytes: "\x92\x45\x92\xb9\xb1\x03\xf1\x4f\x83\x3f\xaa\xfb\x67\xf4\x80\x69\x1f\x01\x98\x8a\xa4\x57\xc0\x06\x17\x69\xf5\x8c\xd4\x73\x11\xbc"
  )pb";
  google::storage::v2::CommonObjectRequestParams proto;
  if (TextFormat::ParseFromString(kProto, &proto)) return proto;
  ADD_FAILURE() << "Parsing text proto for " << __func__ << " failed";
  return proto;
}

TEST(GrpcObjectRequestParser, ComposeObjectRequestAllOptions) {
  auto constexpr kTextProto = R"pb(
    source_objects { name: "source-object-1" }
    source_objects {
      name: "source-object-2"
      generation: 27
      object_preconditions { if_generation_match: 28 }
    }
    source_objects { name: "source-object-3" generation: 37 }
    source_objects {
      name: "source-object-4"
      object_preconditions { if_generation_match: 48 }
    }
    destination_predefined_acl: "projectPrivate"
    if_generation_match: 1
    if_metageneration_match: 3
    kms_key: "test-only-kms-key"
  )pb";
  google::storage::v2::ComposeObjectRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kTextProto, &expected));
  auto& destination = *expected.mutable_destination();
  destination = ExpectedFullObjectMetadata();
  destination.set_bucket("projects/_/buckets/bucket-name");
  destination.set_name("object-name");
  destination.set_storage_class("STANDARD");
  *expected.mutable_common_object_request_params() =
      ExpectedCommonObjectRequestParams();

  storage::internal::ComposeObjectRequest req(
      "bucket-name",
      {
          storage::ComposeSourceObject{"source-object-1", absl::nullopt,
                                       absl::nullopt},
          storage::ComposeSourceObject{"source-object-2", 27, 28},
          storage::ComposeSourceObject{"source-object-3", 37, absl::nullopt},
          storage::ComposeSourceObject{"source-object-4", absl::nullopt, 48},
      },
      "object-name");
  req.set_multiple_options(
      storage::EncryptionKey::FromBinaryKey("01234567"),
      storage::DestinationPredefinedAcl("projectPrivate"),
      storage::KmsKeyName("test-only-kms-key"), storage::IfGenerationMatch(1),
      storage::IfMetagenerationMatch(3),
      storage::UserProject("test-user-project"),
      storage::WithObjectMetadata(
          FullObjectMetadata().set_storage_class("STANDARD")),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"));

  auto actual = ToProto(req);
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, DeleteObjectAllFields) {
  google::storage::v2::DeleteObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        generation: 7
        if_generation_match: 1
        if_generation_not_match: 2
        if_metageneration_match: 3
        if_metageneration_not_match: 4
      )pb",
      &expected));

  storage::internal::DeleteObjectRequest req("test-bucket", "test-object");
  req.set_multiple_options(
      storage::Generation(7), storage::IfGenerationMatch(1),
      storage::IfGenerationNotMatch(2), storage::IfMetagenerationMatch(3),
      storage::IfMetagenerationNotMatch(4),
      storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"));

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, GetObjectMetadata) {
  google::storage::v2::GetObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket" object: "test-object"
      )pb",
      &expected));

  storage::internal::GetObjectMetadataRequest req("test-bucket", "test-object");

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, GetObjectMetadataAllFields) {
  google::storage::v2::GetObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        generation: 7
        if_generation_match: 1
        if_generation_not_match: 2
        if_metageneration_match: 3
        if_metageneration_not_match: 4
        read_mask { paths: "*" }
        soft_deleted: true
      )pb",
      &expected));

  storage::internal::GetObjectMetadataRequest req("test-bucket", "test-object");
  req.set_multiple_options(
      storage::Generation(7), storage::IfGenerationMatch(1),
      storage::IfGenerationNotMatch(2), storage::IfMetagenerationMatch(3),
      storage::IfMetagenerationNotMatch(4), storage::Projection("full"),
      storage::SoftDeleted(true), storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"));

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, ReadObjectRangeRequestSimple) {
  google::storage::v2::ReadObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket" object: "test-object"
      )pb",
      &expected));

  storage::internal::ReadObjectRangeRequest req("test-bucket", "test-object");

  auto const actual = ToProto(req).value();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, ReadObjectRangeRequestAllFields) {
  google::storage::v2::ReadObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        generation: 7
        read_offset: 2000
        read_limit: 1000
        if_generation_match: 1
        if_generation_not_match: 2
        if_metageneration_match: 3
        if_metageneration_not_match: 4
      )pb",
      &expected));
  *expected.mutable_common_object_request_params() =
      ExpectedCommonObjectRequestParams();

  storage::internal::ReadObjectRangeRequest req("test-bucket", "test-object");
  req.set_multiple_options(
      storage::Generation(7), storage::ReadFromOffset(2000),
      storage::ReadRange(1000, 3000), storage::IfGenerationMatch(1),
      storage::IfGenerationNotMatch(2), storage::IfMetagenerationMatch(3),
      storage::IfMetagenerationNotMatch(4),
      storage::UserProject("test-user-project"),
      storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"),
      storage::EncryptionKey::FromBinaryKey("01234567"));

  auto const actual = ToProto(req).value();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, ReadObjectRangeRequestReadLast) {
  google::storage::v2::ReadObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket"
        object: "test-object"
        read_offset: -2000
      )pb",
      &expected));

  storage::internal::ReadObjectRangeRequest req("test-bucket", "test-object");
  req.set_multiple_options(storage::ReadLast(2000));

  auto const actual = ToProto(req).value();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, ReadObjectRangeRequestReadLastZero) {
  google::storage::v2::ReadObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket" object: "test-object"
      )pb",
      &expected));

  storage::internal::ReadObjectRangeRequest req("test-bucket", "test-object");
  req.set_multiple_options(storage::ReadLast(0));

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, StatusIs(StatusCode::kOutOfRange));
}

TEST(GrpcObjectRequestParser,
     ReadObjectRangeRequestReadLastConflictsWithOffset) {
  google::storage::v2::ReadObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket" object: "test-object"
      )pb",
      &expected));

  storage::internal::ReadObjectRangeRequest req("test-bucket", "test-object");
  req.set_multiple_options(storage::ReadLast(5), storage::ReadFromOffset(7));

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

TEST(GrpcObjectRequestParser,
     ReadObjectRangeRequestReadLastConflictsWithRange) {
  google::storage::v2::ReadObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        bucket: "projects/_/buckets/test-bucket" object: "test-object"
      )pb",
      &expected));

  storage::internal::ReadObjectRangeRequest req("test-bucket", "test-object");
  req.set_multiple_options(storage::ReadLast(5), storage::ReadRange(0, 7));

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, StatusIs(StatusCode::kInvalidArgument));
}

TEST(GrpcObjectRequestParser, PatchObjectRequestAllOptions) {
  auto constexpr kTextProto = R"pb(
    predefined_acl: "projectPrivate"
    if_generation_match: 1
    if_generation_not_match: 2
    if_metageneration_match: 3
    if_metageneration_not_match: 4
    update_mask {}
  )pb";
  google::storage::v2::UpdateObjectRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kTextProto, &expected));
  auto& object = *expected.mutable_object();
  object = ExpectedFullObjectMetadata();
  object.set_name("object-name");
  object.set_bucket("projects/_/buckets/bucket-name");
  object.set_generation(7);
  *expected.mutable_common_object_request_params() =
      ExpectedCommonObjectRequestParams();

  storage::internal::PatchObjectRequest req(
      "bucket-name", "object-name",
      storage::ObjectMetadataPatchBuilder{}
          .SetContentEncoding("test-content-encoding")
          .SetContentDisposition("test-content-disposition")
          .SetCacheControl("test-cache-control")
          .SetContentLanguage("test-content-language")
          .SetContentType("test-content-type")
          .SetMetadata("test-metadata-key1", "test-value1")
          .SetMetadata("test-metadata-key2", "test-value2")
          .SetTemporaryHold(true)
          .SetAcl({
              storage::ObjectAccessControl{}
                  .set_entity("test-entity1")
                  .set_role("test-role1"),
              storage::ObjectAccessControl{}
                  .set_entity("test-entity2")
                  .set_role("test-role2"),
          })
          .SetEventBasedHold(true)
          .SetCustomTime(std::chrono::system_clock::time_point{} +
                         std::chrono::seconds(1643126687) +
                         std::chrono::milliseconds(123)));
  req.set_multiple_options(
      storage::Generation(7), storage::IfGenerationMatch(1),
      storage::IfGenerationNotMatch(2), storage::IfMetagenerationMatch(3),
      storage::IfMetagenerationNotMatch(4),
      storage::PredefinedAcl("projectPrivate"),
      storage::EncryptionKey::FromBinaryKey("01234567"),
      storage::Projection("full"), storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"));

  auto actual = ToProto(req);
  ASSERT_STATUS_OK(actual);
  // First check the paths. We do not care about their order, so checking them
  // with IsProtoEqual does not work.
  EXPECT_THAT(actual->update_mask().paths(),
              UnorderedElementsAre(
                  "acl", "content_encoding", "content_disposition",
                  "cache_control", "content_language", "content_type",
                  "metadata.test-metadata-key1", "metadata.test-metadata-key2",
                  "temporary_hold", "event_based_hold", "custom_time"));
  // Clear the paths, which we already compared, and compare the proto.
  actual->mutable_update_mask()->clear_paths();
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, PatchObjectRequestAllResets) {
  auto constexpr kTextProto = R"pb(
    object { bucket: "projects/_/buckets/bucket-name" name: "object-name" }
    update_mask {}
  )pb";
  google::storage::v2::UpdateObjectRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kTextProto, &expected));

  storage::internal::PatchObjectRequest req(
      "bucket-name", "object-name",
      storage::ObjectMetadataPatchBuilder{}
          .ResetAcl()
          .ResetCacheControl()
          .ResetContentDisposition()
          .ResetContentEncoding()
          .ResetContentLanguage()
          .ResetContentType()
          .ResetEventBasedHold()
          .ResetMetadata()
          .ResetTemporaryHold()
          .ResetCustomTime());

  auto actual = ToProto(req);
  ASSERT_STATUS_OK(actual);
  // First check the paths. We do not care about their order, so checking them
  // with IsProtoEqual does not work.
  EXPECT_THAT(
      actual->update_mask().paths(),
      UnorderedElementsAre("acl", "content_encoding", "content_disposition",
                           "cache_control", "content_language", "content_type",
                           "metadata", "temporary_hold", "event_based_hold",
                           "custom_time"));
  // Clear the paths, which we already compared, and compare the proto.
  actual->mutable_update_mask()->clear_paths();
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, PatchObjectRequestMetadata) {
  auto constexpr kTextProto = R"pb(
    object {
      bucket: "projects/_/buckets/bucket-name"
      name: "object-name"
      metadata { key: "key0" value: "v0" }
    }
    update_mask {}
  )pb";
  google::storage::v2::UpdateObjectRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kTextProto, &expected));

  storage::internal::PatchObjectRequest req(
      "bucket-name", "object-name",
      storage::ObjectMetadataPatchBuilder{}
          .SetMetadata("key0", "v0")
          .ResetMetadata("key1"));

  auto actual = ToProto(req);
  ASSERT_STATUS_OK(actual);
  // First check the paths. We do not care about their order, so checking them
  // with IsProtoEqual does not work.
  EXPECT_THAT(actual->update_mask().paths(),
              UnorderedElementsAre("metadata.key0", "metadata.key1"));
  // Clear the paths, which we already compared, and compare the proto.
  actual->mutable_update_mask()->clear_paths();
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, PatchObjectRequestResetMetadata) {
  auto constexpr kTextProto = R"pb(
    object { bucket: "projects/_/buckets/bucket-name" name: "object-name" }
    update_mask {}
  )pb";
  google::storage::v2::UpdateObjectRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kTextProto, &expected));

  storage::internal::PatchObjectRequest req(
      "bucket-name", "object-name",
      storage::ObjectMetadataPatchBuilder{}.ResetMetadata());

  auto actual = ToProto(req);
  ASSERT_STATUS_OK(actual);
  // First check the paths. We do not care about their order, so checking them
  // with IsProtoEqual does not work.
  EXPECT_THAT(actual->update_mask().paths(), UnorderedElementsAre("metadata"));
  // Clear the paths, which we already compared, and compare the proto.
  actual->mutable_update_mask()->clear_paths();
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, UpdateObjectRequestAllOptions) {
  auto constexpr kTextProto = R"pb(
    predefined_acl: "projectPrivate"
    if_generation_match: 1
    if_generation_not_match: 2
    if_metageneration_match: 3
    if_metageneration_not_match: 4
    update_mask {}
  )pb";
  google::storage::v2::UpdateObjectRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kTextProto, &expected));
  auto& object = *expected.mutable_object();
  object = ExpectedFullObjectMetadata();
  object.set_bucket("projects/_/buckets/bucket-name");
  object.set_name("object-name");
  object.set_generation(7);
  *expected.mutable_common_object_request_params() =
      ExpectedCommonObjectRequestParams();

  storage::internal::UpdateObjectRequest req("bucket-name", "object-name",
                                             FullObjectMetadata());
  req.set_multiple_options(
      storage::Generation(7), storage::IfGenerationMatch(1),
      storage::IfGenerationNotMatch(2), storage::IfMetagenerationMatch(3),
      storage::IfMetagenerationNotMatch(4),
      storage::PredefinedAcl("projectPrivate"),
      storage::EncryptionKey::FromBinaryKey("01234567"),
      storage::Projection("full"), storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"));

  auto actual = ToProto(req);
  ASSERT_STATUS_OK(actual);
  // First check the paths, we do not care about their order, so checking them
  // with IsProtoEqual does not work.
  EXPECT_THAT(
      actual->update_mask().paths(),
      UnorderedElementsAre("acl", "content_encoding", "content_disposition",
                           "cache_control", "content_language", "content_type",
                           "metadata", "temporary_hold", "event_based_hold",
                           "custom_time"));
  // Clear the paths, which we already compared, and test the rest
  actual->mutable_update_mask()->clear_paths();
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, InsertObjectMediaRequestSimple) {
  storage_proto::WriteObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        write_object_spec: {
          resource: {
            bucket: "projects/_/buckets/test-bucket-name"
            name: "test-object-name"
          }
        }
      )pb",
      &expected));

  storage::internal::InsertObjectMediaRequest request(
      "test-bucket-name", "test-object-name",
      "The quick brown fox jumps over the lazy dog");
  auto actual = ToProto(request).value();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, MaybeFinalizeInsertObjectMediaRequest) {
  using storage::internal::InsertObjectMediaRequest;
  // See top-of-file comments for details on the magic numbers
  struct Test {
    std::function<void(InsertObjectMediaRequest&)> apply_options;
    std::string expected_checksums;
  } cases[] = {
      // These tests provide the "wrong" hashes. This is what would happen if
      // one was (for example) reading a GCS file, obtained the expected hashes
      // from GCS, and then uploaded to another GCS destination *but* the data
      // was somehow corrupted locally (say a bad disk). In that case, we don't
      // want to recompute the hashes in the upload.
      {
          [](storage::internal::InsertObjectMediaRequest& r) {
            r.set_option(storage::MD5HashValue(storage::ComputeMD5Hash(kText)));
            r.set_option(storage::DisableCrc32cChecksum(true));
          },
          R"pb(
            md5_hash: "\x9e\x10\x7d\x9d\x37\x2b\xb6\x82\x6b\xd8\x1d\x35\x42\xa4\x19\xd6")pb",
      },
      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(storage::MD5HashValue(storage::ComputeMD5Hash(kText)));
            r.set_option(storage::DisableCrc32cChecksum(false));
          },
          R"pb(
            md5_hash: "\x9e\x10\x7d\x9d\x37\x2b\xb6\x82\x6b\xd8\x1d\x35\x42\xa4\x19\xd6"
            crc32c: 0x4ad67f80)pb",
      },
      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(storage::MD5HashValue(storage::ComputeMD5Hash(kText)));
            r.set_option(storage::Crc32cChecksumValue(
                storage::ComputeCrc32cChecksum(kText)));
          },
          R"pb(
            md5_hash: "\x9e\x10\x7d\x9d\x37\x2b\xb6\x82\x6b\xd8\x1d\x35\x42\xa4\x19\xd6"
            crc32c: 0x22620404)pb",
      },

      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(storage::DisableMD5Hash(false));
            r.set_option(storage::DisableCrc32cChecksum(true));
          },
          R"pb(
            md5_hash: "\x4a\xd1\x2f\xa3\x65\x7f\xaa\x80\xc2\xb9\xa9\x2d\x65\x2c\x37\x21")pb",
      },
      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(storage::DisableMD5Hash(false));
            r.set_option(storage::DisableCrc32cChecksum(false));
          },
          R"pb(
            md5_hash: "\x4a\xd1\x2f\xa3\x65\x7f\xaa\x80\xc2\xb9\xa9\x2d\x65\x2c\x37\x21"
            crc32c: 0x4ad67f80)pb",
      },
      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(storage::DisableMD5Hash(false));
            r.set_option(storage::Crc32cChecksumValue(
                storage::ComputeCrc32cChecksum(kText)));
          },
          R"pb(
            md5_hash: "\x4a\xd1\x2f\xa3\x65\x7f\xaa\x80\xc2\xb9\xa9\x2d\x65\x2c\x37\x21"
            crc32c: 0x22620404)pb",
      },

      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(storage::DisableMD5Hash(true));
            r.set_option(storage::DisableCrc32cChecksum(true));
          },
          R"pb(
          )pb",
      },
      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(storage::DisableMD5Hash(true));
            r.set_option(storage::DisableCrc32cChecksum(false));
          },
          R"pb(
            crc32c: 0x4ad67f80)pb",
      },
      {
          [](InsertObjectMediaRequest& r) {
            r.set_option(storage::DisableMD5Hash(true));
            r.set_option(storage::Crc32cChecksumValue(
                storage::ComputeCrc32cChecksum(kText)));
          },
          R"pb(
            crc32c: 0x22620404)pb",
      },
  };
  for (auto const& test : cases) {
    SCOPED_TRACE("Expected outcome " + test.expected_checksums);
    storage_proto::ObjectChecksums expected;
    ASSERT_TRUE(
        TextFormat::ParseFromString(test.expected_checksums, &expected));

    storage::internal::InsertObjectMediaRequest request(
        "test-bucket-name", "test-object-name", kAlt);
    test.apply_options(request);
    request.set_multiple_options();
    request.hash_function().Update(0, kAlt);
    storage_proto::WriteObjectRequest write_request;
    grpc::WriteOptions write_options;
    auto status = MaybeFinalize(write_request, write_options, request, false);
    ASSERT_STATUS_OK(status);
    EXPECT_TRUE(write_request.finish_write());
    EXPECT_TRUE(write_options.is_last_message());
    EXPECT_THAT(write_request.object_checksums(), IsProtoEqual(expected));
  }
}

TEST(GrpcObjectRequestParser, InsertObjectMediaRequestAllOptions) {
  auto constexpr kTextProto = R"pb(
    write_object_spec {
      resource: {
        bucket: "projects/_/buckets/test-bucket-name"
        name: "test-object-name"
        content_type: "test-content-type"
        content_encoding: "test-content-encoding"
        # Should not be set, the proto file says these values should
        # not be included in the upload
        #     crc32c:
        #     md5_hash:
        kms_key: "test-kms-key-name"
      }
      predefined_acl: "private"
      if_generation_match: 0
      if_generation_not_match: 7
      if_metageneration_match: 42
      if_metageneration_not_match: 84
    })pb";
  storage_proto::WriteObjectRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(kTextProto, &expected));
  *expected.mutable_common_object_request_params() =
      ExpectedCommonObjectRequestParams();

  auto constexpr kContents = "The quick brown fox jumps over the lazy dog";

  storage::internal::InsertObjectMediaRequest request(
      "test-bucket-name", "test-object-name", kContents);
  request.set_multiple_options(
      storage::ContentType("test-content-type"),
      storage::ContentEncoding("test-content-encoding"),
      storage::Crc32cChecksumValue(storage::ComputeCrc32cChecksum(kContents)),
      storage::MD5HashValue(storage::ComputeMD5Hash(kContents)),
      storage::PredefinedAcl("private"), storage::IfGenerationMatch(0),
      storage::IfGenerationNotMatch(7), storage::IfMetagenerationMatch(42),
      storage::IfMetagenerationNotMatch(84), storage::Projection::Full(),
      storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"),
      storage::EncryptionKey::FromBinaryKey("01234567"),
      storage::KmsKeyName("test-kms-key-name"));

  auto actual = ToProto(request).value();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, InsertObjectMediaRequestWithObjectMetadata) {
  storage_proto::WriteObjectRequest expected;
  auto& resource = *expected.mutable_write_object_spec()->mutable_resource();
  resource = ExpectedFullObjectMetadata();
  resource.set_bucket("projects/_/buckets/test-bucket-name");
  resource.set_name("test-object-name");
  resource.set_storage_class("STANDARD");

  auto constexpr kContents = "The quick brown fox jumps over the lazy dog";

  storage::internal::InsertObjectMediaRequest request(
      "test-bucket-name", "test-object-name", kContents);
  request.set_multiple_options(storage::WithObjectMetadata(
      FullObjectMetadata().set_storage_class("STANDARD")));

  auto actual = ToProto(request).value();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, WriteObjectResponseSimple) {
  google::storage::v2::WriteObjectResponse input;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        persisted_size: 123456
      )pb",
      &input));

  auto const actual = FromProto(input, Options{}, {});
  EXPECT_EQ(actual.committed_size.value_or(0), 123456);
  EXPECT_FALSE(actual.payload.has_value());
}

TEST(GrpcObjectRequestParser, WriteObjectResponseWithResource) {
  google::storage::v2::WriteObjectResponse input;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        resource {
          name: "test-object-name"
          bucket: "projects/_/buckets/test-bucket-name"
          size: 123456
        })pb",
      &input));

  auto const actual = FromProto(
      input, Options{},
      RpcMetadata{{{"header", "value"}, {"other-header", "other-value"}}, {}});
  EXPECT_FALSE(actual.committed_size.has_value());
  ASSERT_TRUE(actual.payload.has_value());
  EXPECT_EQ(actual.payload->name(), "test-object-name");
  EXPECT_EQ(actual.payload->bucket(), "test-bucket-name");
  EXPECT_EQ(actual.payload->size(), 123456);
  EXPECT_THAT(actual.request_metadata,
              UnorderedElementsAre(Pair("header", "value"),
                                   Pair("other-header", "other-value")));
}

TEST(GrpcObjectRequestParser, ListObjectsRequest) {
  google::storage::v2::ListObjectsRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        parent: "projects/_/buckets/test-bucket"
      )pb",
      &expected));

  storage::internal::ListObjectsRequest req("test-bucket");

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, ListObjectsRequestAllFields) {
  google::storage::v2::ListObjectsRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        parent: "projects/_/buckets/test-bucket"
        page_size: 10
        page_token: "test-only-invalid"
        delimiter: "/"
        include_trailing_delimiter: true
        prefix: "test/prefix"
        versions: true
        lexicographic_start: "test/prefix/a"
        lexicographic_end: "test/prefix/abc"
        match_glob: "**/*.cc"
        soft_deleted: true
        include_folders_as_prefixes: true
      )pb",
      &expected));

  storage::internal::ListObjectsRequest req("test-bucket");
  req.set_page_token("test-only-invalid");
  req.set_multiple_options(
      storage::MaxResults(10), storage::Delimiter("/"),
      storage::IncludeTrailingDelimiter(true), storage::Prefix("test/prefix"),
      storage::Versions(true), storage::StartOffset("test/prefix/a"),
      storage::EndOffset("test/prefix/abc"), storage::MatchGlob("**/*.cc"),
      storage::SoftDeleted(true), storage::IncludeFoldersAsPrefixes(true),
      storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"));

  auto const actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, ListObjectsResponse) {
  google::storage::v2::ListObjectsResponse response;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        objects { bucket: "projects/_/buckets/test-bucket" name: "object1" }
        objects { bucket: "projects/_/buckets/test-bucket" name: "object2" }
        prefixes: "prefix1/"
        prefixes: "prefix2/"
        next_page_token: "test-only-invalid-token"
      )pb",
      &response));

  auto actual = FromProto(response, Options{});
  EXPECT_EQ(actual.next_page_token, "test-only-invalid-token");
  EXPECT_THAT(actual.prefixes, ElementsAre("prefix1/", "prefix2/"));
  auto get_bucket = [](auto const& o) { return o.bucket(); };
  auto get_name = [](auto const& o) { return o.name(); };
  EXPECT_THAT(actual.items, ElementsAre(ResultOf(get_bucket, "test-bucket"),
                                        ResultOf(get_bucket, "test-bucket")));
  EXPECT_THAT(actual.items, ElementsAre(ResultOf(get_name, "object1"),
                                        ResultOf(get_name, "object2")));
}

TEST(GrpcObjectRequestParser, RewriteObjectRequestAllOptions) {
  auto constexpr kTextProto = R"pb(
    destination_bucket: "projects/_/buckets/destination-bucket"
    destination_name: "destination-object"
    source_bucket: "projects/_/buckets/source-bucket"
    source_object: "source-object"
    source_generation: 7
    rewrite_token: "test-only-rewrite-token"
    destination_predefined_acl: "projectPrivate"
    if_generation_match: 1
    if_generation_not_match: 2
    if_metageneration_match: 3
    if_metageneration_not_match: 4
    if_source_generation_match: 5
    if_source_generation_not_match: 6
    if_source_metageneration_match: 7
    if_source_metageneration_not_match: 8
    max_bytes_rewritten_per_call: 123456
    copy_source_encryption_algorithm: "AES256"
    copy_source_encryption_key_bytes: "ABCDEFGH"
    # Used `/bin/echo -n "ABCDEFGH" | sha256sum` to create this magic string
    copy_source_encryption_key_sha256_bytes: "\x9a\xc2\x19\x7d\x92\x58\x25\x7b\x1a\xe8\x46\x3e\x42\x14\xe4\xcd\x0a\x57\x8b\xc1\x51\x7f\x24\x15\x92\x8b\x91\xbe\x42\x83\xfc\x48"
  )pb";
  google::storage::v2::RewriteObjectRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kTextProto, &expected));
  auto& destination = *expected.mutable_destination();
  destination = ExpectedFullObjectMetadata();
  // Set via the `DestinationKmsKeyName()` option.
  destination.set_kms_key("test-kms-key-name-from-option");
  destination.set_storage_class("STANDARD");
  *expected.mutable_common_object_request_params() =
      ExpectedCommonObjectRequestParams();

  storage::internal::RewriteObjectRequest req(
      "source-bucket", "source-object", "destination-bucket",
      "destination-object", "test-only-rewrite-token");
  req.set_multiple_options(
      storage::DestinationKmsKeyName("test-kms-key-name-from-option"),
      storage::DestinationPredefinedAcl("projectPrivate"),
      storage::EncryptionKey::FromBinaryKey("01234567"),
      storage::IfGenerationMatch(1), storage::IfGenerationNotMatch(2),
      storage::IfMetagenerationMatch(3), storage::IfMetagenerationNotMatch(4),
      storage::IfSourceGenerationMatch(5),
      storage::IfSourceGenerationNotMatch(6),
      storage::IfSourceMetagenerationMatch(7),
      storage::IfSourceMetagenerationNotMatch(8),
      storage::MaxBytesRewrittenPerCall(123456), storage::Projection("full"),
      storage::SourceEncryptionKey::FromBinaryKey("ABCDEFGH"),
      storage::SourceGeneration(7), storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"),
      storage::WithObjectMetadata(
          FullObjectMetadata().set_storage_class("STANDARD")));

  auto const actual = ToProto(req);
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, RewriteObjectRequestNoDestination) {
  google::storage::v2::RewriteObjectRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        destination_bucket: "projects/_/buckets/destination-bucket"
        destination_name: "destination-object"
        source_bucket: "projects/_/buckets/source-bucket"
        source_object: "source-object"
        source_generation: 7
        rewrite_token: "test-only-rewrite-token"
        destination_predefined_acl: "projectPrivate"
        if_generation_match: 1
        if_generation_not_match: 2
        if_metageneration_match: 3
        if_metageneration_not_match: 4
        if_source_generation_match: 5
        if_source_generation_not_match: 6
        if_source_metageneration_match: 7
        if_source_metageneration_not_match: 8
        max_bytes_rewritten_per_call: 123456
        copy_source_encryption_algorithm: "AES256"
        copy_source_encryption_key_bytes: "ABCDEFGH"
        # Used `/bin/echo -n "ABCDEFGH" | sha256sum` to create this magic string
        copy_source_encryption_key_sha256_bytes: "\x9a\xc2\x19\x7d\x92\x58\x25\x7b\x1a\xe8\x46\x3e\x42\x14\xe4\xcd\x0a\x57\x8b\xc1\x51\x7f\x24\x15\x92\x8b\x91\xbe\x42\x83\xfc\x48"
      )pb",
      &expected));
  *expected.mutable_common_object_request_params() =
      ExpectedCommonObjectRequestParams();

  storage::internal::RewriteObjectRequest req(
      "source-bucket", "source-object", "destination-bucket",
      "destination-object", "test-only-rewrite-token");
  req.set_multiple_options(
      storage::DestinationPredefinedAcl("projectPrivate"),
      storage::EncryptionKey::FromBinaryKey("01234567"),
      storage::IfGenerationMatch(1), storage::IfGenerationNotMatch(2),
      storage::IfMetagenerationMatch(3), storage::IfMetagenerationNotMatch(4),
      storage::IfSourceGenerationMatch(5),
      storage::IfSourceGenerationNotMatch(6),
      storage::IfSourceMetagenerationMatch(7),
      storage::IfSourceMetagenerationNotMatch(8),
      storage::MaxBytesRewrittenPerCall(123456), storage::Projection("full"),
      storage::SourceEncryptionKey::FromBinaryKey("ABCDEFGH"),
      storage::SourceGeneration(7), storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"));

  auto const actual = ToProto(req);
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, RewriteObjectResponse) {
  google::storage::v2::RewriteResponse input;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        total_bytes_rewritten: 123456
        object_size: 1234560
        done: false
        rewrite_token: "test-only-token"
        resource {
          bucket: "projects/_/buckets/bucket-name"
          name: "object-name"
        }
      )pb",
      &input));

  auto const actual = FromProto(input, Options{});
  EXPECT_EQ(actual.total_bytes_rewritten, 123456);
  EXPECT_EQ(actual.object_size, 1234560);
  EXPECT_FALSE(actual.done);
  EXPECT_EQ(actual.rewrite_token, "test-only-token");
  EXPECT_EQ(actual.resource.bucket(), "bucket-name");
  EXPECT_EQ(actual.resource.name(), "object-name");
}

TEST(GrpcObjectRequestParser, CopyObjectRequestAllOptions) {
  auto constexpr kTextProto = R"pb(
    destination_bucket: "projects/_/buckets/destination-bucket"
    destination_name: "destination-object"
    source_bucket: "projects/_/buckets/source-bucket"
    source_object: "source-object"
    source_generation: 7
    destination_predefined_acl: "projectPrivate"
    if_generation_match: 1
    if_generation_not_match: 2
    if_metageneration_match: 3
    if_metageneration_not_match: 4
    if_source_generation_match: 5
    if_source_generation_not_match: 6
    if_source_metageneration_match: 7
    if_source_metageneration_not_match: 8
    copy_source_encryption_algorithm: "AES256"
    copy_source_encryption_key_bytes: "ABCDEFGH"
    # Used `/bin/echo -n "ABCDEFGH" | sha256sum` to create this magic string
    copy_source_encryption_key_sha256_bytes: "\x9a\xc2\x19\x7d\x92\x58\x25\x7b\x1a\xe8\x46\x3e\x42\x14\xe4\xcd\x0a\x57\x8b\xc1\x51\x7f\x24\x15\x92\x8b\x91\xbe\x42\x83\xfc\x48"
  )pb";
  google::storage::v2::RewriteObjectRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(kTextProto, &expected));
  auto& destination = *expected.mutable_destination();
  destination = ExpectedFullObjectMetadata();
  destination.set_kms_key("test-kms-key-name-from-option");
  destination.set_storage_class("STANDARD");
  *expected.mutable_common_object_request_params() =
      ExpectedCommonObjectRequestParams();

  storage::internal::CopyObjectRequest req("source-bucket", "source-object",
                                           "destination-bucket",
                                           "destination-object");
  req.set_multiple_options(
      storage::DestinationKmsKeyName("test-kms-key-name-from-option"),
      storage::DestinationPredefinedAcl("projectPrivate"),
      storage::EncryptionKey::FromBinaryKey("01234567"),
      storage::IfGenerationMatch(1), storage::IfGenerationNotMatch(2),
      storage::IfMetagenerationMatch(3), storage::IfMetagenerationNotMatch(4),
      storage::IfSourceGenerationMatch(5),
      storage::IfSourceGenerationNotMatch(6),
      storage::IfSourceMetagenerationMatch(7),
      storage::IfSourceMetagenerationNotMatch(8), storage::Projection("full"),
      storage::SourceEncryptionKey::FromBinaryKey("ABCDEFGH"),
      storage::SourceGeneration(7), storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"),
      storage::WithObjectMetadata(
          FullObjectMetadata().set_storage_class("STANDARD")));

  auto const actual = ToProto(req);
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, CopyObjectRequestNoDestination) {
  google::storage::v2::RewriteObjectRequest expected;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        destination_bucket: "projects/_/buckets/destination-bucket"
        destination_name: "destination-object"
        source_bucket: "projects/_/buckets/source-bucket"
        source_object: "source-object"
        source_generation: 7
        destination_predefined_acl: "projectPrivate"
        if_generation_match: 1
        if_generation_not_match: 2
        if_metageneration_match: 3
        if_metageneration_not_match: 4
        if_source_generation_match: 5
        if_source_generation_not_match: 6
        if_source_metageneration_match: 7
        if_source_metageneration_not_match: 8
        copy_source_encryption_algorithm: "AES256"
        copy_source_encryption_key_bytes: "ABCDEFGH"
        # Used `/bin/echo -n "ABCDEFGH" | sha256sum` to create this magic string
        copy_source_encryption_key_sha256_bytes: "\x9a\xc2\x19\x7d\x92\x58\x25\x7b\x1a\xe8\x46\x3e\x42\x14\xe4\xcd\x0a\x57\x8b\xc1\x51\x7f\x24\x15\x92\x8b\x91\xbe\x42\x83\xfc\x48"
      )pb",
      &expected));
  *expected.mutable_common_object_request_params() =
      ExpectedCommonObjectRequestParams();

  storage::internal::CopyObjectRequest req("source-bucket", "source-object",
                                           "destination-bucket",
                                           "destination-object");
  req.set_multiple_options(
      storage::DestinationPredefinedAcl("projectPrivate"),
      storage::EncryptionKey::FromBinaryKey("01234567"),
      storage::IfGenerationMatch(1), storage::IfGenerationNotMatch(2),
      storage::IfMetagenerationMatch(3), storage::IfMetagenerationNotMatch(4),
      storage::IfSourceGenerationMatch(5),
      storage::IfSourceGenerationNotMatch(6),
      storage::IfSourceMetagenerationMatch(7),
      storage::IfSourceMetagenerationNotMatch(8), storage::Projection("full"),
      storage::SourceEncryptionKey::FromBinaryKey("ABCDEFGH"),
      storage::SourceGeneration(7), storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"));

  auto const actual = ToProto(req);
  ASSERT_STATUS_OK(actual);
  EXPECT_THAT(*actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, ResumableUploadRequestSimple) {
  google::storage::v2::StartResumableWriteRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(R"""(
      write_object_spec: {
          resource: {
            name: "test-object"
            bucket: "projects/_/buckets/test-bucket"
          }
      })""",
                                          &expected));

  storage::internal::ResumableUploadRequest req("test-bucket", "test-object");

  auto actual = ToProto(req).value();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, ResumableUploadRequestAllFields) {
  google::storage::v2::StartResumableWriteRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        write_object_spec: {
          resource: {
            name: "test-object"
            bucket: "projects/_/buckets/test-bucket"
            content_encoding: "test-content-encoding"
            content_type: "test-content-type"
            # Should not be set, the proto file says these values should
            # not be included in the upload
            #     crc32c:
            #     md5_hash:
            kms_key: "test-kms-key-name"
          }
          predefined_acl: "private"
          if_generation_match: 0
          if_generation_not_match: 7
          if_metageneration_match: 42
          if_metageneration_not_match: 84
        }
      )pb",
      &expected));
  *expected.mutable_common_object_request_params() =
      ExpectedCommonObjectRequestParams();

  storage::internal::ResumableUploadRequest req("test-bucket", "test-object");
  req.set_multiple_options(
      storage::ContentType("test-content-type"),
      storage::ContentEncoding("test-content-encoding"),
      storage::Crc32cChecksumValue(storage::ComputeCrc32cChecksum(
          "The quick brown fox jumps over the lazy dog")),
      storage::MD5HashValue(storage::ComputeMD5Hash(
          "The quick brown fox jumps over the lazy dog")),
      storage::PredefinedAcl("private"), storage::IfGenerationMatch(0),
      storage::IfGenerationNotMatch(7), storage::IfMetagenerationMatch(42),
      storage::IfMetagenerationNotMatch(84), storage::Projection::Full(),
      storage::UserProject("test-user-project"),
      storage::QuotaUser("test-quota-user"), storage::UserIp("test-user-ip"),
      storage::EncryptionKey::FromBinaryKey("01234567"),
      storage::KmsKeyName("test-kms-key-name"));

  auto actual = ToProto(req).value();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, ResumableUploadRequestWithObjectMetadataFields) {
  google::storage::v2::StartResumableWriteRequest expected;
  auto& resource = *expected.mutable_write_object_spec()->mutable_resource();
  resource = ExpectedFullObjectMetadata();
  // In this particular case, the object name and bucket are part of the
  // metadata
  resource.set_name("test-object");
  resource.set_bucket("projects/_/buckets/test-bucket");
  resource.set_storage_class("STANDARD");

  storage::internal::ResumableUploadRequest req("test-bucket", "test-object");
  req.set_multiple_options(storage::WithObjectMetadata(
      FullObjectMetadata().set_storage_class("STANDARD")));

  auto actual = ToProto(req).value();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, ResumableUploadRequestWithContentLength) {
  google::storage::v2::StartResumableWriteRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(R"""(
      write_object_spec: {
          resource: {
            name: "test-object"
            bucket: "projects/_/buckets/test-bucket"
          }
          object_size: 123456
      })""",
                                          &expected));

  storage::internal::ResumableUploadRequest req("test-bucket", "test-object");
  req.set_multiple_options(storage::UploadContentLength(123456));

  auto actual = ToProto(req).value();
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, QueryResumableUploadRequestSimple) {
  google::storage::v2::QueryWriteStatusRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        upload_id: "test-upload-id"
      )pb",
      &expected));

  storage::internal::QueryResumableUploadRequest req("test-upload-id");

  auto actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, QueryResumableUploadResponseSimple) {
  google::storage::v2::QueryWriteStatusResponse input;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        persisted_size: 123456
      )pb",
      &input));

  auto const actual = FromProto(input, Options{});
  EXPECT_EQ(actual.committed_size.value_or(0), 123456);
  EXPECT_FALSE(actual.payload.has_value());
}

TEST(GrpcObjectRequestParser, QueryResumableUploadResponseWithResource) {
  google::storage::v2::QueryWriteStatusResponse input;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        resource {
          name: "test-object-name"
          bucket: "projects/_/buckets/test-bucket-name"
          size: 123456
        })pb",
      &input));

  auto const actual = FromProto(input, Options{});
  EXPECT_FALSE(actual.committed_size.has_value());
  ASSERT_TRUE(actual.payload.has_value());
  EXPECT_EQ(actual.payload->name(), "test-object-name");
  EXPECT_EQ(actual.payload->bucket(), "test-bucket-name");
  EXPECT_EQ(actual.payload->size(), 123456);
}

TEST(GrpcObjectRequestParser, DeleteResumableUploadRequest) {
  google::storage::v2::CancelResumableWriteRequest expected;
  EXPECT_TRUE(TextFormat::ParseFromString(
      R"pb(
        upload_id: "test-upload-id"
      )pb",
      &expected));

  storage::internal::DeleteResumableUploadRequest req("test-upload-id");

  auto actual = ToProto(req);
  EXPECT_THAT(actual, IsProtoEqual(expected));
}

TEST(GrpcObjectRequestParser, MaybeFinalizeUploadChunkRequest) {
  using ::google::cloud::storage::internal::CompositeFunction;
  using ::google::cloud::storage::internal::Crc32cHashFunction;
  using ::google::cloud::storage::internal::HashFunction;
  using ::google::cloud::storage::internal::HashValues;
  using ::google::cloud::storage::internal::MD5HashFunction;
  using ::google::cloud::storage::internal::NullHashFunction;

  // See top-of-file comments for details on the magic numbers
  auto make_hasher = [](bool with_crc32c,
                        bool with_md5) -> std::shared_ptr<HashFunction> {
    if (with_crc32c && with_md5) {
      return std::make_shared<CompositeFunction>(
          std::make_unique<Crc32cHashFunction>(), MD5HashFunction::Create());
    }
    if (with_crc32c) return std::make_shared<Crc32cHashFunction>();
    if (with_md5) return MD5HashFunction::Create();
    return std::make_shared<NullHashFunction>();
  };

  struct Test {
    HashValues hashes;
    std::function<std::shared_ptr<HashFunction>()> make_hash_function;
    std::string expected_checksums;
  } const cases[] = {
      // These tests provide the "wrong" hashes. This is what would happen if
      // one was (for example) reading a GCS file, obtained the expected hashes
      // from GCS, and then uploaded to another GCS destination *but* the data
      // was somehow corrupted locally (say a bad disk). In that case, we don't
      // want to recompute the hashes in the upload.
      {
          HashValues{storage::ComputeCrc32cChecksum(kText),
                     storage::ComputeMD5Hash(kText)},
          [&]() { return make_hasher(false, false); },
          R"pb(
            md5_hash: "\x9e\x10\x7d\x9d\x37\x2b\xb6\x82\x6b\xd8\x1d\x35\x42\xa4\x19\xd6"
            crc32c: 0x22620404)pb",
      },
      {
          HashValues{storage::ComputeCrc32cChecksum(kText),
                     storage::ComputeMD5Hash(kText)},
          [&]() { return make_hasher(false, true); },
          R"pb(
            md5_hash: "\x9e\x10\x7d\x9d\x37\x2b\xb6\x82\x6b\xd8\x1d\x35\x42\xa4\x19\xd6"
            crc32c: 0x22620404)pb",
      },
      {
          HashValues{storage::ComputeCrc32cChecksum(kText),
                     storage::ComputeMD5Hash(kText)},
          [&]() { return make_hasher(true, false); },
          R"pb(
            md5_hash: "\x9e\x10\x7d\x9d\x37\x2b\xb6\x82\x6b\xd8\x1d\x35\x42\xa4\x19\xd6"
            crc32c: 0x22620404)pb",
      },
      {
          HashValues{storage::ComputeCrc32cChecksum(kText),
                     storage::ComputeMD5Hash(kText)},
          [&]() { return make_hasher(true, true); },
          R"pb(
            md5_hash: "\x9e\x10\x7d\x9d\x37\x2b\xb6\x82\x6b\xd8\x1d\x35\x42\xa4\x19\xd6"
            crc32c: 0x22620404)pb",
      },

      // In these tests we assume no hashes are provided by the application, and
      // the library computes none, some, or all the hashes.
      {
          HashValues{},
          [&]() { return make_hasher(false, false); },
          R"pb()pb",
      },
      {
          HashValues{},
          [&]() { return make_hasher(false, true); },
          R"pb(
            md5_hash: "\x4a\xd1\x2f\xa3\x65\x7f\xaa\x80\xc2\xb9\xa9\x2d\x65\x2c\x37\x21")pb",
      },
      {
          HashValues{},
          [&]() { return make_hasher(true, false); },
          R"pb(
            crc32c: 0x4ad67f80)pb",
      },
      {
          HashValues{},
          [&]() { return make_hasher(true, true); },
          R"pb(
            md5_hash: "\x4a\xd1\x2f\xa3\x65\x7f\xaa\x80\xc2\xb9\xa9\x2d\x65\x2c\x37\x21"
            crc32c: 0x4ad67f80)pb",
      },
  };
  for (auto const& test : cases) {
    SCOPED_TRACE("Expected outcome " + test.expected_checksums);
    storage_proto::ObjectChecksums expected;
    ASSERT_TRUE(
        TextFormat::ParseFromString(test.expected_checksums, &expected));

    auto request = storage::internal::UploadChunkRequest(
        "test-upload-id", /*offset=*/0,
        /*payload=*/{absl::string_view{kAlt}}, test.make_hash_function(),
        test.hashes);
    request.hash_function().Update(0, kAlt);
    storage_proto::WriteObjectRequest write_request;
    grpc::WriteOptions write_options;
    auto status = MaybeFinalize(write_request, write_options, request, false);
    ASSERT_STATUS_OK(status);
    EXPECT_TRUE(write_request.finish_write());
    EXPECT_TRUE(write_options.is_last_message());
    EXPECT_THAT(write_request.object_checksums(), IsProtoEqual(expected));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage_internal
}  // namespace cloud
}  // namespace google

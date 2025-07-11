// Copyright 2018 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/client_unit_test.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/testing/temp_file.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::internal::CurrentOptions;
using ::google::cloud::storage::testing::TempFile;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::testing::ByMove;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Return;
using ms = std::chrono::milliseconds;

/**
 * Test the functions in Storage::Client related to 'Objects: *'.
 *
 * In general, this file should include for the APIs listed in:
 *
 * https://cloud.google.com/storage/docs/json_api/v1/objects
 */
class ObjectTest : public ::google::cloud::storage::testing::ClientUnitTest {};

ObjectMetadata CreateObject(int index) {
  std::string id = "object-" + std::to_string(index);
  std::string name = id;
  std::string link =
      "https://storage.googleapis.com/storage/v1/b/test-bucket/" + id + "#1";
  nlohmann::json metadata{
      {"bucket", "test-bucket"},
      {"id", id},
      {"name", name},
      {"selfLink", link},
      {"generation", "1"},
      {"kind", "storage#object"},
  };
  return internal::ObjectMetadataParser::FromJson(metadata).value();
};

TEST_F(ObjectTest, InsertObjectMedia) {
  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected =
      storage::internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, InsertObjectMedia)
      .WillOnce([&expected](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());
        EXPECT_EQ("test object contents", request.payload());
        return make_status_or(expected);
      });

  auto client = ClientForMock();
  auto actual = client.InsertObject(
      "test-bucket-name", "test-object-name", "test object contents",
      Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectTest, GetObjectMetadata) {
  std::string text = R"""({
      "bucket": "test-bucket-name",
      "contentDisposition": "a-disposition",
      "contentLanguage": "a-language",
      "contentType": "application/octet-stream",
      "crc32c": "d1e2f3",
      "etag": "XYZ=",
      "generation": "12345",
      "id": "test-bucket-name/test-object-name/12345",
      "kind": "storage#object",
      "md5Hash": "xa1b2c3==",
      "mediaLink": "https://storage.googleapis.com/download/storage/v1/b/test-bucket-name/o/test-object-name?generation=12345&alt=media",
      "metageneration": "4",
      "name": "test-object-name",
      "selfLink": "https://storage.googleapis.com/storage/v1/b/test-bucket-name/o/test-object-name",
      "size": 1024,
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "timeDeleted": "2018-05-19T19:32:24Z",
      "timeStorageClassUpdated": "2018-05-19T19:31:34Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, GetObjectMetadata)
      .WillOnce(Return(StatusOr<ObjectMetadata>(TransientError())))
      .WillOnce([&expected](internal::GetObjectMetadataRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual =
      client.GetObjectMetadata("test-bucket-name", "test-object-name",
                               Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectTest, ListObjects) {
  EXPECT_CALL(*mock_, ListObjects)
      .WillOnce(Return(TransientError()))
      .WillOnce([](internal::ListObjectsRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        internal::ListObjectsResponse response;
        response.items.emplace_back(CreateObject(1));
        response.items.emplace_back(CreateObject(2));
        response.items.emplace_back(CreateObject(3));
        return response;
      });
  auto client = ClientForMock();
  std::vector<std::string> names;
  auto list = client.ListObjects("test-bucket-name",
                                 Options{}.set<UserProjectOption>("u-p-test"));
  for (auto&& o : list) {
    EXPECT_STATUS_OK(o);
    names.push_back(o->name());
  }
  EXPECT_THAT(names, ElementsAre("object-1", "object-2", "object-3"));
}

TEST_F(ObjectTest, ListObjectsAndPrefixes) {
  EXPECT_CALL(*mock_, ListObjects)
      .WillOnce(Return(TransientError()))
      .WillOnce([](internal::ListObjectsRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        internal::ListObjectsResponse response;
        response.items.emplace_back(CreateObject(1));
        response.items.emplace_back(CreateObject(2));
        response.items.emplace_back(CreateObject(3));
        return response;
      });
  auto client = ClientForMock();
  std::vector<std::string> names;
  auto list = client.ListObjectsAndPrefixes(
      "test-bucket-name", Options{}.set<UserProjectOption>("u-p-test"));
  for (auto&& o : list) {
    EXPECT_STATUS_OK(o);
    names.push_back(absl::get<ObjectMetadata>(*o).name());
  }
  EXPECT_THAT(names, ElementsAre("object-1", "object-2", "object-3"));
}

TEST_F(ObjectTest, ReadObject) {
  EXPECT_CALL(*mock_, ReadObject)
      .WillOnce(Return(ByMove(TransientError())))
      .WillOnce([](internal::ReadObjectRangeRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        auto read_source = std::make_unique<testing::MockObjectReadSource>();
        EXPECT_CALL(*read_source, IsOpen()).WillRepeatedly(Return(true));
        EXPECT_CALL(*read_source, Read)
            .WillOnce(Return(internal::ReadSourceResult{1024, {}}));
        EXPECT_CALL(*read_source, Close).Times(1);
        return StatusOr<std::unique_ptr<internal::ObjectReadSource>>(
            std::move(read_source));
      });
  auto client = ClientForMock();
  auto actual = client.ReadObject("test-bucket-name", "test-object-name",
                                  Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual.status());
  std::vector<char> v(1024);
  actual.read(v.data(), v.size());
  EXPECT_EQ(actual.gcount(), 1024);
}

TEST_F(ObjectTest, WriteObject) {
  EXPECT_CALL(*mock_, CreateResumableUpload)
      .WillOnce(Return(TransientError()))
      .WillOnce([](internal::ResumableUploadRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        return internal::CreateResumableUploadResponse{"test-upload-id"};
      });
  auto client = ClientForMock();
  auto writer =
      client.WriteObject("test-bucket-name", "test-object-name",
                         Options{}.set<UserProjectOption>("u-p-test"));
  EXPECT_STATUS_OK(writer.last_status());
  EXPECT_EQ(writer.resumable_session_id(), "test-upload-id");
  std::move(writer).Suspend();
}

TEST_F(ObjectTest, UploadFile) {
  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected =
      storage::internal::ObjectMetadataParser::FromString(text).value();
  auto const contents = std::string{"How vexingly quick daft zebras jump!"};

  EXPECT_CALL(*mock_, InsertObjectMedia)
      .WillOnce([&](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());
        EXPECT_EQ(contents, request.payload());
        return make_status_or(expected);
      });

  TempFile temp(contents);

  auto client = ClientForMock();
  auto actual =
      client.UploadFile(temp.name(), "test-bucket-name", "test-object-name",
                        Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectTest, UploadFileSimple) {
  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected =
      storage::internal::ObjectMetadataParser::FromString(text).value();
  auto const contents = std::string{"some simple contents"};

  EXPECT_CALL(*mock_, InsertObjectMedia)
      .WillOnce([&](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());
        EXPECT_EQ(contents, request.payload());
        return make_status_or(expected);
      });

  TempFile temp(contents);
  auto client = ClientForMock();
  auto actual =
      client.UploadFile(temp.name(), "test-bucket-name", "test-object-name");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectTest, UploadFileResumable) {
  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected =
      storage::internal::ObjectMetadataParser::FromString(text).value();
  auto const contents = std::string{"some not so simple contents"};

  EXPECT_CALL(*mock_, CreateResumableUpload)
      .WillOnce(Return(internal::CreateResumableUploadResponse{"test-upload-id"}));
  EXPECT_CALL(*mock_, UploadChunk).WillOnce(Return(make_status_or(
      internal::QueryResumableUploadResponse{absl::nullopt, expected})));

  TempFile temp(contents);
  auto client = ClientForMock();
  auto actual = client.UploadFile(
      temp.name(), "test-bucket-name", "test-object-name",
      UseResumableUploadSession(""));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectTest, DeleteResumableUpload) {
  EXPECT_CALL(*mock_, DeleteResumableUpload)
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce([](internal::DeleteResumableUploadRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-upload-id", r.upload_session_url());
        return make_status_or(internal::EmptyResponse{});
      });
  auto client = ClientForMock();
  auto status = client.DeleteResumableUpload(
      "test-upload-id", Options{}.set<UserProjectOption>("u-p-test"));
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectTest, DownloadToFile) {
  auto const contents = std::string{"How vexingly quick daft zebras jump!"};

  EXPECT_CALL(*mock_, ReadObject)
      .WillOnce(Return(ByMove(TransientError())))
      .WillOnce([&](internal::ReadObjectRangeRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        auto read_source = std::make_unique<testing::MockObjectReadSource>();
        EXPECT_CALL(*read_source, IsOpen()).WillRepeatedly(Return(true));
        EXPECT_CALL(*read_source, Read)
            .WillOnce(Return(internal::ReadSourceResult{contents.size(), {}}))
            .WillOnce(Return(internal::ReadSourceResult{0, {}}));
        EXPECT_CALL(*read_source, Close).Times(1);
        return StatusOr<std::unique_ptr<internal::ObjectReadSource>>(
            std::move(read_source));
      });

  TempFile temp("");

  auto client = ClientForMock();
  auto actual =
      client.DownloadToFile("test-bucket-name", "test-object-name", temp.name(),
                            Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
}

TEST_F(ObjectTest, DeleteObject) {
  EXPECT_CALL(*mock_, DeleteObject)
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });
  auto client = ClientForMock();
  auto status =
      client.DeleteObject("test-bucket-name", "test-object-name",
                          Options{}.set<UserProjectOption>("u-p-test"));
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectTest, UpdateObject) {
  std::string text = R"""({
      "bucket": "test-bucket-name",
      "contentDisposition": "new-disposition",
      "contentLanguage": "new-language",
      "contentType": "application/octet-stream",
      "crc32c": "d1e2f3",
      "etag": "XYZ=",
      "generation": "12345",
      "id": "test-bucket-name/test-object-name/12345",
      "kind": "storage#object",
      "md5Hash": "xa1b2c3==",
      "mediaLink": "https://storage.googleapis.com/download/storage/v1/b/test-bucket-name/o/test-object-name?generation=12345&alt=media",
      "metageneration": "4",
      "name": "test-object-name",
      "selfLink": "https://storage.googleapis.com/storage/v1/b/test-bucket-name/o/test-object-name",
      "size": 1024,
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "timeDeleted": "2018-05-19T19:32:24Z",
      "timeStorageClassUpdated": "2018-05-19T19:31:34Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, UpdateObject)
      .WillOnce(Return(StatusOr<ObjectMetadata>(TransientError())))
      .WillOnce([&expected](internal::UpdateObjectRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        auto actual_payload = nlohmann::json::parse(r.json_payload());
        nlohmann::json expected_payload = {
            {"acl",
             nlohmann::json{
                 {{"entity", "user-test-user"}, {"role", "READER"}},
             }},
            {"cacheControl", "no-cache"},
            {"contentDisposition", "new-disposition"},
            {"contentEncoding", "new-encoding"},
            {"contentLanguage", "new-language"},
            {"contentType", "new-type"},
            {"eventBasedHold", false},
            {"metadata",
             nlohmann::json{
                 {"test-label", "test-value"},
             }},
        };
        EXPECT_EQ(expected_payload, actual_payload);
        return make_status_or(expected);
      });
  ObjectMetadata update;
  update.mutable_acl().push_back(
      ObjectAccessControl().set_entity("user-test-user").set_role("READER"));
  update.set_cache_control("no-cache")
      .set_content_disposition("new-disposition")
      .set_content_encoding("new-encoding")
      .set_content_language("new-language")
      .set_content_type("new-type");
  update.mutable_metadata().emplace("test-label", "test-value");
  auto client = ClientForMock();
  auto actual =
      client.UpdateObject("test-bucket-name", "test-object-name", update,
                          Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectTest, PatchObject) {
  std::string text = R"""({
      "bucket": "test-bucket-name",
      "contentDisposition": "new-disposition",
      "contentLanguage": "new-language",
      "contentType": "application/octet-stream",
      "crc32c": "d1e2f3",
      "etag": "XYZ=",
      "generation": "12345",
      "id": "test-bucket-name/test-object-name/12345",
      "kind": "storage#object",
      "md5Hash": "xa1b2c3==",
      "mediaLink": "https://storage.googleapis.com/download/storage/v1/b/test-bucket-name/o/test-object-name?generation=12345&alt=media",
      "metageneration": "4",
      "name": "test-object-name",
      "selfLink": "https://storage.googleapis.com/storage/v1/b/test-bucket-name/o/test-object-name",
      "size": 1024,
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "timeDeleted": "2018-05-19T19:32:24Z",
      "timeStorageClassUpdated": "2018-05-19T19:31:34Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  auto expected = internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, PatchObject)
      .WillOnce(Return(StatusOr<ObjectMetadata>(TransientError())))
      .WillOnce([&expected](internal::PatchObjectRequest const& r) {
        EXPECT_EQ(CurrentOptions().get<AuthorityOption>(), "a-default");
        EXPECT_EQ(CurrentOptions().get<UserProjectOption>(), "u-p-test");
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        EXPECT_THAT(r.payload(), HasSubstr("new-disposition"));
        EXPECT_THAT(r.payload(), HasSubstr("x-made-up-lang"));
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual =
      client.PatchObject("test-bucket-name", "test-object-name",
                         ObjectMetadataPatchBuilder()
                             .SetContentDisposition("new-disposition")
                             .SetContentLanguage("x-made-up-lang"),
                         Options{}.set<UserProjectOption>("u-p-test"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

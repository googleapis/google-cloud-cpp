// Copyright 2018 Google LLC
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

#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/storage/oauth2/google_credentials.h"
#include "google/cloud/storage/retry_policy.h"
#include "google/cloud/storage/testing/canonical_errors.h"
#include "google/cloud/storage/testing/client_unit_test.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "google/cloud/storage/testing/retry_tests.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {

using ::google::cloud::storage::testing::canonical_errors::PermanentError;
using ::google::cloud::storage::testing::canonical_errors::TransientError;
using ::google::cloud::testing_util::StatusIs;
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

TEST_F(ObjectTest, InsertObjectMedia) {
  std::string text = R"""({
      "name": "test-bucket-name/test-object-name/1"
})""";
  auto expected =
      storage::internal::ObjectMetadataParser::FromString(text).value();

  EXPECT_CALL(*mock_, InsertObjectMedia)
      .WillOnce([&expected](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ("test-bucket-name", request.bucket_name());
        EXPECT_EQ("test-object-name", request.object_name());
        EXPECT_EQ("test object contents", request.contents());
        return make_status_or(expected);
      });

  auto client = ClientForMock();
  auto actual = client.InsertObject("test-bucket-name", "test-object-name",
                                    "test object contents");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectTest, InsertObjectMediaTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectMetadata>(
      mock_, EXPECT_CALL(*mock_, InsertObjectMedia),
      [](Client& client) {
        return client
            .InsertObject("test-bucket-name", "test-object-name",
                          "test object contents")
            .status();
      },
      [](Client& client) {
        return client
            .InsertObject("test-bucket-name", "test-object-name",
                          "test object contents", IfGenerationMatch(0))
            .status();
      },
      "InsertObjectMedia");
}

TEST_F(ObjectTest, InsertObjectMediaPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<ObjectMetadata>(
      client, EXPECT_CALL(*mock_, InsertObjectMedia),
      [](Client& client) {
        return client
            .InsertObject("test-bucket-name", "test-object-name",
                          "test object contents")
            .status();
      },
      "InsertObjectMedia");
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
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual =
      client.GetObjectMetadata("test-bucket-name", "test-object-name");
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectTest, GetObjectMetadataTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectMetadata>(
      mock_, EXPECT_CALL(*mock_, GetObjectMetadata),
      [](Client& client) {
        return client.GetObjectMetadata("test-bucket-name", "test-object-name")
            .status();
      },
      "GetObjectMetadata");
}

TEST_F(ObjectTest, GetObjectMetadataPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<ObjectMetadata>(
      client, EXPECT_CALL(*mock_, GetObjectMetadata),
      [](Client& client) {
        return client.GetObjectMetadata("test-bucket-name", "test-object-name")
            .status();
      },
      "GetObjectMetadata");
}

TEST_F(ObjectTest, DeleteObject) {
  EXPECT_CALL(*mock_, DeleteObject)
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(TransientError())))
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });
  auto client = ClientForMock();
  auto status = client.DeleteObject("test-bucket-name", "test-object-name");
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectTest, DeleteObjectTooManyFailures) {
  testing::TooManyFailuresStatusTest<internal::EmptyResponse>(
      mock_, EXPECT_CALL(*mock_, DeleteObject),
      [](Client& client) {
        return client.DeleteObject("test-bucket-name", "test-object-name");
      },
      [](Client& client) {
        return client.DeleteObject("test-bucket-name", "test-object-name",
                                   IfGenerationMatch(7));
      },
      "DeleteObject");
}

TEST_F(ObjectTest, DeleteObjectPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<internal::EmptyResponse>(
      client, EXPECT_CALL(*mock_, DeleteObject),
      [](Client& client) {
        return client.DeleteObject("test-bucket-name", "test-object-name");
      },
      "DeleteObject");
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
      client.UpdateObject("test-bucket-name", "test-object-name", update);
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectTest, UpdateObjectTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectMetadata>(
      mock_, EXPECT_CALL(*mock_, UpdateObject),
      [](Client& client) {
        return client
            .UpdateObject("test-bucket-name", "test-object-name",
                          ObjectMetadata().set_content_language("new-language"))
            .status();
      },
      [](Client& client) {
        return client
            .UpdateObject("test-bucket-name", "test-object-name",
                          ObjectMetadata().set_content_language("new-language"),
                          IfMetagenerationMatch(42))
            .status();
      },
      "UpdateObject");
}

TEST_F(ObjectTest, UpdateObjectPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<ObjectMetadata>(
      client, EXPECT_CALL(*mock_, UpdateObject),
      [](Client& client) {
        return client
            .UpdateObject("test-bucket-name", "test-object-name",
                          ObjectMetadata().set_content_language("new-language"))
            .status();
      },
      "UpdateObject");
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
        EXPECT_EQ("test-bucket-name", r.bucket_name());
        EXPECT_EQ("test-object-name", r.object_name());
        EXPECT_THAT(r.payload(), HasSubstr("new-disposition"));
        EXPECT_THAT(r.payload(), HasSubstr("x-made-up-lang"));
        return make_status_or(expected);
      });
  auto client = ClientForMock();
  auto actual = client.PatchObject("test-bucket-name", "test-object-name",
                                   ObjectMetadataPatchBuilder()
                                       .SetContentDisposition("new-disposition")
                                       .SetContentLanguage("x-made-up-lang"));
  ASSERT_STATUS_OK(actual);
  EXPECT_EQ(expected, *actual);
}

TEST_F(ObjectTest, PatchObjectTooManyFailures) {
  testing::TooManyFailuresStatusTest<ObjectMetadata>(
      mock_, EXPECT_CALL(*mock_, PatchObject),
      [](Client& client) {
        return client
            .PatchObject(
                "test-bucket-name", "test-object-name",
                ObjectMetadataPatchBuilder().SetContentLanguage("x-pig-latin"))
            .status();
      },
      [](Client& client) {
        return client
            .PatchObject(
                "test-bucket-name", "test-object-name",
                ObjectMetadataPatchBuilder().SetContentLanguage("x-pig-latin"),
                IfMetagenerationMatch(42))
            .status();
      },
      "PatchObject");
}

TEST_F(ObjectTest, PatchObjectPermanentFailure) {
  auto client = ClientForMock();
  testing::PermanentFailureStatusTest<ObjectMetadata>(
      client, EXPECT_CALL(*mock_, PatchObject),
      [](Client& client) {
        return client
            .PatchObject(
                "test-bucket-name", "test-object-name",
                ObjectMetadataPatchBuilder().SetContentLanguage("x-pig-latin"))
            .status();
      },
      "PatchObject");
}

TEST_F(ObjectTest, ReadObjectTooManyFailures) {
  // We cannot use google::cloud::storage::testing::TooManyFailuresStatusTest,
  // because that assumes the type returned by the RawClient operation is
  // copyable.
  using ReturnType = std::unique_ptr<internal::ObjectReadSource>;

  auto transient_error = [](internal::ReadObjectRangeRequest const&) {
    return StatusOr<ReturnType>(TransientError());
  };
  EXPECT_CALL(*mock_, ReadObject)
      .WillOnce(transient_error)
      .WillOnce(transient_error)
      .WillOnce(transient_error);

  auto client = ClientForMock();
  Status status =
      client.ReadObject("test-bucket-name", "test-object-name").status();
  EXPECT_EQ(TransientError().code(), status.code());
  EXPECT_THAT(status.message(), HasSubstr("Retry policy exhausted"));
  EXPECT_THAT(status.message(), HasSubstr("ReadObject"));
}

TEST_F(ObjectTest, ReadObjectPermanentFailure) {
  // We cannot use google::cloud::storage::testing::PermanentFailureStatusTest,
  // because that assumes the type returned by the RawClient operation is
  // copyable.
  using ReturnType = std::unique_ptr<internal::ObjectReadSource>;

  auto permanent_error = [](internal::ReadObjectRangeRequest const&) {
    return StatusOr<ReturnType>(PermanentError());
  };
  EXPECT_CALL(*mock_, ReadObject).WillOnce(permanent_error);

  auto client = ClientForMock();
  Status status =
      client.ReadObject("test-bucket-name", "test-object-name").status();
  EXPECT_EQ(PermanentError().code(), status.code());
  EXPECT_THAT(status.message(), HasSubstr("Permanent error"));
  EXPECT_THAT(status.message(), HasSubstr("ReadObject"));
}

ObjectMetadata CreateObject(int index) {
  std::string id = "object-" + std::to_string(index);
  std::string name = id;
  std::string link =
      "https://storage.googleapis.com/storage/v1/b/test-bucket/" + id + "/1";
  nlohmann::json metadata{
      {"bucket", "test-bucket"},
      {"id", id},
      {"name", name},
      {"selfLink", link},
      {"kind", "storage#object"},
  };
  return internal::ObjectMetadataParser::FromJson(metadata).value();
};

TEST_F(ObjectTest, DeleteByPrefix) {
  // Pretend ListObjects returns object-1, object-2, object-3.

  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce([](internal::ListObjectsRequest const& req)
                    -> StatusOr<internal::ListObjectsResponse> {
        EXPECT_EQ("test-bucket", req.bucket_name());
        std::ostringstream os;
        os << req;
        EXPECT_THAT(os.str(), HasSubstr("userProject=project-to-bill"));
        EXPECT_THAT(os.str(), HasSubstr("prefix=object-"));

        internal::ListObjectsResponse response;
        response.items.emplace_back(CreateObject(1));
        response.items.emplace_back(CreateObject(2));
        response.items.emplace_back(CreateObject(3));
        return response;
      });
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-1", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-2", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-3", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });
  auto client = testing::ClientFromMock(mock);
  auto status = DeleteByPrefix(client, "test-bucket", "object-", Versions(),
                               UserProject("project-to-bill"));
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectTest, DeleteByPrefixNoOptions) {
  // Pretend ListObjects returns object-1, object-2, object-3.

  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce([](internal::ListObjectsRequest const& req)
                    -> StatusOr<internal::ListObjectsResponse> {
        EXPECT_EQ("test-bucket", req.bucket_name());

        internal::ListObjectsResponse response;
        response.items.emplace_back(CreateObject(1));
        response.items.emplace_back(CreateObject(2));
        response.items.emplace_back(CreateObject(3));
        return response;
      });
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-1", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-2", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-3", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });
  auto client = testing::ClientFromMock(mock);
  auto status = DeleteByPrefix(client, "test-bucket", "object-");

  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectTest, DeleteByPrefixListFailure) {
  // Pretend ListObjects returns object-1, object-2, object-3.

  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce(Return(StatusOr<internal::ListObjectsResponse>(
          Status(StatusCode::kPermissionDenied, ""))));
  auto client = testing::ClientFromMock(mock);
  auto status = DeleteByPrefix(client, "test-bucket", "object-", Versions(),
                               UserProject("project-to-bill"));
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(ObjectTest, DeleteByPrefixDeleteFailure) {
  // Pretend ListObjects returns object-1, object-2, object-3.

  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ListObjects)
      .WillOnce([](internal::ListObjectsRequest const& req)
                    -> StatusOr<internal::ListObjectsResponse> {
        EXPECT_EQ("test-bucket", req.bucket_name());

        internal::ListObjectsResponse response;
        response.items.emplace_back(CreateObject(1));
        response.items.emplace_back(CreateObject(2));
        response.items.emplace_back(CreateObject(3));
        return response;
      });
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("object-1", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(
          Status(StatusCode::kPermissionDenied, ""))));
  auto client = testing::ClientFromMock(mock);
  auto status = DeleteByPrefix(client, "test-bucket", "object-", Versions(),
                               UserProject("project-to-bill"));
  EXPECT_THAT(status, StatusIs(StatusCode::kPermissionDenied));
}

TEST_F(ObjectTest, ComposeManyNone) {
  auto mock = std::make_shared<testing::MockClient>();
  auto client = testing::ClientFromMock(mock);
  auto res =
      ComposeMany(client, "test-bucket", std::vector<ComposeSourceObject>{},
                  "prefix", "dest", false);
  EXPECT_FALSE(res);
  EXPECT_EQ(StatusCode::kInvalidArgument, res.status().code());
}

ObjectMetadata MockObject(std::string const& bucket_name,
                          std::string const& object_name, int generation) {
  std::stringstream text;
  text << R"""({
        "contentDisposition": "a-disposition",
        "contentLanguage": "a-language",
        "contentType": "application/octet-stream",
        "crc32c": "d1e2f3",
        "etag": "XYZ=",
        "kind": "storage#object",
        "md5Hash": "xa1b2c3==",
        "mediaLink": "https://storage.googleapis.com/download/storage/v1/b/test-bucket-name/o/test-object-name?generation=12345&alt=media",
        "metageneration": "4",
        "selfLink": "https://storage.googleapis.com/storage/v1/b/test-bucket-name/o/test-object-name",
        "size": 1024,
        "storageClass": "STANDARD",
        "timeCreated": "2018-05-19T19:31:14Z",
        "timeDeleted": "2018-05-19T19:32:24Z",
        "timeStorageClassUpdated": "2018-05-19T19:31:34Z",
        "updated": "2018-05-19T19:31:24Z",
)""";
  text << R"("bucket": ")" << bucket_name << "\","
       << R"("generation": ")" << generation << "\","
       << R"("id": ")" << bucket_name << "/" << object_name << "/" << generation
       << "\","
       << R"("name": ")" << object_name << "\"}";
  return internal::ObjectMetadataParser::FromString(text.str()).value();
}

TEST_F(ObjectTest, ComposeManyOne) {
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce([](internal::ComposeObjectRequest const& req)
                    -> StatusOr<ObjectMetadata> {
        EXPECT_EQ("test-bucket", req.bucket_name());
        auto parsed = nlohmann::json::parse(req.JsonPayload());
        auto source_objects = parsed["sourceObjects"];
        EXPECT_EQ(1, source_objects.size());
        EXPECT_EQ(42, source_objects[0]["generation"]);
        EXPECT_EQ("1", source_objects[0]["name"]);

        return MockObject("test-bucket", "test-object", 42);
      });
  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce([](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ("test-bucket", request.bucket_name());
        EXPECT_EQ("prefix", request.object_name());
        EXPECT_EQ("", request.contents());
        return make_status_or(MockObject("test-bucket", "prefix", 42));
      });
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("prefix", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });
  auto client = testing::ClientFromMock(mock);
  auto status = ComposeMany(
      client, "test-bucket",
      std::vector<ComposeSourceObject>{ComposeSourceObject{"1", 42, {}}},
      "prefix", "dest", false);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectTest, ComposeManyThree) {
  auto mock = std::make_shared<testing::MockClient>();
  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce([](internal::ComposeObjectRequest const& req)
                    -> StatusOr<ObjectMetadata> {
        EXPECT_EQ("test-bucket", req.bucket_name());
        auto parsed = nlohmann::json::parse(req.JsonPayload());
        auto source_objects = parsed["sourceObjects"];
        EXPECT_EQ(3, source_objects.size());
        EXPECT_EQ(42, source_objects[0]["generation"]);
        EXPECT_EQ("1", source_objects[0]["name"]);
        EXPECT_EQ(43, source_objects[1]["generation"]);
        EXPECT_EQ("2", source_objects[1]["name"]);
        EXPECT_EQ(44, source_objects[2]["generation"]);
        EXPECT_EQ("3", source_objects[2]["name"]);

        return MockObject("test-bucket", "test-object", 42);
      });
  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce([](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ("test-bucket", request.bucket_name());
        EXPECT_EQ("prefix", request.object_name());
        EXPECT_EQ("", request.contents());
        return make_status_or(MockObject("test-bucket", "prefix", 42));
      });
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("prefix", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });
  auto client = testing::ClientFromMock(mock);
  auto status = ComposeMany(
      client, "test-bucket",
      std::vector<ComposeSourceObject>{ComposeSourceObject{"1", 42, {}},
                                       ComposeSourceObject{"2", 43, {}},
                                       ComposeSourceObject{"3", 44, {}}},
      "prefix", "dest", false);
  EXPECT_STATUS_OK(status);
}

TEST_F(ObjectTest, ComposeManyThreeLayers) {
  auto mock = std::make_shared<testing::MockClient>();

  // Test 63 sources.

  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce([](internal::ComposeObjectRequest const& req)
                    -> StatusOr<ObjectMetadata> {
        EXPECT_EQ("test-bucket", req.bucket_name());
        EXPECT_EQ("prefix.compose-tmp-0", req.object_name());
        auto parsed = nlohmann::json::parse(req.JsonPayload());
        auto source_objects = parsed["sourceObjects"];

        EXPECT_EQ(32, source_objects.size());

        for (int i = 0; i != 32; ++i) {
          EXPECT_EQ(std::to_string(i), source_objects[i]["name"]);
        }

        return MockObject(req.bucket_name(), req.object_name(), 42);
      })
      .WillOnce([](internal::ComposeObjectRequest const& req)
                    -> StatusOr<ObjectMetadata> {
        EXPECT_EQ("test-bucket", req.bucket_name());
        EXPECT_EQ("prefix.compose-tmp-1", req.object_name());
        auto parsed = nlohmann::json::parse(req.JsonPayload());
        auto source_objects = parsed["sourceObjects"];

        EXPECT_EQ(31, source_objects.size());

        for (int i = 0; i != 31; ++i) {
          EXPECT_EQ(std::to_string(i + 32), source_objects[i]["name"]);
        }

        return MockObject(req.bucket_name(), req.object_name(), 42);
      })
      .WillOnce([](internal::ComposeObjectRequest const& req)
                    -> StatusOr<ObjectMetadata> {
        EXPECT_EQ("test-bucket", req.bucket_name());
        EXPECT_EQ("dest", req.object_name());
        auto parsed = nlohmann::json::parse(req.JsonPayload());
        auto source_objects = parsed["sourceObjects"];

        EXPECT_EQ(2, source_objects.size());
        EXPECT_EQ("prefix.compose-tmp-0", source_objects[0]["name"]);
        EXPECT_EQ("prefix.compose-tmp-1", source_objects[1]["name"]);

        return MockObject(req.bucket_name(), req.object_name(), 42);
      });
  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce([](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ("test-bucket", request.bucket_name());
        EXPECT_EQ("prefix", request.object_name());
        EXPECT_EQ("", request.contents());
        return make_status_or(MockObject("test-bucket", "prefix", 42));
      });
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("prefix.compose-tmp-1", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("prefix.compose-tmp-0", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("prefix", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });

  auto client = testing::ClientFromMock(mock);

  std::vector<ComposeSourceObject> sources;

  std::size_t i = 0;
  std::generate_n(std::back_inserter(sources), 63, [&i] {
    return ComposeSourceObject{std::to_string(i++), 42, {}};
  });

  auto res =
      ComposeMany(client, "test-bucket", sources, "prefix", "dest", false);
  EXPECT_STATUS_OK(res);
  EXPECT_EQ("dest", res->name());
}

TEST_F(ObjectTest, ComposeManyComposeFails) {
  auto mock = std::make_shared<testing::MockClient>();

  // Test 63 sources - second composition fails.

  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce(Return(make_status_or(
          MockObject("test-bucket", "prefix.compose-tmp-0", 42))))
      .WillOnce(Return(
          StatusOr<ObjectMetadata>(Status(StatusCode::kPermissionDenied, ""))));

  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce([](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ("test-bucket", request.bucket_name());
        EXPECT_EQ("prefix", request.object_name());
        EXPECT_EQ("", request.contents());
        return make_status_or(MockObject("test-bucket", "prefix", 42));
      });

  // Cleanup is still expected
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("prefix.compose-tmp-0", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      })
      .WillOnce([](internal::DeleteObjectRequest const& r) {
        EXPECT_EQ("test-bucket", r.bucket_name());
        EXPECT_EQ("prefix", r.object_name());
        return make_status_or(internal::EmptyResponse{});
      });

  auto client = testing::ClientFromMock(mock);

  std::vector<ComposeSourceObject> sources;
  std::size_t i = 0;
  std::generate_n(std::back_inserter(sources), 63, [&i] {
    return ComposeSourceObject{std::to_string(i++), 42, {}};
  });

  auto res =
      ComposeMany(client, "test-bucket", sources, "prefix", "dest", false);
  EXPECT_FALSE(res);
  EXPECT_EQ(StatusCode::kPermissionDenied, res.status().code());
}

TEST_F(ObjectTest, ComposeManyCleanupFailsLoudly) {
  auto mock = std::make_shared<testing::MockClient>();

  // Test 63 sources - second composition fails.

  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce(Return(make_status_or(
          MockObject("test-bucket", "prefix.compose-tmp-0", 42))))
      .WillOnce(Return(make_status_or(
          MockObject("test-bucket", "prefix.compose-tmp-1", 42))))
      .WillOnce(Return(make_status_or(MockObject("test-bucket", "dest", 42))));

  // Cleanup is still expected
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(
          Status(StatusCode::kPermissionDenied, ""))));
  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce([](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ("test-bucket", request.bucket_name());
        EXPECT_EQ("prefix", request.object_name());
        EXPECT_EQ("", request.contents());
        return make_status_or(MockObject("test-bucket", "prefix", 42));
      });

  auto client = testing::ClientFromMock(mock);

  std::vector<ComposeSourceObject> sources;
  std::size_t i = 0;
  std::generate_n(std::back_inserter(sources), 63, [&i] {
    return ComposeSourceObject{std::to_string(i++), 42, {}};
  });

  auto res =
      ComposeMany(client, "test-bucket", sources, "prefix", "dest", false);
  EXPECT_FALSE(res);
  EXPECT_EQ(StatusCode::kPermissionDenied, res.status().code());
}

TEST_F(ObjectTest, ComposeManyCleanupFailsSilently) {
  auto mock = std::make_shared<testing::MockClient>();

  // Test 63 sources - second composition fails.

  EXPECT_CALL(*mock, ComposeObject)
      .WillOnce(Return(make_status_or(
          MockObject("test-bucket", "prefix.compose-tmp-0", 42))))
      .WillOnce(Return(make_status_or(
          MockObject("test-bucket", "prefix.compose-tmp-1", 42))))
      .WillOnce(Return(make_status_or(MockObject("test-bucket", "dest", 42))));

  // Cleanup is still expected
  EXPECT_CALL(*mock, DeleteObject)
      .WillOnce(Return(StatusOr<internal::EmptyResponse>(
          Status(StatusCode::kPermissionDenied, ""))));

  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce([](internal::InsertObjectMediaRequest const& request) {
        EXPECT_EQ("test-bucket", request.bucket_name());
        EXPECT_EQ("prefix", request.object_name());
        EXPECT_EQ("", request.contents());
        return make_status_or(MockObject("test-bucket", "prefix", 42));
      });

  auto client = testing::ClientFromMock(mock);

  std::vector<ComposeSourceObject> sources;
  std::size_t i = 0;
  std::generate_n(std::back_inserter(sources), 63, [&i] {
    return ComposeSourceObject{std::to_string(i++), 42, {}};
  });

  auto res =
      ComposeMany(client, "test-bucket", sources, "prefix", "dest", true);
  EXPECT_STATUS_OK(res);
  EXPECT_EQ("dest", res->name());
}

TEST_F(ObjectTest, ComposeManyLockingPrefixFails) {
  auto mock = std::make_shared<testing::MockClient>();

  EXPECT_CALL(*mock, InsertObjectMedia)
      .WillOnce(Return(
          Status(StatusCode::kFailedPrecondition, "Generation mismatch")));
  auto client = testing::ClientFromMock(mock);
  auto res = ComposeMany(
      client, "test-bucket",
      std::vector<ComposeSourceObject>{ComposeSourceObject{"1", 42, {}}},
      "prefix", "dest", false);
  EXPECT_FALSE(res);
  EXPECT_EQ(StatusCode::kFailedPrecondition, res.status().code());
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

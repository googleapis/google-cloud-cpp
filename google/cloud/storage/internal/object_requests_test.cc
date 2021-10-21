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

#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/object_acl_requests.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::testing::HasSubstr;
using ::testing::Not;

TEST(ObjectRequestsTest, ParseFailure) {
  auto actual = internal::ObjectMetadataParser::FromString("{123");
  EXPECT_THAT(actual, Not(IsOk()));
}

TEST(ObjectRequestsTest, ParseAclListFailure) {
  std::string text = R"""({
      "acl": [{
        "kind": "storage#objectAccessControl",
        "id": "acl-id-0",
        "entity": "user-qux"
      },
      "not-a-valid-acl"
      ],
      "bucket": "foo-bar",
      "generation": "12345",
      "id": "foo-bar/baz/12345",
      "kind": "storage#object",
      "name": "baz"
})""";
  auto actual = internal::ObjectMetadataParser::FromString(text);
  EXPECT_THAT(actual, Not(IsOk()));
}

TEST(ObjectRequestsTest, List) {
  ListObjectsRequest request("my-bucket");
  EXPECT_EQ("my-bucket", request.bucket_name());
  request.set_multiple_options(UserProject("my-project"), Prefix("foo/"));

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("my-bucket"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
  EXPECT_THAT(actual, HasSubstr("prefix=foo/"));
}

TEST(ObjectRequestsTest, ParseListResponse) {
  std::string object1 = R"""({
      "bucket": "foo-bar",
      "etag": "XYZ=",
      "id": "baz",
      "kind": "storage#object",
      "generation": 1,
      "location": "US",
      "metadata": {
        "foo": "bar",
        "baz": "qux"
      },
      "metageneration": "4",
      "name": "foo-bar-baz",
      "projectNumber": "123456789",
      "selfLink": "https://storage.googleapis.com/storage/v1/b/foo-bar/baz/1",
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  std::string object2 = R"""({
      "bucket": "foo-bar",
      "etag": "XYZ=",
      "id": "qux",
      "kind": "storage#object",
      "generation": "7",
      "location": "US",
      "metadata": {
        "lbl1": "bar",
        "lbl2": "qux"
      },
      "metageneration": "4",
      "name": "qux",
      "projectNumber": "123456789",
      "selfLink": "https://storage.googleapis.com/storage/v1/b/foo-bar/qux/7",
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  std::string text = R"""({
      "kind": "storage#objects",
      "nextPageToken": "some-token-42",
      "items":
)""";
  text += "[" + object1 + "," + object2 + "],\n";
  text += R"""(
    "prefixes" : ["foo/", "qux/"]}
)""";

  auto o1 = internal::ObjectMetadataParser::FromString(object1).value();
  auto o2 = internal::ObjectMetadataParser::FromString(object2).value();

  auto actual = ListObjectsResponse::FromHttpResponse(text).value();
  EXPECT_EQ("some-token-42", actual.next_page_token);
  EXPECT_THAT(actual.items, ::testing::ElementsAre(o1, o2));
  EXPECT_THAT(actual.prefixes, ::testing::ElementsAre("foo/", "qux/"));
}

TEST(ObjectRequestsTest, ParseListResponseFailure) {
  std::string text = R"""({123)""";

  auto actual = ListObjectsResponse::FromHttpResponse(text);
  EXPECT_THAT(actual, Not(IsOk()));
}

TEST(ObjectRequestsTest, ParseListResponseFailureInItems) {
  std::string text = R"""({"items": [ "invalid-item" ]})""";

  auto actual = ListObjectsResponse::FromHttpResponse(text);
  EXPECT_THAT(actual, Not(IsOk()));
}

TEST(ObjectRequestsTest, Get) {
  GetObjectMetadataRequest request("my-bucket", "my-object");
  request.set_multiple_options(Generation(1), IfMetagenerationMatch(3));
  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("my-object"));
  EXPECT_THAT(str, HasSubstr("generation=1"));
  EXPECT_THAT(str, HasSubstr("ifMetagenerationMatch=3"));
}

TEST(ObjectRequestsTest, InsertObjectMedia) {
  InsertObjectMediaRequest request("my-bucket", "my-object", "object contents");
  request.set_multiple_options(
      IfGenerationMatch(0), Projection("full"), ContentEncoding("media"),
      KmsKeyName("random-key"), PredefinedAcl("authenticatedRead"));
  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("InsertObjectMediaRequest"));
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("my-object"));
  EXPECT_THAT(str, HasSubstr("ifGenerationMatch=0"));
  EXPECT_THAT(str, HasSubstr("projection=full"));
  EXPECT_THAT(str, HasSubstr("kmsKeyName=random-key"));
  EXPECT_THAT(str, HasSubstr("contentEncoding=media"));
  EXPECT_THAT(str, HasSubstr("predefinedAcl=authenticatedRead"));
}

TEST(ObjectRequestsTest, InsertObjectMediaUpdateContents) {
  InsertObjectMediaRequest request("my-bucket", "my-object", "object contents");
  EXPECT_EQ("object contents", request.contents());
  request.set_contents("new contents");
  EXPECT_EQ("new contents", request.contents());
}

TEST(ObjectRequestsTest, Copy) {
  CopyObjectRequest request("source-bucket", "source-object", "my-bucket",
                            "my-object");
  EXPECT_EQ("source-bucket", request.source_bucket());
  EXPECT_EQ("source-object", request.source_object());
  EXPECT_EQ("my-bucket", request.destination_bucket());
  EXPECT_EQ("my-object", request.destination_object());
  request.set_multiple_options(
      IfMetagenerationNotMatch(7), DestinationPredefinedAcl("private"),
      UserProject("my-project"),
      WithObjectMetadata(ObjectMetadata().set_content_type("text/plain")));

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("my-bucket"));
  EXPECT_THAT(actual, HasSubstr("my-object"));
  EXPECT_THAT(actual, HasSubstr("source-bucket"));
  EXPECT_THAT(actual, HasSubstr("=source-object"));
  EXPECT_THAT(actual, HasSubstr("text/plain"));
  EXPECT_THAT(actual, HasSubstr("destinationPredefinedAcl=private"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
}

HttpResponse CreateRangeRequestResponse(
    char const* content_range_header_value) {
  HttpResponse response;
  response.status_code = 200;
  response.headers.emplace(std::string("content-range"),
                           std::string(content_range_header_value));
  response.payload = "some payload";
  return response;
}

TEST(ObjectRequestsTest, ReadObjectRange) {
  ReadObjectRangeRequest request("my-bucket", "my-object");

  EXPECT_EQ("my-bucket", request.bucket_name());
  EXPECT_EQ("my-object", request.object_name());

  request.set_option(storage::UserProject("my-project"));
  request.set_multiple_options(storage::IfGenerationMatch(7),
                               storage::UserProject("my-project"),
                               storage::ReadRange(0, 1024));

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("my-bucket"));
  EXPECT_THAT(actual, HasSubstr("my-object"));
  EXPECT_THAT(actual, HasSubstr("ifGenerationMatch=7"));
  EXPECT_THAT(actual, HasSubstr("my-project"));
  EXPECT_THAT(actual, HasSubstr("begin=0"));
  EXPECT_THAT(actual, HasSubstr("end=1024"));
}

TEST(ObjectRequestsTest, ReadObjectRangeRequiresRangeHeader) {
  EXPECT_FALSE(ReadObjectRangeRequest("test-bucket", "test-object")
                   .RequiresRangeHeader());
  EXPECT_TRUE(ReadObjectRangeRequest("test-bucket", "test-object")
                  .set_multiple_options(ReadRange(0, 2048))
                  .RequiresRangeHeader());
  EXPECT_TRUE(ReadObjectRangeRequest("test-bucket", "test-object")
                  .set_multiple_options(ReadFromOffset(1024))
                  .RequiresRangeHeader());
  EXPECT_FALSE(ReadObjectRangeRequest("test-bucket", "test-object")
                   .set_multiple_options(ReadFromOffset(0))
                   .RequiresRangeHeader());
  EXPECT_TRUE(
      ReadObjectRangeRequest("test-bucket", "test-object")
          .set_multiple_options(ReadRange(0, 2048), ReadFromOffset(1024))
          .RequiresRangeHeader());
  EXPECT_TRUE(ReadObjectRangeRequest("test-bucket", "test-object")
                  .set_multiple_options(ReadLast(1024))
                  .RequiresRangeHeader());
  EXPECT_TRUE(ReadObjectRangeRequest("test-bucket", "test-object")
                  .set_multiple_options(ReadLast(0))
                  .RequiresRangeHeader());
}

TEST(ObjectRequestsTest, ReadObjectRangeRequiresNoCache) {
  EXPECT_FALSE(
      ReadObjectRangeRequest("test-bucket", "test-object").RequiresNoCache());
  EXPECT_TRUE(ReadObjectRangeRequest("test-bucket", "test-object")
                  .set_multiple_options(ReadRange(0, 2048))
                  .RequiresNoCache());
  EXPECT_TRUE(ReadObjectRangeRequest("test-bucket", "test-object")
                  .set_multiple_options(ReadFromOffset(1024))
                  .RequiresNoCache());
  EXPECT_FALSE(ReadObjectRangeRequest("test-bucket", "test-object")
                   .set_multiple_options(ReadFromOffset(0))
                   .RequiresNoCache());
  EXPECT_TRUE(
      ReadObjectRangeRequest("test-bucket", "test-object")
          .set_multiple_options(ReadRange(0, 2048), ReadFromOffset(1024))
          .RequiresNoCache());
  EXPECT_TRUE(ReadObjectRangeRequest("test-bucket", "test-object")
                  .set_multiple_options(ReadLast(1024))
                  .RequiresNoCache());
  EXPECT_TRUE(ReadObjectRangeRequest("test-bucket", "test-object")
                  .set_multiple_options(ReadLast(0))
                  .RequiresNoCache());
}

TEST(ObjectRequestsTest, ReadObjectRangeRangeHeader) {
  EXPECT_EQ("",
            ReadObjectRangeRequest("test-bucket", "test-object").RangeHeader());
  EXPECT_EQ("Range: bytes=0-2047",
            ReadObjectRangeRequest("test-bucket", "test-object")
                .set_multiple_options(ReadRange(0, 2048))
                .RangeHeader());
  EXPECT_EQ("Range: bytes=1024-",
            ReadObjectRangeRequest("test-bucket", "test-object")
                .set_multiple_options(ReadFromOffset(1024))
                .RangeHeader());
  EXPECT_EQ("", ReadObjectRangeRequest("test-bucket", "test-object")
                    .set_multiple_options(ReadFromOffset(0))
                    .RangeHeader());
  EXPECT_EQ("Range: bytes=1024-2047",
            ReadObjectRangeRequest("test-bucket", "test-object")
                .set_multiple_options(ReadRange(0, 2048), ReadFromOffset(1024))
                .RangeHeader());
  EXPECT_EQ("Range: bytes=-1024",
            ReadObjectRangeRequest("test-bucket", "test-object")
                .set_multiple_options(ReadLast(1024))
                .RangeHeader());
  EXPECT_EQ("Range: bytes=-0",
            ReadObjectRangeRequest("test-bucket", "test-object")
                .set_multiple_options(ReadLast(0))
                .RangeHeader());
}

TEST(ObjectRequestsTest, RangeResponseParse) {
  auto actual = ReadObjectRangeResponse::FromHttpResponse(
      CreateRangeRequestResponse("bytes 100-200/20000"));
  EXPECT_EQ(100, actual.first_byte);
  EXPECT_EQ(200, actual.last_byte);
  EXPECT_EQ(20000, actual.object_size);
  EXPECT_EQ("some payload", actual.contents);
}

TEST(ObjectRequestsTest, RangeResponseParseStar) {
  auto actual = ReadObjectRangeResponse::FromHttpResponse(
      CreateRangeRequestResponse("bytes */20000"));
  EXPECT_EQ(0, actual.first_byte);
  EXPECT_EQ(0, actual.last_byte);
  EXPECT_EQ(20000, actual.object_size);
}

TEST(ObjectRequestsTest, RangeResponseParseErrors) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THROW(ReadObjectRangeResponse::FromHttpResponse(
                   CreateRangeRequestResponse("bits 100-200/20000")),
               std::invalid_argument);
  EXPECT_THROW(ReadObjectRangeResponse::FromHttpResponse(
                   CreateRangeRequestResponse("100-200/20000")),
               std::invalid_argument);
  EXPECT_THROW(ReadObjectRangeResponse::FromHttpResponse(
                   CreateRangeRequestResponse("bytes ")),
               std::invalid_argument);
  EXPECT_THROW(ReadObjectRangeResponse::FromHttpResponse(
                   CreateRangeRequestResponse("bytes */")),
               std::invalid_argument);
  EXPECT_THROW(ReadObjectRangeResponse::FromHttpResponse(
                   CreateRangeRequestResponse("bytes 100-200/")),
               std::invalid_argument);
  EXPECT_THROW(ReadObjectRangeResponse::FromHttpResponse(
                   CreateRangeRequestResponse("bytes 100-/20000")),
               std::invalid_argument);
  EXPECT_THROW(ReadObjectRangeResponse::FromHttpResponse(
                   CreateRangeRequestResponse("bytes -200/20000")),
               std::invalid_argument);
#else
  EXPECT_DEATH_IF_SUPPORTED(
      ReadObjectRangeResponse::FromHttpResponse(
          CreateRangeRequestResponse("bits 100-200/20000")),
      "");
  EXPECT_DEATH_IF_SUPPORTED(ReadObjectRangeResponse::FromHttpResponse(
                                CreateRangeRequestResponse("100-200/20000")),
                            "");
  EXPECT_DEATH_IF_SUPPORTED(ReadObjectRangeResponse::FromHttpResponse(
                                CreateRangeRequestResponse("bytes ")),
                            "");
  EXPECT_DEATH_IF_SUPPORTED(ReadObjectRangeResponse::FromHttpResponse(
                                CreateRangeRequestResponse("bytes */")),
                            "");
  EXPECT_DEATH_IF_SUPPORTED(ReadObjectRangeResponse::FromHttpResponse(
                                CreateRangeRequestResponse("bytes 100-200/")),
                            "");
  EXPECT_DEATH_IF_SUPPORTED(ReadObjectRangeResponse::FromHttpResponse(
                                CreateRangeRequestResponse("bytes 100-/20000")),
                            "");
  EXPECT_DEATH_IF_SUPPORTED(ReadObjectRangeResponse::FromHttpResponse(
                                CreateRangeRequestResponse("bytes -200/20000")),
                            "");
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(ObjectRequestsTest, Delete) {
  DeleteObjectRequest request("my-bucket", "my-object");
  request.set_multiple_options(IfMetagenerationNotMatch(7),
                               UserProject("my-project"));
  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("my-bucket"));
  EXPECT_THAT(os.str(), HasSubstr("my-object"));
  EXPECT_THAT(os.str(), HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(os.str(), HasSubstr("userProject=my-project"));
}

TEST(ObjectRequestsTest, Update) {
  ObjectMetadata meta = ObjectMetadata().set_content_type("application/json");
  UpdateObjectRequest request("my-bucket", "my-object", std::move(meta));
  request.set_multiple_options(IfMetagenerationNotMatch(7),
                               UserProject("my-project"));
  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("my-bucket"));
  EXPECT_THAT(os.str(), HasSubstr("my-object"));
  EXPECT_THAT(os.str(), HasSubstr("content_type=application/json"));
  EXPECT_THAT(os.str(), HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(os.str(), HasSubstr("userProject=my-project"));
}

TEST(ObjectRequestsTest, Rewrite) {
  RewriteObjectRequest request("source-bucket", "source-object", "my-bucket",
                               "my-object", "abcd-test-token-0");
  EXPECT_EQ("source-bucket", request.source_bucket());
  EXPECT_EQ("source-object", request.source_object());
  EXPECT_EQ("my-bucket", request.destination_bucket());
  EXPECT_EQ("my-object", request.destination_object());
  EXPECT_EQ("abcd-test-token-0", request.rewrite_token());
  request.set_rewrite_token("abcd-test-token");
  EXPECT_EQ("abcd-test-token", request.rewrite_token());
  request.set_multiple_options(
      IfMetagenerationNotMatch(7), DestinationPredefinedAcl("private"),
      UserProject("my-project"),
      WithObjectMetadata(ObjectMetadata().set_content_type("text/plain")));

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("my-bucket"));
  EXPECT_THAT(actual, HasSubstr("my-object"));
  EXPECT_THAT(actual, HasSubstr("source-bucket"));
  EXPECT_THAT(actual, HasSubstr("source-object"));
  EXPECT_THAT(actual, HasSubstr("abcd-test-token"));
  EXPECT_THAT(actual, HasSubstr("text/plain"));
  EXPECT_THAT(actual, HasSubstr("destinationPredefinedAcl=private"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
}

TEST(ObjectRequestsTest, RewriteObjectResponse) {
  std::string object1 = R"""({
      "kind": "storage#object",
      "bucket": "test-bucket-name",
      "etag": "XYZ=",
      "id": "test-object-name",
      "generation": 1,
      "location": "US",
      "name": "test-object-name",
      "projectNumber": "123456789",
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "updated": "2018-05-19T19:31:24Z"
})""";

  std::string text = R"""({
      "kind": "storage#rewriteResponse",
      "totalBytesRewritten": 7,
      "objectSize": 42,
      "done": false,
      "rewriteToken": "abcd-test-token",
      "resource":)""";

  text += object1 + "\n}";

  auto expected_resource =
      internal::ObjectMetadataParser::FromString(object1).value();

  auto actual = RewriteObjectResponse::FromHttpResponse(text).value();
  EXPECT_EQ(7, actual.total_bytes_rewritten);
  EXPECT_EQ(42, actual.object_size);
  EXPECT_EQ(false, actual.done);
  EXPECT_EQ("abcd-test-token", actual.rewrite_token);
  EXPECT_EQ(expected_resource, actual.resource);

  std::ostringstream os;
  os << actual;
  auto actual_str = os.str();
  EXPECT_THAT(actual_str, HasSubstr("total_bytes_rewritten=7"));
  EXPECT_THAT(actual_str, HasSubstr("object_size=42"));
  EXPECT_THAT(actual_str, HasSubstr("done=false"));
  EXPECT_THAT(actual_str, HasSubstr("rewrite_token=abcd-test-token"));
  EXPECT_THAT(actual_str, HasSubstr("test-object-name"));
}

TEST(ObjectRequestsTest, RewriteObjectResponseFailure) {
  std::string text = R"""({123)""";

  auto actual = RewriteObjectResponse::FromHttpResponse(text);
  EXPECT_THAT(actual, Not(IsOk()));
}

TEST(ObjectRequestsTest, RewriteObjectResponseFailureInResource) {
  std::string text = R"""({"resource": "invalid-resource"})""";

  auto actual = RewriteObjectResponse::FromHttpResponse(text);
  EXPECT_THAT(actual, Not(IsOk()));
}

TEST(ObjectRequestsTest, ResumableUpload) {
  ResumableUploadRequest request("source-bucket", "source-object");
  EXPECT_EQ("source-bucket", request.bucket_name());
  EXPECT_EQ("source-object", request.object_name());
  request.set_multiple_options(
      IfMetagenerationNotMatch(7), PredefinedAcl("private"),
      UserProject("my-project"),
      WithObjectMetadata(ObjectMetadata().set_content_type("text/plain")));

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("source-bucket"));
  EXPECT_THAT(actual, HasSubstr("source-object"));
  EXPECT_THAT(actual, HasSubstr("text/plain"));
  EXPECT_THAT(actual, HasSubstr("predefinedAcl=private"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
}

TEST(ObjectRequestsTest, DeleteResumableUpload) {
  DeleteResumableUploadRequest request("source-upload-session-url");
  EXPECT_EQ("source-upload-session-url", request.upload_session_url());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("source-upload-session-url"));
}

TEST(ObjectRequestsTest, UploadChunk) {
  std::string const url =
      "https://storage.googleapis.com/upload/storage/v1/b/"
      "myBucket/o?uploadType=resumable"
      "&upload_id=xa298sd_sdlkj2";
  UploadChunkRequest request(url, 0, {ConstBuffer{"abc123", 6}}, 2048);
  EXPECT_EQ(url, request.upload_session_url());
  EXPECT_EQ(0, request.range_begin());
  EXPECT_EQ(5, request.range_end());
  EXPECT_EQ(2048, request.source_size());
  EXPECT_EQ("Content-Range: bytes 0-5/2048", request.RangeHeader());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr(url));
  EXPECT_THAT(actual, HasSubstr("<Content-Range: bytes 0-5/2048>"));
}

TEST(ObjectRequestsTest, UploadChunkContentRangeNotLast) {
  std::string const url = "https://unused.googleapis.com/test-only";
  UploadChunkRequest request(url, 1024, {ConstBuffer{"1234", 4}});
  EXPECT_EQ("Content-Range: bytes 1024-1027/*", request.RangeHeader());
}

TEST(ObjectRequestsTest, UploadChunkContentRangeLast) {
  std::string const url = "https://unused.googleapis.com/test-only";
  UploadChunkRequest request(url, 2045, {ConstBuffer{"1234", 4}}, 2048U);
  EXPECT_EQ("Content-Range: bytes 2045-2048/2048", request.RangeHeader());
}

TEST(ObjectRequestsTest, UploadChunkContentRangeEmptyPayloadNotLast) {
  std::string const url = "https://unused.googleapis.com/test-only";
  UploadChunkRequest request(url, 1024, {});
  EXPECT_EQ("Content-Range: bytes */*", request.RangeHeader());
}

TEST(ObjectRequestsTest, UploadChunkContentRangeEmptyPayloadLast) {
  std::string const url = "https://unused.googleapis.com/test-only";
  UploadChunkRequest request(url, 2047, {}, 2048U);
  EXPECT_EQ("Content-Range: bytes */2048", request.RangeHeader());
}

TEST(ObjectRequestsTest, UploadChunkContentRangeEmptyPayloadEmpty) {
  std::string const url = "https://unused.googleapis.com/test-only";
  UploadChunkRequest r0(url, 1024, {}, 0U);
  EXPECT_EQ("Content-Range: bytes */0", r0.RangeHeader());
  UploadChunkRequest r1(url, 1024, {{}, {}, {}}, 0U);
  EXPECT_EQ("Content-Range: bytes */0", r1.RangeHeader());
}

TEST(ObjectRequestsTest, QueryResumableUpload) {
  std::string const url =
      "https://storage.googleapis.com/upload/storage/v1/b/"
      "myBucket/o?uploadType=resumable"
      "&upload_id=xa298sd_sdlkj2";
  QueryResumableUploadRequest request(url);
  EXPECT_EQ(url, request.upload_session_url());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr(url));
}

ObjectMetadata CreateObjectMetadataForTest() {
  // This metadata object has some impossible combination of fields in it. The
  // goal is to fully test the parsing, not to simulate valid objects.
  std::string text = R"""({
      "acl": [{
        "kind": "storage#objectAccessControl",
        "id": "acl-id-0",
        "selfLink": "https://storage.googleapis.com/storage/v1/b/foo-bar/o/baz/acl/user-qux",
        "bucket": "foo-bar",
        "object": "foo",
        "generation": 12345,
        "entity": "user-qux",
        "role": "OWNER",
        "email": "qux@example.com",
        "entityId": "user-qux-id-123",
        "domain": "example.com",
        "projectTeam": {
          "projectNumber": "4567",
          "team": "owners"
        },
        "etag": "AYX="
      }, {
        "kind": "storage#objectAccessControl",
        "id": "acl-id-1",
        "selfLink": "https://storage.googleapis.com/storage/v1/b/foo-bar/o/baz/acl/user-quux",
        "bucket": "foo-bar",
        "object": "foo",
        "generation": 12345,
        "entity": "user-quux",
        "role": "READER",
        "email": "qux@example.com",
        "entityId": "user-quux-id-123",
        "domain": "example.com",
        "projectTeam": {
          "projectNumber": "4567",
          "team": "viewers"
        },
        "etag": "AYX="
      }
      ],
      "bucket": "foo-bar",
      "cacheControl": "no-cache",
      "componentCount": 7,
      "contentDisposition": "a-disposition",
      "contentEncoding": "an-encoding",
      "contentLanguage": "a-language",
      "contentType": "application/octet-stream",
      "crc32c": "deadbeef",
      "customerEncryption": {
        "encryptionAlgorithm": "some-algo",
        "keySha256": "abc123"
      },
      "etag": "XYZ=",
      "generation": "12345",
      "id": "foo-bar/baz/12345",
      "kind": "storage#object",
      "kmsKeyName": "/foo/bar/baz/key",
      "md5Hash": "deaderBeef=",
      "mediaLink": "https://storage.googleapis.com/storage/v1/b/foo-bar/o/baz?generation=12345&alt=media",
      "metadata": {
        "foo": "bar",
        "baz": "qux"
      },
      "metageneration": "4",
      "name": "baz",
      "owner": {
        "entity": "user-qux",
        "entityId": "user-qux-id-123"
      },
      "selfLink": "https://storage.googleapis.com/storage/v1/b/foo-bar/o/baz",
      "size": 102400,
      "storageClass": "STANDARD",
      "timeCreated": "2018-05-19T19:31:14Z",
      "timeDeleted": "2018-05-19T19:32:24Z",
      "timeStorageClassUpdated": "2018-05-19T19:31:34Z",
      "updated": "2018-05-19T19:31:24Z"
})""";
  return internal::ObjectMetadataParser::FromString(text).value();
}

TEST(PatchObjectRequestTest, DiffSetAcl) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.set_acl({});
  ObjectMetadata updated = original;
  updated.set_acl({internal::ObjectAccessControlParser::FromString(
                       R"""({"entity": "user-test-user", "role": "OWNER"})""")
                       .value()});
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
      "acl": [{"entity": "user-test-user", "role": "OWNER"}]
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffResetAcl) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.set_acl({internal::ObjectAccessControlParser::FromString(
                        R"""({"entity": "user-test-user", "role": "OWNER"})""")
                        .value()});
  ObjectMetadata updated = original;
  updated.set_acl({});
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"acl": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffSetCacheControl) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.set_cache_control("");
  ObjectMetadata updated = original;
  updated.set_cache_control("no-cache");
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"cacheControl": "no-cache"})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffResetCacheControl) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.set_cache_control("no-cache");
  ObjectMetadata updated = original;
  updated.set_cache_control("");
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"cacheControl": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffSetContentDisposition) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.set_content_disposition("");
  ObjectMetadata updated = original;
  updated.set_content_disposition("test-value");
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  nlohmann::json expected =
      nlohmann::json::parse(R"""({"contentDisposition": "test-value"})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffResetContentDisposition) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.set_content_disposition("test-value");
  ObjectMetadata updated = original;
  updated.set_content_disposition("");
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"contentDisposition": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffSetContentEncoding) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.set_content_encoding("");
  ObjectMetadata updated = original;
  updated.set_content_encoding("test-value");
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  nlohmann::json expected =
      nlohmann::json::parse(R"""({"contentEncoding": "test-value"})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffResetContentEncoding) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.set_content_encoding("test-value");
  ObjectMetadata updated = original;
  updated.set_content_encoding("");
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"contentEncoding": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffSetContentLanguage) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.set_content_language("");
  ObjectMetadata updated = original;
  updated.set_content_language("test-value");
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  nlohmann::json expected =
      nlohmann::json::parse(R"""({"contentLanguage": "test-value"})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffResetContentLanguage) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.set_content_language("test-value");
  ObjectMetadata updated = original;
  updated.set_content_language("");
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"contentLanguage": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffSetContentType) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.set_content_type("");
  ObjectMetadata updated = original;
  updated.set_content_type("test-value");
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"contentType": "test-value"})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffResetContentType) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.set_content_type("test-value");
  ObjectMetadata updated = original;
  updated.set_content_type("");
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"contentType": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffSetEventBasedHold) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.set_event_based_hold(false);
  ObjectMetadata updated = original;
  updated.set_event_based_hold(true);
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"eventBasedHold": true})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffSetLabels) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.mutable_metadata() = {
      {"label1", "v1"},
      {"label2", "v2"},
  };
  ObjectMetadata updated = original;
  updated.mutable_metadata().erase("label2");
  updated.mutable_metadata().insert({"label3", "v3"});
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
      "metadata": {"label2": null, "label3": "v3"}
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffResetLabels) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.mutable_metadata() = {
      {"label1", "v1"},
      {"label2", "v2"},
  };
  ObjectMetadata updated = original;
  updated.mutable_metadata().clear();
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"metadata": null})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffSetTemporaryHold) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.set_temporary_hold(false);
  ObjectMetadata updated = original;
  updated.set_temporary_hold(true);
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({"temporaryHold": true})""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, Builder) {
  PatchObjectRequest request(
      "test-bucket", "test-object",
      ObjectMetadataPatchBuilder().SetContentType("application/json"));
  request.set_multiple_options(IfMetagenerationNotMatch(7),
                               UserProject("my-project"));
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test-object", request.object_name());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("test-bucket"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=7"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
  EXPECT_THAT(actual, HasSubstr("contentType"));
  EXPECT_THAT(actual, HasSubstr("application/json"));
}

TEST(ComposeObjectRequestTest, SimpleCompose) {
  ComposeSourceObject object1{"object1", {}, {}};
  ComposeSourceObject object2{"object2", {}, {}};
  object1.object_name = "object1";
  object1.generation.emplace(1L);
  object1.if_generation_match.emplace(1L);
  object2.object_name = "object2";
  object2.generation.emplace(2L);
  object2.if_generation_match.emplace(2L);
  std::vector<ComposeSourceObject> source_objects = {object1, object2};

  ComposeObjectRequest request("test-bucket", source_objects, "test-object");
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test-object", request.object_name());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("test-bucket"));
  EXPECT_THAT(actual, HasSubstr("test-object"));
  EXPECT_THAT(actual, HasSubstr("object1"));
  EXPECT_THAT(actual, HasSubstr("object2"));
  EXPECT_THAT(actual, HasSubstr("\"generation\":1"));
  EXPECT_THAT(actual, HasSubstr("\"generation\":2"));
  EXPECT_THAT(actual, HasSubstr("\"ifGenerationMatch\":1"));
  EXPECT_THAT(actual, HasSubstr("\"ifGenerationMatch\":2"));
}

TEST(DefaultCtorsWork, Trivial) {
  EXPECT_FALSE(ReadFromOffset().has_value());
  EXPECT_FALSE(ReadLast().has_value());
  EXPECT_FALSE(MD5HashValue().has_value());
  EXPECT_TRUE(DisableMD5Hash().has_value());
  EXPECT_FALSE(Crc32cChecksumValue().has_value());
  EXPECT_FALSE(DisableCrc32cChecksum().has_value());
  EXPECT_FALSE(WithObjectMetadata().has_value());
  EXPECT_FALSE(UseResumableUploadSession().has_value());
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

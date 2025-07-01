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

#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/internal/object_access_control_parser.h"
#include "google/cloud/storage/internal/object_acl_requests.h"
#include "google/cloud/storage/internal/object_metadata_parser.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOk;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

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
  request.set_multiple_options(UserProject("my-project"), Prefix("foo/"),
                               SoftDeleted(true));

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("my-bucket"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
  EXPECT_THAT(actual, HasSubstr("prefix=foo/"));
  EXPECT_THAT(actual, HasSubstr("softDeleted=true"));
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
  EXPECT_THAT(actual.items, ElementsAre(o1, o2));
  EXPECT_THAT(actual.prefixes, ElementsAre("foo/", "qux/"));
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
  request.set_multiple_options(Generation(1), IfMetagenerationMatch(3),
                               SoftDeleted(true));
  std::ostringstream os;
  os << request;
  auto str = os.str();
  EXPECT_THAT(str, HasSubstr("my-bucket"));
  EXPECT_THAT(str, HasSubstr("my-object"));
  EXPECT_THAT(str, HasSubstr("generation=1"));
  EXPECT_THAT(str, HasSubstr("ifMetagenerationMatch=3"));
  EXPECT_THAT(str, HasSubstr("softDeleted=true"));
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
  EXPECT_EQ("object contents", request.payload());
  request.set_payload("new contents");
  EXPECT_EQ("new contents", request.payload());
}

#include "google/cloud/internal/disable_deprecation_warnings.inc"
TEST(ObjectRequestsTest, InsertObjectBackwardsCompat) {
  auto const payload =
      std::string("The quick brown fox jumps over the lazy dog");
  auto const zebras = std::string("How quickly daft jumping zebras vex");
  InsertObjectMediaRequest request("my-bucket", "my-object", payload);
  EXPECT_EQ(payload, request.payload());
  EXPECT_EQ(payload, request.contents());
  request.set_contents(zebras);
  EXPECT_EQ(zebras, request.payload());
  EXPECT_EQ(zebras, request.contents());
}
#include "google/cloud/internal/diagnostics_pop.inc"

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

TEST(ObjectRequestsTest, CopyAllOptions) {
  CopyObjectRequest request("source-bucket", "source-object", "my-bucket",
                            "my-object");
  EXPECT_EQ("source-bucket", request.source_bucket());
  EXPECT_EQ("source-object", request.source_object());
  EXPECT_EQ("my-bucket", request.destination_bucket());
  EXPECT_EQ("my-object", request.destination_object());
  request.set_multiple_options(
      DestinationKmsKeyName("test-only-kms-key"),
      DestinationPredefinedAcl("private"),
      EncryptionKey::FromBinaryKey("1234ABCD"), IfGenerationMatch(1),
      IfGenerationNotMatch(2), IfMetagenerationMatch(3),
      IfMetagenerationNotMatch(4), IfSourceGenerationMatch(5),
      IfSourceGenerationNotMatch(6), IfSourceMetagenerationMatch(7),
      IfSourceMetagenerationNotMatch(8), Projection("full"),
      SourceGeneration(7), SourceEncryptionKey::FromBinaryKey("ABCD1234"),
      UserProject("my-project"),
      WithObjectMetadata(ObjectMetadata().set_content_type("text/plain")));

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("my-bucket"));
  EXPECT_THAT(actual, HasSubstr("my-object"));
  EXPECT_THAT(actual, HasSubstr("source-bucket"));
  EXPECT_THAT(actual, HasSubstr("=source-object"));
  EXPECT_THAT(actual, HasSubstr("destinationKmsKeyName=test-only-kms-key"));
  EXPECT_THAT(actual, HasSubstr("destinationPredefinedAcl=private"));
  EXPECT_THAT(actual, HasSubstr("x-goog-encryption-algorithm: AES256"));
  // /bin/echo -n ABCD1234 | openssl base64 -e
  EXPECT_THAT(actual, HasSubstr("x-goog-encryption-key: MTIzNEFCQ0Q="));
  // /bin/echo -n 1234ABCD | sha256sum | awk '{printf("%s", $1);}' |
  //     xxd -r -p | openssl base64
  EXPECT_THAT(actual,
              HasSubstr("x-goog-encryption-key-sha256: "
                        "xBECBA30JV48aHcnGxXLZMs2dEryI1CA+PZg8ODIRRk="));
  EXPECT_THAT(actual, HasSubstr("ifGenerationMatch=1"));
  EXPECT_THAT(actual, HasSubstr("ifGenerationNotMatch=2"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationMatch=3"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=4"));
  EXPECT_THAT(actual, HasSubstr("ifSourceGenerationMatch=5"));
  EXPECT_THAT(actual, HasSubstr("ifSourceGenerationNotMatch=6"));
  EXPECT_THAT(actual, HasSubstr("ifSourceMetagenerationMatch=7"));
  EXPECT_THAT(actual, HasSubstr("ifSourceMetagenerationNotMatch=8"));
  EXPECT_THAT(actual, HasSubstr("projection=full"));
  EXPECT_THAT(actual, HasSubstr("sourceGeneration=7"));
  EXPECT_THAT(actual,
              HasSubstr("x-goog-copy-source-encryption-algorithm: AES256"));
  // /bin/echo -n ABCD1234 | openssl base64 -e
  EXPECT_THAT(actual,
              HasSubstr("x-goog-copy-source-encryption-key: QUJDRDEyMzQ="));
  // /bin/echo -n ABCD1234 | sha256sum | awk '{printf("%s", $1);}' |
  //     xxd -r -p | openssl base64
  EXPECT_THAT(actual,
              HasSubstr("x-goog-copy-source-encryption-key-sha256: "
                        "FjXIUlr7rljDe+3jyUQIROkUNyfMfBYL7WZew3jYomI="));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
  EXPECT_THAT(actual, HasSubstr("text/plain"));
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
  request.set_multiple_options(
      Generation(7), EncryptionKey::FromBinaryKey("1234ABCD"),
      IfGenerationMatch(1), IfGenerationNotMatch(2), IfMetagenerationMatch(3),
      IfMetagenerationNotMatch(4), PredefinedAcl("private"),
      UserProject("my-project"));
  std::ostringstream os;
  os << request;
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("my-bucket"));
  EXPECT_THAT(actual, HasSubstr("my-object"));
  EXPECT_THAT(actual, HasSubstr("content_type=application/json"));
  EXPECT_THAT(actual, HasSubstr("generation=7"));
  EXPECT_THAT(actual, HasSubstr("x-goog-encryption-algorithm: AES256"));
  // /bin/echo -n 1234ABCD | openssl base64 -e
  EXPECT_THAT(actual, HasSubstr("x-goog-encryption-key: MTIzNEFCQ0Q="));
  // /bin/echo -n 1234ABCD | sha256sum | awk '{printf("%s", $1);}' |
  //     xxd -r -p | openssl base64
  EXPECT_THAT(actual,
              HasSubstr("x-goog-encryption-key-sha256: "
                        "xBECBA30JV48aHcnGxXLZMs2dEryI1CA+PZg8ODIRRk="));
  EXPECT_THAT(actual, HasSubstr("ifGenerationMatch=1"));
  EXPECT_THAT(actual, HasSubstr("ifGenerationNotMatch=2"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationMatch=3"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=4"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=4"));
  EXPECT_THAT(actual, HasSubstr("predefinedAcl=private"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
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

TEST(ObjectRequestsTest, RestoreObject) {
  RestoreObjectRequest request("test-bucket", "test-object", 1234);
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test-object", request.object_name());
  EXPECT_EQ(1234, request.generation());
  request.set_multiple_options(IfGenerationMatch(7), IfGenerationNotMatch(8),
                               IfMetagenerationMatch(9),
                               IfMetagenerationNotMatch(10),
                               UserProject("my-project"), CopySourceAcl(true));

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("test-bucket"));
  EXPECT_THAT(actual, HasSubstr("test-object"));
  EXPECT_THAT(actual, HasSubstr("generation=1234"));
  EXPECT_THAT(actual, HasSubstr("ifGenerationMatch=7"));
  EXPECT_THAT(actual, HasSubstr("ifGenerationNotMatch=8"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationMatch=9"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=10"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
  EXPECT_THAT(actual, HasSubstr("copySourceAcl=true"));
}

TEST(ObjectRequestsTest, MoveObject) {
  MoveObjectRequest request("test-bucket", "source-object-name",
                            "destination-object-name");
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("source-object-name", request.source_object_name());
  EXPECT_EQ("destination-object-name", request.destination_object_name());
  request.set_multiple_options(
      IfGenerationMatch(1), IfGenerationNotMatch(2), IfMetagenerationMatch(3),
      IfMetagenerationNotMatch(4), IfSourceGenerationMatch(5),
      IfSourceGenerationNotMatch(6), IfSourceMetagenerationMatch(7),
      IfSourceMetagenerationNotMatch(8));
  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("test-bucket"));
  EXPECT_THAT(actual, HasSubstr("source-object-name"));
  EXPECT_THAT(actual, HasSubstr("destination-object-name"));
  EXPECT_THAT(actual, HasSubstr("ifGenerationMatch=1"));
  EXPECT_THAT(actual, HasSubstr("ifGenerationNotMatch=2"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationMatch=3"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=4"));
  EXPECT_THAT(actual, HasSubstr("ifSourceGenerationMatch=5"));
  EXPECT_THAT(actual, HasSubstr("ifSourceGenerationNotMatch=6"));
  EXPECT_THAT(actual, HasSubstr("ifSourceMetagenerationMatch=7"));
  EXPECT_THAT(actual, HasSubstr("ifSourceMetagenerationNotMatch=8"));
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
  auto const payload = std::string(2048, 'A');
  UploadChunkRequest request(url, 0, {{payload}}, CreateNullHashFunction(),
                             HashValues{});
  EXPECT_EQ(url, request.upload_session_url());
  EXPECT_EQ(0, request.offset());
  EXPECT_EQ(2048, request.upload_size().value_or(0));
  EXPECT_EQ("Content-Range: bytes 0-2047/2048", request.RangeHeader());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr(url));
  EXPECT_THAT(actual, HasSubstr("<Content-Range: bytes 0-2047/2048>"));
}

TEST(ObjectRequestsTest, UploadChunkRemainingChunk) {
  auto const p0 = std::string(128, '0');
  auto const p1 = std::string(256, '1');
  auto const p2 = std::string(1024, '2');
  auto const base_offset = 123456;
  auto request = UploadChunkRequest("unused", base_offset, {{p0, p1, p2}},
                                    CreateNullHashFunction());
  EXPECT_EQ(request.offset(), base_offset);
  EXPECT_THAT(request.payload(), ElementsAre(p0, p1, p2));
  auto remaining = request.RemainingChunk(base_offset + 42);
  EXPECT_THAT(remaining.payload(), ElementsAre(p0.substr(42), p1, p2));
  remaining = request.RemainingChunk(base_offset + 128 + 42);
  EXPECT_THAT(remaining.payload(), ElementsAre(p1.substr(42), p2));
  remaining = request.RemainingChunk(base_offset + 128 + 256 + 42);
  EXPECT_THAT(remaining.payload(), ElementsAre(p2.substr(42)));
}

TEST(ObjectRequestsTest, UploadChunkContentRangeNotLast) {
  std::string const url = "https://unused.googleapis.com/test-only";
  UploadChunkRequest request(url, 1024, {ConstBuffer{"1234", 4}},
                             CreateNullHashFunction());
  EXPECT_EQ("Content-Range: bytes 1024-1027/*", request.RangeHeader());
}

TEST(ObjectRequestsTest, UploadChunkContentRangeLast) {
  std::string const url = "https://unused.googleapis.com/test-only";
  UploadChunkRequest request(url, 2045, {ConstBuffer{"1234", 4}},
                             CreateNullHashFunction(), HashValues{});
  EXPECT_EQ("Content-Range: bytes 2045-2048/2049", request.RangeHeader());
}

TEST(ObjectRequestsTest, UploadChunkContentRangeEmptyPayloadNotLast) {
  std::string const url = "https://unused.googleapis.com/test-only";
  UploadChunkRequest request(url, 1024, {}, CreateNullHashFunction());
  EXPECT_EQ("Content-Range: bytes */*", request.RangeHeader());
}

TEST(ObjectRequestsTest, UploadChunkContentRangeEmptyPayloadLast) {
  std::string const url = "https://unused.googleapis.com/test-only";
  UploadChunkRequest request(url, 2047, {}, CreateNullHashFunction(),
                             HashValues{});
  EXPECT_EQ("Content-Range: bytes */2047", request.RangeHeader());
}

TEST(ObjectRequestsTest, UploadChunkContentRangeEmptyPayloadEmpty) {
  std::string const url = "https://unused.googleapis.com/test-only";
  UploadChunkRequest r0(url, 1024, {}, CreateNullHashFunction(), HashValues{});
  EXPECT_EQ("Content-Range: bytes */1024", r0.RangeHeader());
  UploadChunkRequest r1(url, 1024, {{}, {}, {}}, CreateNullHashFunction(),
                        HashValues{});
  EXPECT_EQ("Content-Range: bytes */1024", r1.RangeHeader());
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

TEST(PatchObjectRequestTest, DiffSetMetadata) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.mutable_metadata() = {
      {"meta1", "v1"},
      {"meta2", "v2"},
  };
  ObjectMetadata updated = original;
  updated.mutable_metadata().erase("meta2");
  updated.mutable_metadata().insert({"meta3", "v3"});
  PatchObjectRequest request("test-bucket", "test-object", original, updated);

  auto patch = nlohmann::json::parse(request.payload());
  auto expected = nlohmann::json::parse(R"""({
      "metadata": {"meta2": null, "meta3": "v3"}
  })""");
  EXPECT_EQ(expected, patch);
}

TEST(PatchObjectRequestTest, DiffResetMetadata) {
  ObjectMetadata original = CreateObjectMetadataForTest();
  original.mutable_metadata() = {
      {"meta1", "v1"},
      {"meta2", "v2"},
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
  request.set_multiple_options(
      Generation(7), IfGenerationMatch(1), IfGenerationNotMatch(2),
      IfMetagenerationMatch(3), IfMetagenerationNotMatch(4),
      PredefinedAcl::ProjectPrivate(), EncryptionKey::FromBinaryKey("ABCD1234"),
      UserProject("my-project"));
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test-object", request.object_name());

  std::ostringstream os;
  os << request;
  std::string actual = os.str();
  EXPECT_THAT(actual, HasSubstr("test-bucket"));
  EXPECT_THAT(actual, HasSubstr("test-object"));
  EXPECT_THAT(actual, HasSubstr("generation=7"));
  EXPECT_THAT(actual, HasSubstr("ifGenerationMatch=1"));
  EXPECT_THAT(actual, HasSubstr("ifGenerationNotMatch=2"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationMatch=3"));
  EXPECT_THAT(actual, HasSubstr("ifMetagenerationNotMatch=4"));
  EXPECT_THAT(actual, HasSubstr("predefinedAcl=projectPrivate"));
  EXPECT_THAT(actual, HasSubstr("userProject=my-project"));
  EXPECT_THAT(actual, HasSubstr("contentType"));
  EXPECT_THAT(actual, HasSubstr("application/json"));
  EXPECT_THAT(actual, HasSubstr("x-goog-encryption-algorithm: AES256"));
  // /bin/echo -n ABCD1234 | sha256sum #openssl base64 -e
  EXPECT_THAT(actual, HasSubstr("x-goog-encryption-key: QUJDRDEyMzQ="));
  // /bin/echo -n ABCD1234 | sha256sum | awk '{printf("%s", $1);}' |
  //     xxd -r -p | openssl base64
  EXPECT_THAT(actual,
              HasSubstr("x-goog-encryption-key-sha256: "
                        "FjXIUlr7rljDe+3jyUQIROkUNyfMfBYL7WZew3jYomI="));
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

TEST(CreateResumableUploadResponseTest, Base) {
  auto actual = CreateResumableUploadResponse::FromHttpResponse(
                    HttpResponse{200,
                                 R"""({"name": "test-object-name"})""",
                                 {
                                     {"ignored-header", "value"},
                                     {"location", "location-value"},
                                 }})
                    .value();
  EXPECT_EQ("location-value", actual.upload_id);

  std::ostringstream os;
  os << actual;
  auto actual_str = os.str();
  EXPECT_THAT(actual_str, HasSubstr("upload_id=location-value"));
}

TEST(CreateResumableUploadResponseTest, NoLocation) {
  auto actual = CreateResumableUploadResponse::FromHttpResponse(
      HttpResponse{201,
                   R"""({"name": "test-object-name"})""",
                   {{"uh-oh", "location-value"}}});
  EXPECT_THAT(actual, Not(IsOk()));
}

TEST(QueryResumableUploadResponseTest, Base) {
  auto actual = QueryResumableUploadResponse::FromHttpResponse(
                    HttpResponse{200,
                                 R"""({"name": "test-object-name"})""",
                                 {{"ignored-header", "value"},
                                  {"location", "location-value"},
                                  {"range", "bytes=0-1999"}}})
                    .value();
  ASSERT_TRUE(actual.payload.has_value());
  EXPECT_EQ("test-object-name", actual.payload->name());
  EXPECT_EQ(2000, actual.committed_size.value_or(0));
  EXPECT_THAT(actual.request_metadata,
              UnorderedElementsAre(Pair("ignored-header", "value"),
                                   Pair("location", "location-value"),
                                   Pair("range", "bytes=0-1999")));

  std::ostringstream os;
  os << actual;
  auto actual_str = os.str();
  EXPECT_THAT(actual_str, HasSubstr("committed_size=2000"));
}

TEST(QueryResumableUploadResponseTest, NoRange) {
  auto actual = QueryResumableUploadResponse::FromHttpResponse(
                    HttpResponse{201,
                                 R"""({"name": "test-object-name"})""",
                                 {{"location", "location-value"}}})
                    .value();
  ASSERT_TRUE(actual.payload.has_value());
  EXPECT_EQ("test-object-name", actual.payload->name());
  EXPECT_FALSE(actual.committed_size.has_value());
}

TEST(QueryResumableUploadResponseTest, MissingBytesInRange) {
  auto actual = QueryResumableUploadResponse::FromHttpResponse(HttpResponse{
      308, {}, {{"location", "location-value"}, {"range", "units=0-2000"}}});
  EXPECT_THAT(actual,
              StatusIs(StatusCode::kInternal, HasSubstr("units=0-2000")));
}

TEST(QueryResumableUploadResponseTest, MissingRangeEnd) {
  auto actual = QueryResumableUploadResponse::FromHttpResponse(
      HttpResponse{308, {}, {{"range", "bytes=0-"}}});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInternal, HasSubstr("bytes=0-")));
}

TEST(QueryResumableUploadResponseTest, InvalidRangeEnd) {
  auto actual = QueryResumableUploadResponse::FromHttpResponse(
      HttpResponse{308, {}, {{"range", "bytes=0-abcd"}}});
  EXPECT_THAT(actual,
              StatusIs(StatusCode::kInternal, HasSubstr("bytes=0-abcd")));
}

TEST(QueryResumableUploadResponseTest, InvalidRangeBegin) {
  auto actual = QueryResumableUploadResponse::FromHttpResponse(
      HttpResponse{308, {}, {{"range", "bytes=abcd-2000"}}});
  EXPECT_THAT(actual,
              StatusIs(StatusCode::kInternal, HasSubstr("bytes=abcd-2000")));
}

TEST(QueryResumableUploadResponseTest, UnexpectedRangeBegin) {
  auto actual = QueryResumableUploadResponse::FromHttpResponse(
      HttpResponse{308, {}, {{"range", "bytes=3000-2000"}}});
  EXPECT_THAT(actual,
              StatusIs(StatusCode::kInternal, HasSubstr("bytes=3000-2000")));
}

TEST(QueryResumableUploadResponseTest, NegativeEnd) {
  auto actual = QueryResumableUploadResponse::FromHttpResponse(
      HttpResponse{308, {}, {{"range", "bytes=0--7"}}});
  EXPECT_THAT(actual, StatusIs(StatusCode::kInternal, HasSubstr("bytes=0--7")));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

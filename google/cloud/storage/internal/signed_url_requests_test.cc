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

#include "google/cloud/storage/internal/signed_url_requests.h"
#include "google/cloud/storage/internal/format_time_point.h"
#include "google/cloud/storage/internal/parse_rfc3339.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::testing::HasSubstr;

TEST(V2SignedUrlRequests, Sign) {
  V2SignUrlRequest request("GET", "test-bucket", "test-object");
  EXPECT_EQ("GET", request.verb());
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test-object", request.object_name());

  request.set_multiple_options(
      ExpirationTime(ParseRfc3339("2018-12-03T12:00:00Z")),
      MD5HashValue("rmYdCNHKFXam78uCt7xQLw=="), ContentType("text/plain"),
      AddExtensionHeader("x-goog-meta-foo", "bar"),
      AddExtensionHeader("x-goog-meta-foo", "baz"),
      AddExtensionHeader("x-goog-encryption-algorithm", "AES256"),
      AddExtensionHeader("x-goog-acl", "project-private"),
      WithUserProject("test-project"),
      SigningAccount("another-account@example.com"),
      SigningAccountDelegates({"test-delegate1", "test-delegate2"}));

  // Used this command to get the date in seconds:
  //   date +%s -u --date=2018-12-03T12:00:00Z
  std::string expected_blob = R"""(GET
rmYdCNHKFXam78uCt7xQLw==
text/plain
1543838400
x-goog-acl:project-private
x-goog-encryption-algorithm:AES256
x-goog-meta-foo:bar,baz
/test-bucket/test-object?userProject=test-project)""";

  EXPECT_EQ(expected_blob, request.StringToSign());
}

TEST(V2SignedUrlRequests, SigningAccount) {
  V2SignUrlRequest request("GET", "test-bucket", "test-object");
  EXPECT_EQ("GET", request.verb());
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test-object", request.object_name());
  EXPECT_FALSE(request.signing_account().has_value());
  EXPECT_FALSE(request.signing_account_delegates().has_value());

  request.set_multiple_options(
      ExpirationTime(ParseRfc3339("2018-12-03T12:00:00Z")),
      MD5HashValue("rmYdCNHKFXam78uCt7xQLw=="), ContentType("text/plain"),
      AddExtensionHeader("x-goog-meta-foo", "bar"),
      AddExtensionHeader("x-goog-meta-foo", "baz"),
      AddExtensionHeader("x-goog-encryption-algorithm", "AES256"),
      AddExtensionHeader("x-goog-acl", "project-private"),
      WithUserProject("test-project"),
      SigningAccount("another-account@example.com"),
      SigningAccountDelegates({"test-delegate1", "test-delegate2"}));

  ASSERT_TRUE(request.signing_account().has_value());
  ASSERT_TRUE(request.signing_account_delegates().has_value());
  EXPECT_EQ("another-account@example.com", request.signing_account().value());
  EXPECT_THAT(request.signing_account_delegates().value(),
              ::testing::ElementsAre("test-delegate1", "test-delegate2"));
}

TEST(V2SignedUrlRequests, SignEscaped) {
  V2SignUrlRequest request("GET", "test-bucket", "test- -?-+-/-:-&-object");
  EXPECT_EQ("GET", request.verb());
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test- -?-+-/-:-&-object", request.object_name());

  request.set_multiple_options(
      ExpirationTime(ParseRfc3339("2018-12-03T12:00:00Z")),
      WithMarker("foo+bar"));

  // Used this command to get the date in seconds:
  //   date +%s -u --date=2018-12-03T12:00:00Z
  std::string expected_blob = R"""(GET


1543838400
/test-bucket/test-%20-%3F-%2B-%2F-%3A-%26-object?marker=foo%2Bbar)""";

  EXPECT_EQ(expected_blob, request.StringToSign());

  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr(expected_blob));
}

TEST(V2SignedUrlRequests, SubResource) {
  V2SignUrlRequest request("GET", "test-bucket", "test-object");
  EXPECT_EQ("GET", request.verb());
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test-object", request.object_name());

  request.set_multiple_options(
      WithAcl(), ExpirationTime(ParseRfc3339("2019-02-26T13:14:15Z")));

  EXPECT_EQ("acl", request.sub_resource());

  // The magic seconds where found using:
  //     date +%s -u --date=2019-02-26T13:14:15Z
  //
  std::string expected_blob = R"""(GET


1551186855
/test-bucket/test-object?acl)""";

  EXPECT_EQ(expected_blob, request.StringToSign());

  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr(expected_blob));
}

TEST(V2SignedUrlRequests, SubResourceMultiple) {
  V2SignUrlRequest request("GET", "test-bucket", "");
  EXPECT_EQ("GET", request.verb());
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("", request.object_name());

  request.set_multiple_options(
      WithAcl(), WithBilling(),
      ExpirationTime(ParseRfc3339("2019-02-26T13:14:15Z")));

  EXPECT_EQ("billing", request.sub_resource());

  // The magic seconds where found using:
  //     date +%s -u --date=2019-02-26T13:14:15Z
  //
  std::string expected_blob = R"""(GET


1551186855
/test-bucket?billing)""";

  EXPECT_EQ(expected_blob, request.StringToSign());
}

TEST(V2SignedUrlRequests, RepeatedHeader) {
  V2SignUrlRequest request("PUT", "test-bucket", "test-object");
  EXPECT_EQ("PUT", request.verb());
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test-object", request.object_name());

  request.set_multiple_options(
      WithTagging(), ExpirationTime(ParseRfc3339("2019-02-26T13:14:15Z")),
      AddExtensionHeader("X-Goog-Meta-Reviewer", "test-meta-1"),
      AddExtensionHeader("x-goog-meta-reviewer", "not-encoded- -?-+-/-:-&-"));

  EXPECT_EQ("tagging", request.sub_resource());

  // The magic seconds where found using:
  //     date +%s -u --date=2019-02-26T13:14:15Z
  //
  std::string expected_blob = R"""(PUT


1551186855
x-goog-meta-reviewer:test-meta-1,not-encoded- -?-+-/-:-&-
/test-bucket/test-object?tagging)""";

  EXPECT_EQ(expected_blob, request.StringToSign());
}

TEST(V2SignedUrlRequests, EncodeQueryParameter) {
  V2SignUrlRequest request("GET", "test-bucket", "test-object.txt");
  EXPECT_EQ("GET", request.verb());
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test-object.txt", request.object_name());

  request.set_multiple_options(
      ExpirationTime(ParseRfc3339("2019-02-26T13:14:15Z")),
      WithResponseContentType("text/html"));

  EXPECT_EQ("", request.sub_resource());

  // The magic seconds where found using:
  //     date +%s -u --date=2019-02-26T13:14:15Z
  //
  std::string expected_blob = R"""(GET


1551186855
/test-bucket/test-object.txt?response-content-type=text%2Fhtml)""";

  EXPECT_EQ(expected_blob, request.StringToSign());
}

TEST(V4SignUrlRequest, SigningAccount) {
  V4SignUrlRequest request("GET", "test-bucket", "test-object");
  EXPECT_EQ("GET", request.verb());
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test-object", request.object_name());
  EXPECT_FALSE(request.signing_account().has_value());
  EXPECT_FALSE(request.signing_account_delegates().has_value());

  std::string const date = "2019-02-01T09:00:00Z";
  auto const valid_for = std::chrono::seconds(10);
  request.set_multiple_options(
      SignedUrlTimestamp(ParseRfc3339(date)), SignedUrlDuration(valid_for),
      SigningAccount("another-account@example.com"),
      SigningAccountDelegates({"test-delegate1", "test-delegate2"}));
  ASSERT_TRUE(request.signing_account().has_value());
  ASSERT_TRUE(request.signing_account_delegates().has_value());
  EXPECT_EQ("another-account@example.com", request.signing_account().value());
  EXPECT_THAT(request.signing_account_delegates().value(),
              ::testing::ElementsAre("test-delegate1", "test-delegate2"));
}

TEST(V4SignedUrlRequests, CanonicalQueryStringBasic) {
  V4SignUrlRequest request("GET", "test-bucket", "test-object");
  std::string const date = "2019-02-01T09:00:00Z";
  auto const valid_for = std::chrono::seconds(10);
  request.set_multiple_options(SignedUrlTimestamp(ParseRfc3339(date)),
                               SignedUrlDuration(valid_for));

  std::string expected =
      "X-Goog-Algorithm=GOOG4-RSA-SHA256"
      "&X-Goog-Credential=fake-client-id"
      "%2F20190201%2Fauto%2Fstorage%2Fgoog4_request"
      "&X-Goog-Date=20190201T090000Z"
      "&X-Goog-Expires=10&X-Goog-SignedHeaders=";
  std::string actual = request.CanonicalQueryString("fake-client-id");
  EXPECT_EQ(expected, actual);
}

TEST(V4SignedUrlRequests, CanonicalQueryStringSingleHeader) {
  V4SignUrlRequest request("GET", "test-bucket", "test-object");
  std::string const date = "2019-02-01T09:00:00Z";
  auto const valid_for = std::chrono::seconds(10);
  request.set_multiple_options(
      SignedUrlTimestamp(ParseRfc3339(date)), SignedUrlDuration(valid_for),
      AddExtensionHeader("host", "storage.googleapis.com"));

  std::string expected =
      "X-Goog-Algorithm=GOOG4-RSA-SHA256"
      "&X-Goog-Credential=fake-client-id"
      "%2F20190201%2Fauto%2Fstorage%2Fgoog4_request"
      "&X-Goog-Date=20190201T090000Z"
      "&X-Goog-Expires=10&X-Goog-SignedHeaders=host";
  std::string actual = request.CanonicalQueryString("fake-client-id");
  EXPECT_EQ(expected, actual);
}

TEST(V4SignedUrlRequests, CanonicalQueryStringMultiHeader) {
  V4SignUrlRequest request("GET", "test-bucket", "test-object");
  std::string const date = "2019-02-01T09:00:00Z";
  auto const valid_for = std::chrono::seconds(10);
  request.set_multiple_options(
      SignedUrlTimestamp(ParseRfc3339(date)), SignedUrlDuration(valid_for),
      WithUserProject("test-project"),
      AddExtensionHeader("host", "storage.googleapis.com"), WithGeneration(7),
      AddExtensionHeader("Content-Type", "application/octet-stream"));

  std::string expected =
      "X-Goog-Algorithm=GOOG4-RSA-SHA256"
      "&X-Goog-Credential=fake-client-id"
      "%2F20190201%2Fauto%2Fstorage%2Fgoog4_request"
      "&X-Goog-Date=20190201T090000Z"
      "&X-Goog-Expires=10&X-Goog-SignedHeaders=content-type%3Bhost";
  std::string actual = request.CanonicalQueryString("fake-client-id");
  EXPECT_EQ(expected, actual);
}

TEST(V4SignedUrlRequests, CanonicalRequestBasic) {
  V4SignUrlRequest request("GET", "test-bucket", "test-object");
  std::string const date = "2019-02-01T09:00:00Z";
  auto const valid_for = std::chrono::seconds(10);
  request.set_multiple_options(SignedUrlTimestamp(ParseRfc3339(date)),
                               SignedUrlDuration(valid_for));

  EXPECT_EQ("GET", request.verb());
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test-object", request.object_name());
  EXPECT_EQ("", request.sub_resource());
  EXPECT_EQ("20190201T090000Z",
            FormatV4SignedUrlTimestamp(request.timestamp()));
  EXPECT_EQ(valid_for, request.expires());

  std::string expected_canonical_request = R"""(GET
/test-bucket/test-object
X-Goog-Algorithm=GOOG4-RSA-SHA256&X-Goog-Credential=fake-client-id%2F20190201%2Fauto%2Fstorage%2Fgoog4_request&X-Goog-Date=20190201T090000Z&X-Goog-Expires=10&X-Goog-SignedHeaders=


UNSIGNED-PAYLOAD)""";

  EXPECT_EQ(expected_canonical_request,
            request.CanonicalRequest("fake-client-id"));

  // To obtain the magical SHA256 string below pipe the
  // `expected_canonical_request` string to `openssl sha256 -hex`. Do *NOT*
  // include a trailing newline in your command.
  std::string expected_string_to_sign = R"""(GOOG4-RSA-SHA256
20190201T090000Z
20190201/auto/storage/goog4_request
b4587b273770141b7b36c3a6b7c5bf7a3265ae0933d83785a6f78253fb0da70d)""";

  EXPECT_EQ(expected_string_to_sign, request.StringToSign("fake-client-id"));

  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("/test-bucket/test-object"));
}

TEST(V4SignedUrlRequests, CanonicalRequestFull) {
  V4SignUrlRequest request("GET", "test-bucket", "test-object");
  std::string const date = "2019-02-01T09:00:00Z";
  auto const valid_for = std::chrono::seconds(10);
  request.set_multiple_options(
      SignedUrlTimestamp(ParseRfc3339(date)), SignedUrlDuration(valid_for),
      WithUserProject("test-project"), WithGeneration(7),
      AddExtensionHeader("Content-Type", "application/octet-stream"),
      AddExtensionHeader("Cache-Control", "    no-cache,    max-age=3600   "),
      WithAcl());

  EXPECT_EQ("GET", request.verb());
  EXPECT_EQ("test-bucket", request.bucket_name());
  EXPECT_EQ("test-object", request.object_name());
  EXPECT_EQ("acl", request.sub_resource());
  EXPECT_EQ("20190201T090000Z",
            FormatV4SignedUrlTimestamp(request.timestamp()));
  EXPECT_EQ(valid_for, request.expires());

  std::string expected_canonical_request = R"""(GET
/test-bucket/test-object?acl
X-Goog-Algorithm=GOOG4-RSA-SHA256&X-Goog-Credential=fake-client-id%2F20190201%2Fauto%2Fstorage%2Fgoog4_request&X-Goog-Date=20190201T090000Z&X-Goog-Expires=10&X-Goog-SignedHeaders=cache-control%3Bcontent-type&generation=7&userProject=test-project
cache-control:no-cache, max-age=3600
content-type:application/octet-stream

cache-control;content-type
UNSIGNED-PAYLOAD)""";

  EXPECT_EQ(expected_canonical_request,
            request.CanonicalRequest("fake-client-id"));

  // To obtain the magical SHA256 string below pipe the
  // `expected_canonical_request` string to `openssl sha256 -hex`. Do *NOT*
  // include a trailing newline in your command.
  std::string expected_string_to_sign = R"""(GOOG4-RSA-SHA256
20190201T090000Z
20190201/auto/storage/goog4_request
b6d09f3be351906e01e472adaad90398a37c1c69bfe82ad5cb9ba32d66dac850)""";

  EXPECT_EQ(expected_string_to_sign, request.StringToSign("fake-client-id"));

  std::ostringstream os;
  os << request;
  EXPECT_THAT(os.str(), HasSubstr("/test-bucket/test-object"));
}

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

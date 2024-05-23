// Copyright 2019 Google LLC
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

#include "google/cloud/storage/internal/policy_document_request.h"
#include "google/cloud/internal/parse_rfc3339.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::google::cloud::testing_util::IsOkAndHolds;
using ::google::cloud::testing_util::StatusIs;
using ::testing::ElementsAre;
using ::testing::StartsWith;

TEST(PolicyDocumentRequest, SigningAccount) {
  PolicyDocumentRequest request;
  EXPECT_FALSE(request.signing_account().has_value());
  EXPECT_FALSE(request.signing_account_delegates().has_value());

  request.set_multiple_options(
      SigningAccount("another-account@example.com"),
      SigningAccountDelegates({"test-delegate1", "test-delegate2"}));
  ASSERT_TRUE(request.signing_account().has_value());
  ASSERT_TRUE(request.signing_account_delegates().has_value());
  EXPECT_EQ("another-account@example.com", request.signing_account().value());
  EXPECT_THAT(request.signing_account_delegates().value(),
              ElementsAre("test-delegate1", "test-delegate2"));
}

TEST(PolicyDocumentV4Request, SigningAccount) {
  PolicyDocumentV4Request request;
  EXPECT_FALSE(request.signing_account().has_value());
  EXPECT_FALSE(request.signing_account_delegates().has_value());

  request.set_multiple_options(
      SigningAccount("another-account@example.com"),
      SigningAccountDelegates({"test-delegate1", "test-delegate2"}));
  ASSERT_TRUE(request.signing_account().has_value());
  ASSERT_TRUE(request.signing_account_delegates().has_value());
  EXPECT_EQ("another-account@example.com", request.signing_account().value());
  EXPECT_THAT(request.signing_account_delegates().value(),
              ElementsAre("test-delegate1", "test-delegate2"));
}

TEST(PostPolicyV4EscapeTest, OnlyAscii) {
  EXPECT_THAT(PostPolicyV4Escape("\\\b\f\n\r\t\vabcd"),
              IsOkAndHolds("\\\\b\\f\\n\\r\\t\\vabcd"));
}

TEST(PostPolicyV4EscapeTest, InvalidUtf) {
  // In UTF8 a byte larger than 0x7f indicates that there is a byte following
  // it. Here we truncate the string to cause the UTF parser to fail.
  std::string invalid_utf8(1, '\x80');
  EXPECT_THAT(PostPolicyV4Escape(invalid_utf8),
              StatusIs(StatusCode::kInvalidArgument));

  // Missing characters in encoding
  EXPECT_THAT(PostPolicyV4Escape("\xC2"),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(PostPolicyV4Escape("\xED\x95"),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(PostPolicyV4Escape("\xF0\x90\x8D"),
              StatusIs(StatusCode::kInvalidArgument));

  // Encoding with invalid characters
  EXPECT_THAT(PostPolicyV4Escape("\xC2\x20"),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(PostPolicyV4Escape("\xED\x95\x20"),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(PostPolicyV4Escape("\xF0\x90\x8D\x20"),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(PostPolicyV4Escape("\xFF"),
              StatusIs(StatusCode::kInvalidArgument));
}

TEST(PostPolicyV4EscapeTest, Simple) {
  EXPECT_THAT(PostPolicyV4Escape("\127\065abcd$"),
              IsOkAndHolds("\127\065abcd$"));
  auto constexpr kUtf8Text = u8"\\\b\f\n\r\t\v\u0080\u0119";
  auto input = std::string{kUtf8Text, kUtf8Text + 11};
  EXPECT_THAT(PostPolicyV4Escape(input),
              IsOkAndHolds("\\\\b\\f\\n\\r\\t\\v\\u0080\\u0119"));
  // Taken from the examples in https://en.wikipedia.org/wiki/UTF-8
  EXPECT_THAT(PostPolicyV4Escape("\x24"), IsOkAndHolds("$"));
  EXPECT_THAT(PostPolicyV4Escape("\xC2\xA3"), IsOkAndHolds("\\u00a3"));
  EXPECT_THAT(PostPolicyV4Escape("\xD0\x98"), IsOkAndHolds("\\u0418"));
  EXPECT_THAT(PostPolicyV4Escape("\xE0\xA4\xB9"), IsOkAndHolds("\\u0939"));
  EXPECT_THAT(PostPolicyV4Escape("\xE2\x82\xAC"), IsOkAndHolds("\\u20ac"));
  EXPECT_THAT(PostPolicyV4Escape("\xED\x95\x9C"), IsOkAndHolds("\\ud55c"));
  EXPECT_THAT(PostPolicyV4Escape("\xF0\x90\x8D\x88"),
              IsOkAndHolds("\\U00010348"));
}

TEST(PolicyDocumentV4Request, Printing) {
  PolicyDocumentV4 doc;
  doc.bucket = "test-bucket";
  doc.object = "test-object";
  doc.expiration = std::chrono::seconds(13);
  doc.timestamp =
      google::cloud::internal::ParseRfc3339("2010-06-16T11:11:11Z").value();
  PolicyDocumentV4Request req(doc);
  std::stringstream stream;
  stream << req;
  EXPECT_EQ(
      "PolicyDocumentRequest={{\"conditions\":[{\"bucket\":\"test-bucket\"},{"
      "\"key\":\"test-object\"},{\"x-goog-date\":\"20100616T111111Z\"},{\"x-"
      "goog-credential\":\"/20100616/auto/storage/"
      "goog4_request\"},{\"x-goog-algorithm\":\"GOOG4-RSA-SHA256\"}],"
      "\"expiration\":\"2010-06-16T11:11:24Z\"}}",
      stream.str());
}

TEST(PolicyDocumentV4Request, RequiredFormFields) {
  PolicyDocumentV4 doc;
  doc.bucket = "test-bucket";
  doc.object = "test-object";
  doc.expiration = std::chrono::seconds(13);
  doc.timestamp =
      google::cloud::internal::ParseRfc3339("2010-06-16T11:11:11Z").value();
  doc.conditions = std::vector<PolicyDocumentCondition>{
      PolicyDocumentCondition::StartsWith("key", ""),
      PolicyDocumentCondition::ExactMatchObject("acl", "bucket-owner-read"),
      PolicyDocumentCondition::ExactMatchObject("bucket", "travel-maps"),
      PolicyDocumentCondition::ExactMatch("Content-Type", "image/jpeg"),
      PolicyDocumentCondition::ContentLengthRange(0, 1000000)};
  PolicyDocumentV4Request req(doc);
  req.set_multiple_options(AddExtensionField("x-some-header", "header-value"));
  std::map<std::string, std::string> expected_fields{
      {"Content-Type", "image/jpeg"},
      {"acl", "bucket-owner-read"},
      {"key", "test-object"},
      {"x-goog-algorithm", "GOOG4-RSA-SHA256"},
      {"x-goog-credential", "/20100616/auto/storage/goog4_request"},
      {"x-goog-date", "20100616T111111Z"},
      {"x-some-header", "header-value"}};
  EXPECT_EQ(expected_fields, req.RequiredFormFields());
}

TEST(PolicyDocumentV4Request, Url) {
  PolicyDocumentV4 doc;
  doc.bucket = "test-bucket";
  PolicyDocumentV4Request request(doc);
  auto const custom_endpoint_authority = std::string{"storage.mydomain.com"};
  request.SetEndpointAuthority(custom_endpoint_authority);
  EXPECT_THAT(request.Url(),
              StartsWith("https://" + custom_endpoint_authority));
}

TEST(PolicyDocumentV4Request, UrlWithVirtualHostName) {
  PolicyDocumentV4 doc;
  doc.bucket = "test-bucket";
  auto const custom_endpoint_authority = std::string{"storage.mydomain.com"};
  PolicyDocumentV4Request request(doc);
  request.SetOption(VirtualHostname(true));
  request.SetEndpointAuthority(custom_endpoint_authority);

  auto const expected_url = std::string{"https://" + doc.bucket + "." +
                                        custom_endpoint_authority + "/"};
  EXPECT_EQ(request.Url(), expected_url);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

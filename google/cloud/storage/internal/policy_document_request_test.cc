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

#include "google/cloud/storage/internal/policy_document_request.h"
#include "google/cloud/internal/parse_rfc3339.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace internal {
namespace {

using ::google::cloud::testing_util::StatusIs;

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
              ::testing::ElementsAre("test-delegate1", "test-delegate2"));
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
              ::testing::ElementsAre("test-delegate1", "test-delegate2"));
}

TEST(PostPolicyV4EscapeTest, OnlyAscii) {
  EXPECT_EQ("\\\\b\\f\\n\\r\\t\\vabcd",
            *PostPolicyV4Escape("\\\b\f\n\r\t\vabcd"));
}

#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
TEST(PostPolicyV4EscapeTest, InvalidUtf) {
  // In UTF8 a byte larger than 0x7f indicates that there is a byte following
  // it. Here we truncate the string to cause the UTF parser to fail.
  std::string invalid_utf8(1, '\x80');
  EXPECT_THAT(PostPolicyV4Escape(invalid_utf8),
              StatusIs(StatusCode::kInvalidArgument));
}
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS

TEST(PostPolicyV4EscapeTest, Simple) {
#if GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_EQ("\127\065abcd$", *PostPolicyV4Escape("\127\065abcd$"));
  EXPECT_EQ("\\\\b\\f\\n\\r\\t\\v\\u0080\\u0119",
            *PostPolicyV4Escape(u8"\\\b\f\n\r\t\v\u0080\u0119"));
#else   // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
  EXPECT_THAT(PostPolicyV4Escape("ąę"), StatusIs(StatusCode::kUnimplemented));
#endif  // GOOGLE_CLOUD_CPP_HAVE_EXCEPTIONS
}

TEST(PolicyDocumentV4Request, Printing) {
  PolicyDocumentV4 doc;
  doc.bucket = "test-bucket";
  doc.object = "test-object";
  doc.expiration = std::chrono::seconds(13);
  doc.timestamp = google::cloud::internal::ParseRfc3339("2010-06-16T11:11:11Z");
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
  doc.timestamp = google::cloud::internal::ParseRfc3339("2010-06-16T11:11:11Z");
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

}  // namespace
}  // namespace internal
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

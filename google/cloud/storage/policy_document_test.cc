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

#include "google/cloud/storage/policy_document.h"
#include "google/cloud/storage/bucket_metadata.h"
#include "google/cloud/storage/internal/policy_document_request.h"
#include "google/cloud/internal/format_time_point.h"
#include "google/cloud/internal/parse_rfc3339.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
PolicyDocument CreatePolicyDocumentForTest() {
  PolicyDocument result;
  result.expiration =
      google::cloud::internal::ParseRfc3339("2010-06-16T11:11:11Z");
  result.conditions = {
      {{"starts-with", "$key", ""}},
      {{"acl", "bucket-owner-read"}},
      {{"bucket", "travel-maps"}},
      {{"eq", "$Content-Type", "image/jpeg"}},
      {{"content-length-range", "0", "1000000"}},
  };
  return result;
}

PolicyDocumentV4 CreatePolicyDocumentV4ForTest() {
  PolicyDocumentV4 result;
  result.timestamp =
      google::cloud::internal::ParseRfc3339("2010-06-16T11:11:11Z");
  result.conditions = {
      {{"starts-with", "$key", ""}},
      {{"acl", "bucket-owner-read"}},
      {{"eq", "$Content-Type", "image/jpeg"}},
      {{"content-length-range", "0", "1000000"}},
  };
  result.bucket = "test-bucket";
  result.object = "test-object";
  result.expiration = std::chrono::seconds(123);
  return result;
}

/**
 * @test Verify that PolicyDocumentCondition streaming operator works as
 * expected.
 */
TEST(PolicyDocumentTests, ConditionStreaming) {
  PolicyDocumentCondition condition =
      PolicyDocumentCondition::StartsWith("key", "");
  std::ostringstream os;
  os << condition;
  auto actual = os.str();
  EXPECT_EQ(actual, "PolicyDocumentCondition=[starts-with, $key, ]");
}

/// @test Verify that PolicyDocument parsing work as expected.
TEST(PolicyDocumentTests, Parsing) {
  PolicyDocument actual = CreatePolicyDocumentForTest();
  std::vector<PolicyDocumentCondition> expected_conditions;
  expected_conditions.emplace_back(
      PolicyDocumentCondition::StartsWith("key", ""));
  expected_conditions.emplace_back(
      PolicyDocumentCondition::ExactMatchObject("acl", "bucket-owner-read"));
  expected_conditions.emplace_back(
      PolicyDocumentCondition::ExactMatchObject("bucket", "travel-maps"));
  expected_conditions.emplace_back(
      PolicyDocumentCondition::ExactMatch("Content-Type", "image/jpeg"));
  expected_conditions.emplace_back(
      PolicyDocumentCondition::ContentLengthRange(0, 1000000));
  EXPECT_EQ(expected_conditions, actual.conditions);
}

/// @test Verify that PolicyDocumentCondition::StartsWith works as expected.
TEST(PolicyDocumentTests, StartsWith) {
  auto condition = PolicyDocumentCondition::StartsWith("key", "");
  std::vector<std::string> subset = {"starts-with", "$key", ""};
  EXPECT_THAT(subset, ::testing::IsSubsetOf(condition));
}

/**
 * @test Verify that PolicyDocumentCondition::ExactMatch works as
 * expected.
 */
TEST(PolicyDocumentTests, ExactMatchObject) {
  auto condition = PolicyDocumentCondition::ExactMatch("bucket", "travel-maps");
  std::vector<std::string> subset = {"eq", "$bucket", "travel-maps"};
  EXPECT_THAT(subset, ::testing::IsSubsetOf(condition));
}

/// @test Verify that PolicyDocumentCondition::ExactMatchObject works as
/// expected.
TEST(PolicyDocumentTests, ExactMatch) {
  auto condition =
      PolicyDocumentCondition::ExactMatchObject("Content-Type", "image/jpeg");
  std::vector<std::string> subset = {"Content-Type", "image/jpeg"};
  EXPECT_THAT(subset, ::testing::IsSubsetOf(condition));
}

/**
 * @test Verify that PolicyDocumentCondition::ContentLengthRange works as
 * expected.
 */
TEST(PolicyDocumentTests, ContentLengthRange) {
  auto condition = PolicyDocumentCondition::ContentLengthRange(0, 42);
  std::vector<std::string> subset = {"content-length-range", "0", "42"};
  EXPECT_THAT(subset, ::testing::IsSubsetOf(condition));
}

/// @test Verify that PolicyDocumentCondition comparisons work as expected.
TEST(PolicyDocumentTests, ConditionsCompare) {
  EXPECT_EQ(PolicyDocumentCondition::StartsWith("key", ""),
            PolicyDocumentCondition::StartsWith("key", ""));
  EXPECT_NE(PolicyDocumentCondition::ContentLengthRange(0, 42),
            PolicyDocumentCondition::ContentLengthRange(0, 50));
  EXPECT_NE(PolicyDocumentCondition::ExactMatch("key", ""),
            PolicyDocumentCondition::ExactMatchObject("key", ""));
  EXPECT_NE(PolicyDocumentCondition::StartsWith("key", ""),
            PolicyDocumentCondition::ExactMatchObject("key", ""));
}

/// @test Verify that PolicyDocument streaming operator works as expected.
TEST(PolicyDocumentTests, PolicyDocumentStreaming) {
  PolicyDocument document = CreatePolicyDocumentForTest();
  std::ostringstream os;
  os << document;
  auto actual = os.str();
  EXPECT_EQ(actual,
            "PolicyDocument={expiration=2010-06-16T11:11:11Z, "
            "conditions=[PolicyDocumentCondition=[starts-with, $key, ], "
            "PolicyDocumentCondition=[acl, bucket-owner-read], "
            "PolicyDocumentCondition=[bucket, travel-maps], "
            "PolicyDocumentCondition=[eq, $Content-Type, image/jpeg], "
            "PolicyDocumentCondition=[content-length-range, 0, 1000000]]}");
}

/// @test Verify that PolicyDocumentResult streaming operator works as expected.
TEST(PolicyDocumentTests, PolicyDocumentResultStreaming) {
  PolicyDocumentResult result = {
      "foo@foo.com",
      google::cloud::internal::ParseRfc3339("2010-06-16T11:11:11Z"),
      "asdfasdfasdf", "asdfasdfasdf"};
  std::ostringstream os;
  os << result;
  auto actual = os.str();
  EXPECT_EQ(actual,
            "PolicyDocumentResult={access_id=foo@foo.com"
            ", expiration=" +
                google::cloud::internal::FormatRfc3339(result.expiration) +
                ", policy=asdfasdfasdf, signature=asdfasdfasdf}");
}

/// @test Verify that PolicyDocumentV4 streaming operator works as expected.
TEST(PolicyDocumentTests, PolicyDocumentV4Streaming) {
  PolicyDocumentV4 document = CreatePolicyDocumentV4ForTest();
  std::ostringstream os;
  os << document;
  auto actual = os.str();
  EXPECT_EQ(actual,
            "PolicyDocumentV4={bucket=test-bucket, object=test-object, "
            "expiration=123, timestamp=2010-06-16T11:11:11Z, "
            "conditions=[PolicyDocumentCondition=[starts-with, $key, ], "
            "PolicyDocumentCondition=[acl, bucket-owner-read], "
            "PolicyDocumentCondition=[eq, $Content-Type, image/jpeg], "
            "PolicyDocumentCondition=[content-length-range, 0, 1000000]]}");
}

/// @test Verify that PolicyDocumentResult streaming operator works as expected.
TEST(PolicyDocumentTests, PolicyDocumentV4ResultStreaming) {
  PolicyDocumentV4Result result = {
      "https://storage.googleapis.com/rsaposttest",
      "foo@foo.com",
      google::cloud::internal::ParseRfc3339("2010-06-16T11:11:11Z"),
      "test-policy",
      "test-sig",
      "test-alg",
      {}};
  std::ostringstream os;
  os << result;
  auto actual = os.str();
  EXPECT_EQ(actual,
            "PolicyDocumentV4Result={url=https://storage.googleapis.com/"
            "rsaposttest, access_id=foo@foo.com, expiration=" +
                google::cloud::internal::FormatRfc3339(result.expiration) +
                ", policy=test-policy, signature=test-sig, "
                "signing_algorithm=test-alg}");
}

/// @test Verify that PolicyDocumentV4 ctor works.
TEST(PolicyDocumentTests, PolicyDocumentV4Ctor) {
  auto const now = std::chrono::system_clock::now();
  PolicyDocumentV4 doc("bucket", "object", std::chrono::seconds(42), now);
  EXPECT_EQ("bucket", doc.bucket);
  EXPECT_EQ("object", doc.object);
  EXPECT_EQ(42, doc.expiration.count());
  EXPECT_EQ(now, doc.timestamp);
}

/// @test Verify that PolicyDocumentV4 ctor works.
TEST(PolicyDocumentTests, PolicyDocumentV4CtorDefaultTime) {
  auto const before = std::chrono::system_clock::now();
  PolicyDocumentV4 doc("bucket", "object", std::chrono::seconds(42));
  auto const after = std::chrono::system_clock::now();
  EXPECT_LE(before, doc.timestamp);
  EXPECT_GE(after, doc.timestamp);
}

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

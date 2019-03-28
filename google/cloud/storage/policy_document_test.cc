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
#include "google/cloud/storage/internal/policy_document_requests.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage {
inline namespace STORAGE_CLIENT_NS {
namespace {
PolicyDocument CreatePolicyDocumentForTest() {
  std::string text = R"""({
    "expiration": "2010-06-16T11:11:11Z",
    "conditions": [
      ["starts-with", "$key", "" ],
      {"acl": "bucket-owner-read" },
      {"bucket": "travel-maps"},
      ["eq", "$Content-Type", "image/jpeg" ],
      ["content-length-range", 0, 1000000]
    ]
  })""";
  return internal::PolicyDocumentParser::FromString(text).value();
}

/// @test Verify that PolicyDocument parsing works as expected.
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

/// @test Verify that PolicyDocument streaming operator works as expected.

}  // namespace
}  // namespace STORAGE_CLIENT_NS
}  // namespace storage
}  // namespace cloud
}  // namespace google

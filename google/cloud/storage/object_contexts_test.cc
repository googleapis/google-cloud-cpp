// Copyright 2026 Google LLC
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

#include "google/cloud/storage/object_contexts.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

// ============================================================================
// Happy Path Tests
// ============================================================================

TEST(ObjectContextsTest, ValidContexts) {
  // Typical valid key-value pairs
  EXPECT_TRUE(ValidateObjectContext("validKey1", "validValue1").ok());
  EXPECT_TRUE(ValidateObjectContext("a", "b").ok());

  // Exact 256-byte limits
  EXPECT_TRUE(
      ValidateObjectContext(std::string(256, 'k'), std::string(256, 'v')).ok());
}

TEST(ObjectContextsTest, ReservedPrefixValid) {
  // Similar but valid keys
  EXPECT_TRUE(ValidateObjectContext("goodKey", "value").ok());
  EXPECT_TRUE(ValidateObjectContext("goo", "value").ok());
}

TEST(ObjectContextsTest, AggregateCountLimit) {
  ObjectContexts contexts;

  // Insert exactly 50 entries
  for (int i = 0; i < 50; ++i) {
    contexts.upsert("k" + std::to_string(i),
                    ObjectCustomContextPayload{"v", {}, {}});
  }

  // Should pass validation unconditionally
  EXPECT_TRUE(ValidateObjectContextsAggregate(contexts).ok());
}

// ============================================================================
// Sad Path Tests: Expect failures and non-OK statuses.
// ============================================================================

TEST(ObjectContextsTest, LengthLimits) {
  // Empty strings
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("", "value").code());
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("key", "").code());

  // Exceeding 256 bytes
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext(std::string(257, 'k'), "value").code());
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("key", std::string(257, 'v')).code());
}

TEST(ObjectContextsTest, StartingCharacters) {
  // Cannot start with non-alphanumeric characters
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("-key", "value").code());
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("_key", "value").code());
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("key", ".value").code());
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("key", "@value").code());
}

TEST(ObjectContextsTest, ForbiddenCharacters) {
  // Contains ', ", \, or /
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("ke'y", "value").code());
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("key", "va\"lue").code());
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("ke\\y", "value").code());
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("key", "val/ue").code());
}

TEST(ObjectContextsTest, ReservedPrefix) {
  // Cannot begin with 'goog'
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("googKey", "value").code());
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("Google", "value").code());
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("goog", "value").code());
  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContext("GOOG", "value").code());
}

TEST(ObjectContextsTest, AggregateCountLimitBreached) {
  ObjectContexts contexts;

  // Breaching the limit (51 entries)
  for (int i = 0; i < 51; ++i) {
    contexts.upsert("k" + std::to_string(i),
                    ObjectCustomContextPayload{"v", {}, {}});
  }

  EXPECT_EQ(StatusCode::kInvalidArgument,
            ValidateObjectContextsAggregate(contexts).code());
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

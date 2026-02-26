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
#include <stdexcept>
#include <string>

namespace google {
namespace cloud {
namespace storage {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

TEST(ObjectContextsTest, ValidContexts) {
  // Typical valid key-value pairs
  EXPECT_NO_THROW(ValidateObjectContext("validKey1", "validValue1"));
  EXPECT_NO_THROW(ValidateObjectContext("a", "b"));

  // Exact 256-byte limits
  EXPECT_NO_THROW(
      ValidateObjectContext(std::string(256, 'k'), std::string(256, 'v')));
}

TEST(ObjectContextsTest, LengthLimits) {
  // Empty strings
  EXPECT_THROW(ValidateObjectContext("", "value"), std::invalid_argument);
  EXPECT_THROW(ValidateObjectContext("key", ""), std::invalid_argument);

  // Exceeding 256 bytes
  EXPECT_THROW(ValidateObjectContext(std::string(257, 'k'), "value"),
               std::invalid_argument);
  EXPECT_THROW(ValidateObjectContext("key", std::string(257, 'v')),
               std::invalid_argument);
}

TEST(ObjectContextsTest, StartingCharacters) {
  // Cannot start with non-alphanumeric characters
  EXPECT_THROW(ValidateObjectContext("-key", "value"), std::invalid_argument);
  EXPECT_THROW(ValidateObjectContext("_key", "value"), std::invalid_argument);
  EXPECT_THROW(ValidateObjectContext("key", ".value"), std::invalid_argument);
  EXPECT_THROW(ValidateObjectContext("key", "@value"), std::invalid_argument);
}

TEST(ObjectContextsTest, ForbiddenCharacters) {
  // Contains ', ", \, or /
  EXPECT_THROW(ValidateObjectContext("ke'y", "value"), std::invalid_argument);
  EXPECT_THROW(ValidateObjectContext("key", "va\"lue"), std::invalid_argument);
  EXPECT_THROW(ValidateObjectContext("ke\\y", "value"), std::invalid_argument);
  EXPECT_THROW(ValidateObjectContext("key", "val/ue"), std::invalid_argument);
}

TEST(ObjectContextsTest, ReservedPrefix) {
  // Cannot begin with 'goog'
  EXPECT_THROW(ValidateObjectContext("googKey", "value"),
               std::invalid_argument);
  EXPECT_THROW(ValidateObjectContext("google", "value"), std::invalid_argument);
  EXPECT_THROW(ValidateObjectContext("goog", "value"), std::invalid_argument);

  // Similar but valid keys
  EXPECT_NO_THROW(ValidateObjectContext("goodKey", "value"));
  EXPECT_NO_THROW(ValidateObjectContext("goo", "value"));
}

TEST(ObjectContextsTest, AggregateCountLimit) {
  ObjectContexts contexts;

  // Insert exactly 50 entries
  for (int i = 0; i < 50; ++i) {
    contexts.upsert("k" + std::to_string(i),
                    ObjectCustomContextPayload{"v", {}, {}});
  }
  EXPECT_NO_THROW(ValidateObjectContextsAggregate(contexts));

  // Breaching the limit (51 entries)
  contexts.upsert("k50", ObjectCustomContextPayload{"v", {}, {}});
  EXPECT_THROW(ValidateObjectContextsAggregate(contexts),
               std::invalid_argument);
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace storage
}  // namespace cloud
}  // namespace google

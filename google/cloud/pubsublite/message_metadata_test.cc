// Copyright 2022 Google LLC
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

#include "google/cloud/pubsublite/message_metadata.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <deque>

namespace google {
namespace cloud {
namespace pubsublite {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::pubsublite::MakeMessageMetadata;
using ::google::cloud::pubsublite::MessageMetadata;
using ::google::cloud::pubsublite::v1::Cursor;
using ::google::cloud::testing_util::IsProtoEqual;
using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;

TEST(MessageMetadata, ValidParse) {
  std::int64_t partition = 2389457;
  std::int64_t offset = 945678234;
  Cursor cursor;
  cursor.set_offset(offset);
  std::string input = std::to_string(partition) + ":" + std::to_string(offset);
  auto mm = MakeMessageMetadata(input);
  EXPECT_TRUE(mm.ok());
  EXPECT_EQ(mm->partition, partition);
  EXPECT_THAT(mm->cursor, IsProtoEqual(cursor));
}

TEST(MessageMetadata, InvalidParseBadPartition) {
  std::string input = "q2432asdf:324572368";
  auto mm = MakeMessageMetadata(input);
  EXPECT_FALSE(mm.ok());
  EXPECT_THAT(mm.status(),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Not able to parse `MessageMetadata`")));
}

TEST(MessageMetadata, InvalidParseBadOffset) {
  std::string input = "324572368:q243223423f";
  auto mm = MakeMessageMetadata(input);
  EXPECT_FALSE(mm.ok());
  EXPECT_THAT(mm.status(),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Not able to parse `MessageMetadata`")));
}

TEST(MessageMetadata, Serialize) {
  std::int64_t partition = 2389457;
  std::int64_t offset = 945678234;
  Cursor cursor;
  cursor.set_offset(offset);
  MessageMetadata mm{partition, cursor};
  EXPECT_EQ(mm.Serialize(), "2389457:945678234");
}

TEST(MessageMetadata, RoundTrip) {
  std::int64_t partition = 345452233;
  std::int64_t offset = 8574552345;
  Cursor cursor;
  cursor.set_offset(offset);
  MessageMetadata mm{partition, cursor};
  auto mm1 = MakeMessageMetadata(mm.Serialize());
  EXPECT_TRUE(mm1.ok());
  EXPECT_EQ(*mm1, mm);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsublite
}  // namespace cloud
}  // namespace google

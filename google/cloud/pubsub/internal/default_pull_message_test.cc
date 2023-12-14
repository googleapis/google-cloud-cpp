// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/cloud/pubsub/internal/default_pull_message.h"
#include "google/cloud/pubsub/message.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace google {
namespace cloud {
namespace pubsub_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(DefaultPullMessageTest, UnwrapMessage) {
  auto constexpr kText = R"pb(
    ack_id: "id"
    message {
    data: "test-data"
    attributes: { key: "key1" value: "label1" }
    attributes: { key: "key0" value: "label0" }
    message_id: "test-message-id"
    publish_time { seconds: 123 nanos: 456000 }
    ordering_key: "test-ordering-key"
    }
  )pb";
  ::google::pubsub::v1::ReceivedMessage proto_message;
  EXPECT_TRUE(
      google::protobuf::TextFormat::ParseFromString(kText, &proto_message));
  auto under_test = DefaultPullMessage();

  auto message = under_test.UnwrapMessage(proto_message);

  EXPECT_EQ("test-data", message.data());
  EXPECT_THAT(
      message.attributes(),
      UnorderedElementsAre(Pair("key1", "label1"), Pair("key0", "label0")));
  EXPECT_EQ("test-message-id", message.message_id());
  auto const epoch = std::chrono::system_clock::from_time_t(0);
  auto const expected_publish_time =
      epoch + std::chrono::seconds(123) + std::chrono::nanoseconds(456000);
  EXPECT_EQ(expected_publish_time, message.publish_time());
  EXPECT_EQ("test-ordering-key", message.ordering_key());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub_internal
}  // namespace cloud
}  // namespace google

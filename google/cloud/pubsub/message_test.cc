// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/message.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/protobuf/text_format.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <sstream>
#include <tuple>

namespace google {
namespace cloud {
namespace pubsub {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(Message, Empty) {
  auto const m = MessageBuilder{}.Build();
  EXPECT_THAT(m.data(), IsEmpty());

  EXPECT_THAT(m.attributes(), IsEmpty());
}

TEST(Message, SetDataSimple) {
  auto const m0 = MessageBuilder{}.SetData("contents-0").Build();
  EXPECT_EQ("contents-0", m0.data());
  EXPECT_THAT(m0.attributes(), IsEmpty());
  EXPECT_THAT(m0.ordering_key(), IsEmpty());

  EXPECT_THAT(m0.message_id(), IsEmpty());

  auto const m1 = MessageBuilder{}.SetData("contents-1").Build();
  EXPECT_EQ("contents-1", m1.data());

  EXPECT_NE(m0, m1);
  Message copy(m0);
  EXPECT_EQ(m0, copy);
  Message move(std::move(copy));
  EXPECT_EQ(m0, move);
}

TEST(Message, SetOrderingKey) {
  auto const m0 = MessageBuilder{}.SetOrderingKey("key-0").Build();
  EXPECT_EQ("key-0", m0.ordering_key());
  EXPECT_THAT(m0.attributes(), IsEmpty());
  EXPECT_THAT(m0.data(), IsEmpty());

  EXPECT_THAT(m0.message_id(), IsEmpty());

  auto const m1 = MessageBuilder{}.SetOrderingKey("key-1").Build();
  EXPECT_EQ("key-1", m1.ordering_key());

  EXPECT_NE(m0, m1);
  Message copy(m0);
  EXPECT_EQ(m0, copy);
  Message move(std::move(copy));
  EXPECT_EQ(m0, move);
}

TEST(Message, InsertAttributeSimple) {
  auto const m0 = MessageBuilder{}
                      .InsertAttribute("k1", "v1")
                      .InsertAttribute("k2", "v2")
                      .InsertAttribute("k2", "v3")
                      .Build();
  EXPECT_THAT(m0.data(), IsEmpty());

  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(Pair("k1", "v1"), Pair("k2", "v2")));
  EXPECT_THAT(m0.ordering_key(), IsEmpty());

  EXPECT_THAT(m0.message_id(), IsEmpty());
}

TEST(Message, SetAttributeSimple) {
  auto const m0 = MessageBuilder{}
                      .SetAttribute("k1", "v1")
                      .SetAttribute("k2", "v2")
                      .SetAttribute("k2", "v3")
                      .Build();
  EXPECT_THAT(m0.data(), IsEmpty());

  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(Pair("k1", "v1"), Pair("k2", "v3")));
  EXPECT_THAT(m0.ordering_key(), IsEmpty());

  EXPECT_THAT(m0.message_id(), IsEmpty());
}

TEST(Message, SetAttributesIteratorSimple) {
  std::map<std::string, std::string> const attributes(
      {{"k1", "v1"}, {"k2", "v2"}});

  auto const m0 = MessageBuilder{}
                      .SetAttributes(attributes.begin(), attributes.end())
                      .Build();
  EXPECT_THAT(m0.data(), IsEmpty());

  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(Pair("k1", "v1"), Pair("k2", "v2")));
  EXPECT_THAT(m0.ordering_key(), IsEmpty());

  EXPECT_THAT(m0.message_id(), IsEmpty());
}

TEST(Message, SetAttributesVectorStdPairSimple) {
  auto const m0 =
      MessageBuilder{}.SetAttributes({{"k0", "v0"}, {"k1", "v1"}}).Build();
  EXPECT_THAT(m0.data(), IsEmpty());

  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(Pair("k0", "v0"), Pair("k1", "v1")));
  EXPECT_THAT(m0.ordering_key(), IsEmpty());

  EXPECT_THAT(m0.message_id(), IsEmpty());
}

TEST(Message, SetAttributesVectorStdTupleSimple) {
  using tuple = std::tuple<std::string, std::string>;
  std::vector<tuple> const attributes({tuple("k1", "v1"), tuple("k2", "v2")});
  auto const m0 = MessageBuilder{}.SetAttributes(attributes).Build();
  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(Pair("k1", "v1"), Pair("k2", "v2")));
}

TEST(Message, SetData) {
  auto const m0 =
      MessageBuilder{}.SetData("original").SetData("changed").Build();
  EXPECT_EQ("changed", m0.data());
}

TEST(Message, SetAttributesIterator) {
  std::map<std::string, std::string> const attributes(
      {{"k1", "v1"}, {"k2", "v2"}});
  auto const m0 = MessageBuilder{}
                      .SetData("original")
                      .SetAttributes({{"k0", "v0"}})
                      .SetAttributes(attributes.begin(), attributes.end())
                      .Build();
  EXPECT_EQ("original", m0.data());
  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(Pair("k1", "v1"), Pair("k2", "v2")));
}

TEST(Message, SetAttributesVectorStdPair) {
  std::vector<std::pair<std::string, std::string>> const attributes(
      {{"k1", "v1"}, {"k2", "v2"}});
  auto const m0 = MessageBuilder{}
                      .SetData("original")
                      .SetAttributes({{"k0", "v0"}})
                      .SetAttributes(attributes)
                      .Build();
  EXPECT_EQ("original", m0.data());
  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(Pair("k1", "v1"), Pair("k2", "v2")));
}

TEST(Message, SetAttributesVectorStdTuple) {
  using tuple = std::tuple<std::string, std::string>;
  std::vector<tuple> const attributes({tuple("k1", "v1"), tuple("k2", "v2")});
  auto const m0 = MessageBuilder{}
                      .SetData("original")
                      .SetAttributes({{"k0", "v0"}})
                      .SetAttributes(attributes)
                      .Build();
  EXPECT_EQ("original", m0.data());
  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(Pair("k1", "v1"), Pair("k2", "v2")));
}

TEST(Message, DataMove) {
  auto m0 = MessageBuilder{}.SetData("contents-0").Build();
  auto const d = std::move(m0).data();
  EXPECT_EQ("contents-0", d);
}

TEST(Message, FromProto) {
  auto constexpr kText = R"pb(
    data: "test-data"
    attributes: { key: "key1" value: "label1" }
    attributes: { key: "key0" value: "label0" }
    message_id: "test-message-id"
    publish_time { seconds: 123 nanos: 456000 }
    ordering_key: "test-ordering-key"
  )pb";
  ::google::pubsub::v1::PubsubMessage expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &expected));
  auto const m = pubsub_internal::FromProto(expected);

  EXPECT_EQ("test-data", m.data());
  EXPECT_THAT(m.attributes(), UnorderedElementsAre(Pair("key1", "label1"),
                                                   Pair("key0", "label0")));
  EXPECT_EQ("test-message-id", m.message_id());
  auto const epoch = std::chrono::system_clock::from_time_t(0);
  auto const expected_publish_time =
      epoch + std::chrono::seconds(123) + std::chrono::nanoseconds(456000);
  EXPECT_EQ(expected_publish_time, m.publish_time());
  EXPECT_EQ("test-ordering-key", m.ordering_key());

  auto const& actual_copy = pubsub_internal::ToProto(m);
  EXPECT_THAT(actual_copy, IsProtoEqual(expected));

  auto actual_move =
      pubsub_internal::ToProto(pubsub_internal::FromProto(expected));
  EXPECT_THAT(actual_move, IsProtoEqual(expected));
}

TEST(Message, OutputStream) {
  Message const m = MessageBuilder{}
                        .SetAttributes({{"k0", "v0"}, {"k1", "v1"}})
                        .SetData("test-only-data")
                        .Build();
  std::ostringstream os;
  os << m;
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("test-only-data"));
  EXPECT_THAT(actual, HasSubstr("k0"));
  EXPECT_THAT(actual, HasSubstr("v0"));
  EXPECT_THAT(actual, HasSubstr("k1"));
  EXPECT_THAT(actual, HasSubstr("v1"));
}

TEST(Message, SizeEstimation) {
  // https://cloud.google.com/pubsub/pricing
  auto constexpr kMessageSizeOverhead = 20;
  EXPECT_EQ(kMessageSizeOverhead,
            pubsub_internal::MessageSize(MessageBuilder{}.SetData("").Build()));
  EXPECT_EQ(
      kMessageSizeOverhead + 4,
      pubsub_internal::MessageSize(MessageBuilder{}.SetData("1234").Build()));
  EXPECT_EQ(kMessageSizeOverhead + 4 + (2 + 5) + (2 + 5),
            pubsub_internal::MessageSize(
                MessageBuilder{}
                    .SetData("1234")
                    .SetAttributes({{"k0", "12345"}, {"k1", "12345"}})
                    .Build()));

  auto constexpr kText = R"pb(
    data: "1234"
    attributes: { key: "k0" value: "12345" }
    attributes: { key: "k1" value: "12345" }
    message_id: "0123456789"
    ordering_key: "abcd"
  )pb";
  ::google::pubsub::v1::PubsubMessage expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &expected));
  EXPECT_EQ(kMessageSizeOverhead + 4 + (2 + 5) + (2 + 5) + 10 + 4,
            pubsub_internal::MessageSize(pubsub_internal::FromProto(expected)));
}

TEST(Message, SetAttributeFriend) {
  auto m0 = MessageBuilder{}.Build();
  pubsub_internal::SetAttribute("k1", "v1", m0);
  pubsub_internal::SetAttribute("k2", "v2", m0);
  pubsub_internal::SetAttribute("k2", "v3", m0);

  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(Pair("k1", "v1"), Pair("k2", "v3")));
}

TEST(Message, GetAttributeFriend) {
  auto m0 = MessageBuilder{}.SetAttributes({{"k0", "v0"}}).Build();

  auto const v0 = pubsub_internal::GetAttribute("k0", m0);
  auto const v1 = pubsub_internal::GetAttribute("k1", m0);

  EXPECT_EQ(std::string(v0.data(), v0.size()), "v0");
  EXPECT_THAT(v1, IsEmpty());
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

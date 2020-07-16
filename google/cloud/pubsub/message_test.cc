// Copyright 2020 Google LLC
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

#include "google/cloud/pubsub/message.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>
#include <sstream>
#include <tuple>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

using ::google::cloud::testing_util::IsProtoEqual;
using ::testing::HasSubstr;
using ::testing::UnorderedElementsAre;

TEST(Message, FromData) {
  auto const m0 = Message::FromData("contents-0");
  EXPECT_EQ("contents-0", m0.data());
  EXPECT_TRUE(m0.attributes().empty());
  EXPECT_TRUE(m0.ordering_key().empty());
  EXPECT_TRUE(m0.message_id().empty());

  auto const m1 = Message::FromData("contents-1");
  EXPECT_EQ("contents-1", m1.data());

  EXPECT_NE(m0, m1);
  Message copy(m0);
  EXPECT_EQ(m0, copy);
  Message move(std::move(copy));
  EXPECT_EQ(m0, move);
}

TEST(Message, FromAttributesIterator) {
  std::map<std::string, std::string> const attributes(
      {{"k1", "v1"}, {"k2", "v2"}});

  auto const m0 = Message::FromAttributes(attributes.begin(), attributes.end());
  EXPECT_TRUE(m0.data().empty());
  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(std::make_pair("k1", "v1"),
                                   std::make_pair("k2", "v2")));
  EXPECT_TRUE(m0.ordering_key().empty());
  EXPECT_TRUE(m0.message_id().empty());
}

TEST(Message, FromAttributesVectorStdPair) {
  auto const m0 = Message::FromAttributes({{"k0", "v0"}, {"k1", "v1"}});
  EXPECT_TRUE(m0.data().empty());
  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(std::make_pair("k0", "v0"),
                                   std::make_pair("k1", "v1")));
  EXPECT_TRUE(m0.ordering_key().empty());
  EXPECT_TRUE(m0.message_id().empty());
}

TEST(Message, FromAttributesVectorStdTuple) {
  using tuple = std::tuple<std::string, std::string>;
  std::vector<tuple> const attributes({tuple("k1", "v1"), tuple("k2", "v2")});
  auto const m0 = Message::FromAttributes(attributes);
  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(std::make_pair("k1", "v1"),
                                   std::make_pair("k2", "v2")));
}

TEST(Message, SetData) {
  auto const m0 = Message::SetData(Message::FromData("original"), "changed");
  EXPECT_EQ("changed", m0.data());
}

TEST(Message, SetAttributesIterator) {
  std::map<std::string, std::string> const attributes(
      {{"k1", "v1"}, {"k2", "v2"}});
  auto const m0 = Message::SetAttributes(
      Message::SetData(Message::FromAttributes({{"k0", "v0"}}), "original"),
      attributes.begin(), attributes.end());
  EXPECT_EQ("original", m0.data());
  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(std::make_pair("k1", "v1"),
                                   std::make_pair("k2", "v2")));
}

TEST(Message, SetAttributesVectorStdPair) {
  std::vector<std::pair<std::string, std::string>> const attributes(
      {{"k1", "v1"}, {"k2", "v2"}});
  auto const m0 = Message::SetAttributes(
      Message::SetData(Message::FromAttributes({{"k0", "v0"}}), "original"),
      attributes);
  EXPECT_EQ("original", m0.data());
  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(std::make_pair("k1", "v1"),
                                   std::make_pair("k2", "v2")));
}

TEST(Message, SetAttributesVectorStdTuple) {
  using tuple = std::tuple<std::string, std::string>;
  std::vector<tuple> const attributes({tuple("k1", "v1"), tuple("k2", "v2")});
  auto const m0 = Message::SetAttributes(
      Message::SetData(Message::FromAttributes({{"k0", "v0"}}), "original"),
      attributes);
  EXPECT_EQ("original", m0.data());
  EXPECT_THAT(m0.attributes(),
              UnorderedElementsAre(std::make_pair("k1", "v1"),
                                   std::make_pair("k2", "v2")));
}

TEST(Message, DataMove) {
  auto m0 = Message::FromData("contents-0");
  std::string const d = std::move(m0).data();
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
  EXPECT_THAT(m.attributes(),
              UnorderedElementsAre(std::make_pair("key1", "label1"),
                                   std::make_pair("key0", "label0")));
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
  Message const m = Message::SetAttributes(Message::FromData("test-only-data"),
                                           {{"k0", "v0"}, {"k1", "v1"}});
  std::ostringstream os;
  os << m;
  auto const actual = std::move(os).str();
  EXPECT_THAT(actual, HasSubstr("test-only-data"));
  EXPECT_THAT(actual, HasSubstr("k0"));
  EXPECT_THAT(actual, HasSubstr("v0"));
  EXPECT_THAT(actual, HasSubstr("k1"));
  EXPECT_THAT(actual, HasSubstr("v1"));
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

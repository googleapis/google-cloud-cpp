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

#include "google/cloud/spanner/keys.h"
#include "google/cloud/spanner/testing/matchers.h"
#include <google/protobuf/text_format.h>
#include <google/spanner/v1/keys.pb.h>
#include <gmock/gmock.h>
#include <cstdint>
#include <type_traits>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

TEST(KeyTest, ConstructionCopyAssign) {
  Key key1;
  EXPECT_EQ(key1, key1);

  Key key2 = MakeKey(123, "hello");
  EXPECT_EQ(key2, key2);
  EXPECT_NE(key1, key2);

  key1 = key2;
  EXPECT_EQ(key1, key2);

  key2 = std::move(key1);
  EXPECT_EQ(Value(123), key2[0]);
  EXPECT_EQ(Value("hello"), key2[1]);
}

TEST(KeyTest, MakeKey) {
  Key key = MakeKey();
  EXPECT_EQ(key, Key{});
  EXPECT_EQ(key.size(), 0);

  key = MakeKey(123);
  EXPECT_NE(key, Key{});
  EXPECT_EQ(key.size(), 1);
  EXPECT_EQ(key, MakeKey(std::int64_t{123}));

  key = MakeKey(123, "hello");
  EXPECT_NE(key, Key{});
  EXPECT_EQ(key.size(), 2);
  EXPECT_EQ(key, MakeKey(std::int64_t{123}, std::string("hello")));

  EXPECT_EQ(key, MakeKey(Value(123), Value("hello")));
}

TEST(KeyBoundTest, ValueSemantics) {
  KeyBound kb1 = MakeKeyBoundOpen(123);
  EXPECT_EQ(kb1, kb1);

  KeyBound kb2 = MakeKeyBoundClosed(123);
  EXPECT_NE(kb1, kb2);

  kb2 = kb1;
  EXPECT_EQ(kb1, kb2);

  kb2 = std::move(kb1);
  EXPECT_EQ(kb2.key(), MakeKey(123));
  EXPECT_EQ(kb2.bound(), KeyBound::Bound::kOpen);
}

TEST(KeyBoundTest, Open) {
  KeyBound kb1 = MakeKeyBoundOpen(123);
  EXPECT_EQ(kb1, kb1);
  KeyBound kb2 = MakeKeyBoundOpen(456);
  EXPECT_NE(kb1, kb2);

  EXPECT_EQ(kb1.bound(), KeyBound::Bound::kOpen);
  EXPECT_EQ(kb2.bound(), KeyBound::Bound::kOpen);
}

TEST(KeyBoundTest, Closed) {
  KeyBound kb1 = MakeKeyBoundClosed(123);
  EXPECT_EQ(kb1, kb1);
  KeyBound kb2 = MakeKeyBoundClosed(456);
  EXPECT_NE(kb1, kb2);

  EXPECT_EQ(kb1.bound(), KeyBound::Bound::kClosed);
  EXPECT_EQ(kb2.bound(), KeyBound::Bound::kClosed);
}

TEST(KeyBoundTest, RvalueKeyAccessor) {
  std::string s = "12345678901234567890";
  KeyBound kb = MakeKeyBoundClosed(123, s);
  Key key = std::move(kb).key();
  EXPECT_EQ(key, MakeKey(123, s));
}

TEST(KeySetTest, ValueSemantics) {
  KeySet ks1;
  EXPECT_EQ(ks1, ks1);

  KeySet ks2 = ks1;
  EXPECT_EQ(ks1, ks2);

  ks2 = ks1;
  EXPECT_EQ(ks1, ks2);

  ks2 = std::move(ks1);
  EXPECT_EQ(ks2, ks2);
}

TEST(KeySetTest, NoKeys) {
  ::google::spanner::v1::KeySet expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString("", &expected));
  KeySet no_keys;
  ::google::spanner::v1::KeySet result = internal::ToProto(no_keys);
  EXPECT_THAT(result, spanner_testing::IsProtoEqual(expected));
  EXPECT_EQ(internal::FromProto(expected), no_keys);
}

TEST(KeySetTest, AllKeys) {
  auto constexpr kText = R"pb(
    all: true
  )pb";
  ::google::spanner::v1::KeySet expected;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(kText, &expected));
  auto all_keys = KeySet::All();
  ::google::spanner::v1::KeySet result = internal::ToProto(all_keys);
  EXPECT_THAT(result, spanner_testing::IsProtoEqual(expected));
  EXPECT_EQ(internal::FromProto(expected), all_keys);
}

TEST(KeySetTest, EqualityEmpty) {
  KeySet expected;
  KeySet actual;
  EXPECT_EQ(expected, actual);
}

TEST(KeySetTest, EqualityAll) {
  KeySet expected = KeySet::All();
  KeySet empty;
  EXPECT_NE(expected, empty);
  KeySet actual = KeySet::All();
  EXPECT_EQ(expected, actual);

  // Adding keys to an "all" KeySet still logically represents "all".
  actual.AddKey(MakeKey(123));
  EXPECT_EQ(expected, actual);
}

TEST(KeySetTest, EqualityKeys) {
  auto ks0 = KeySet();
  ks0.AddKey(MakeKey("foo0", "bar0"));
  ks0.AddKey(MakeKey("foo1", "bar1"));

  auto ks1 = KeySet();
  ks1.AddKey(MakeKey("foo0", "bar0"));
  EXPECT_NE(ks0, ks1);
  ks1.AddKey(MakeKey("foo1", "bar1"));
  EXPECT_EQ(ks0, ks1);
}

TEST(KeySetTest, EqualityKeyRanges) {
  auto range0 = std::make_pair(MakeKeyBoundClosed("start00", "start01"),
                               MakeKeyBoundClosed("end00", "end01"));
  auto range1 = std::make_pair(MakeKeyBoundOpen("start10", "start11"),
                               MakeKeyBoundOpen("end10", "end11"));
  auto ks0 = KeySet()
                 .AddRange(range0.first, range0.second)
                 .AddRange(range1.first, range1.second);
  auto ks1 = KeySet().AddRange(range0.first, range0.second);
  EXPECT_NE(ks0, ks1);
  ks1.AddRange(range1.first, range1.second);
  EXPECT_EQ(ks0, ks1);
}

TEST(KeySetTest, RoundTripProtos) {
  auto test_cases = {
      KeySet(),                                      //
      KeySet::All(),                                 //
      KeySet()                                       //
          .AddKey(MakeKey(42)),                      //
      KeySet()                                       //
          .AddKey(MakeKey(42))                       //
          .AddKey(MakeKey(123)),                     //
      KeySet()                                       //
          .AddKey(MakeKey(42, "hi"))                 //
          .AddKey(MakeKey(123, "bye")),              //
      KeySet()                                       //
          .AddRange(MakeKeyBoundClosed(42, "hi"),    //
                    MakeKeyBoundClosed(43, "bye")),  //
      KeySet()                                       //
          .AddRange(MakeKeyBoundClosed(42, "hi"),    //
                    MakeKeyBoundOpen(43, "bye")),    //
      KeySet()                                       //
          .AddRange(MakeKeyBoundOpen(42, "hi"),      //
                    MakeKeyBoundOpen(43, "bye")),    //
      KeySet()                                       //
          .AddRange(MakeKeyBoundOpen(42, "hi"),      //
                    MakeKeyBoundClosed(43, "bye")),  //
  };

  for (auto const& tc : test_cases) {
    EXPECT_EQ(tc, internal::FromProto(internal::ToProto(tc)));
  }
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

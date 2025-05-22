// Copyright 2024 Google LLC
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

#include "google/cloud/bigtable/emulator/filtered_map.h"
#include "google/cloud/bigtable/row_range.h"
#include "google/cloud/testing_util/chrono_literals.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {

using testing_util::chrono_literals::operator""_ms;

bool const kOpen = true;
bool const kClosed = false;

template <typename Map>
std::vector<std::string> Keys(Map const& map) {
  std::vector<std::string> res;
  std::transform(map.begin(), map.end(), std::back_inserter(res),
                 [](typename Map::const_iterator::value_type const& elem) {
                   return elem.first;
                 });
  return res;
}

std::vector<std::string> Vec(std::initializer_list<char const*> const& v) {
  std::vector<std::string> res;
  std::transform(v.begin(), v.end(), std::back_inserter(res),
                 [](char const* s) { return std::string(s); });
  std::sort(res.begin(), res.end());
  return res;
}

template <typename Map>
std::vector<std::chrono::milliseconds> TSKeys(Map const& map) {
  std::vector<std::chrono::milliseconds> res;
  std::transform(map.begin(), map.end(), std::back_inserter(res),
                 [](typename Map::const_iterator::value_type const& elem) {
                   return elem.first;
                 });
  return res;
}

TEST(StringRangeFilteredMapView, NoFilter) {
  std::map<std::string, int> unfiltered{{"zero", 0}, {"one", 1}, {"two", 2}};
  auto filter = StringRangeSet::All();
  StringRangeFilteredMapView<decltype(unfiltered)> filtered(unfiltered, filter);
  EXPECT_EQ(Vec({"zero", "one", "two"}), Keys(filtered));
}

TEST(StringRangeFilteredMapView, EmptyFilter) {
  std::map<std::string, int> unfiltered{{"zero", 0}, {"one", 1}, {"two", 2}};
  auto filter = StringRangeSet::Empty();
  StringRangeFilteredMapView<decltype(unfiltered)> filtered(unfiltered, filter);
  EXPECT_EQ(Vec({}), Keys(filtered));
}

TEST(StringRangeFilteredMapView, OneOpen) {
  std::map<std::string, int> unfiltered{{"AA", 0},   {"AAA", 0}, {"AAAa", 0},
                                        {"AAAb", 0}, {"AAB", 0}, {"AAC", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Sum(StringRangeSet::Range("AAA", kOpen, "AAB", kOpen));
  StringRangeFilteredMapView<decltype(unfiltered)> filtered(unfiltered, filter);
  EXPECT_EQ(Vec({"AAAa", "AAAb"}), Keys(filtered));
}

TEST(StringRangeFilteredMapView, OneClosed) {
  std::map<std::string, int> unfiltered{{"AA", 0},   {"AAA", 0}, {"AAAa", 0},
                                        {"AAAb", 0}, {"AAB", 0}, {"AAC", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Sum(StringRangeSet::Range("AAA", kClosed, "AAB", kClosed));
  StringRangeFilteredMapView<decltype(unfiltered)> filtered(unfiltered, filter);
  EXPECT_EQ(Vec({"AAA", "AAAa", "AAAb", "AAB"}), Keys(filtered));
}

TEST(StringRangeFilteredMapView, NoEntriesAfterClosedFilter) {
  std::map<std::string, int> unfiltered{
      {"AA", 0}, {"AAA", 0}, {"AAAa", 0}, {"AAAb", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Sum(StringRangeSet::Range("AAA", kClosed, "AAB", kClosed));
  StringRangeFilteredMapView<decltype(unfiltered)> filtered(unfiltered, filter);
  EXPECT_EQ(Vec({"AAA", "AAAa", "AAAb"}), Keys(filtered));
}

TEST(StringRangeFilteredMapView, NoEntriesAfterOpenFilter) {
  std::map<std::string, int> unfiltered{
      {"AA", 0}, {"AAA", 0}, {"AAAa", 0}, {"AAAb", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Sum(StringRangeSet::Range("AAA", kOpen, "AAB", kOpen));
  StringRangeFilteredMapView<decltype(unfiltered)> filtered(unfiltered, filter);
  EXPECT_EQ(Vec({"AAAa", "AAAb"}), Keys(filtered));
}

TEST(StringRangeFilteredMapView, NoEntriesBeforeClosedFilter) {
  std::map<std::string, int> unfiltered{
      {"AAA", 0}, {"AAAa", 0}, {"AAAb", 0}, {"AAB", 0}, {"AAC", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Sum(StringRangeSet::Range("AAA", kClosed, "AAB", kClosed));
  StringRangeFilteredMapView<decltype(unfiltered)> filtered(unfiltered, filter);
  EXPECT_EQ(Vec({"AAA", "AAAa", "AAAb", "AAB"}), Keys(filtered));
}

TEST(StringRangeFilteredMapView, NoEntriesBeforeOpenFilter) {
  std::map<std::string, int> unfiltered{
      {"AAAa", 0}, {"AAAb", 0}, {"AAB", 0}, {"AAC", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Sum(StringRangeSet::Range("AAA", kOpen, "AAB", kOpen));
  StringRangeFilteredMapView<decltype(unfiltered)> filtered(unfiltered, filter);
  EXPECT_EQ(Vec({"AAAa", "AAAb"}), Keys(filtered));
}

TEST(StringRangeFilteredMapView, MultipleFilters) {
  std::map<std::string, int> unfiltered{
      {"AA", 0},   {"AAA", 0}, {"AAAa", 0}, {"AAAb", 0}, {"AAB", 0},
      {"AAC", 0},  {"BB", 0},  {"BBB", 0},  {"BBBb", 0}, {"CCCa", 0},
      {"CCCb", 0}, {"CCD", 0}, {"CCE", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Sum(StringRangeSet::Range("AAA", kOpen, "AAB", kClosed));
  filter.Sum(StringRangeSet::Range("BBB", kClosed, "BBC", kOpen));
  filter.Sum(StringRangeSet::Range("CCC", kClosed, "CCD", kOpen));
  StringRangeFilteredMapView<decltype(unfiltered)> filtered(unfiltered, filter);

  EXPECT_EQ(Vec({"AAAa", "AAAb", "AAB", "BBB", "BBBb", "CCCa", "CCCb"}),
            Keys(filtered));
}

TEST(TimestampRangeFilteredMapView, NoFilter) {
  std::map<std::chrono::milliseconds, int, std::greater<>> unfiltered{
      {0_ms, 0}, {1_ms, 1}, {2_ms, 2}};
  auto filter = TimestampRangeSet::All();
  TimestampRangeFilteredMapView<decltype(unfiltered)> filtered(unfiltered,
                                                               filter);
  EXPECT_EQ(std::vector({2_ms, 1_ms, 0_ms}), TSKeys(filtered));
}

TEST(TimestampRangeFilteredMapView, EmptyFilter) {
  std::map<std::chrono::milliseconds, int, std::greater<>> unfiltered{
      {0_ms, 0}, {1_ms, 1}, {2_ms, 2}};
  auto filter = TimestampRangeSet::Empty();
  TimestampRangeFilteredMapView<decltype(unfiltered)> filtered(unfiltered,
                                                               filter);
  EXPECT_EQ(std::vector<std::chrono::milliseconds>({}), TSKeys(filtered));
}

TEST(TimestampRangeFilteredMapView, FiniteRange) {
  std::map<std::chrono::milliseconds, int, std::greater<>> unfiltered{
      {0_ms, 0}, {1_ms, 0}, {2_ms, 0}, {3_ms, 0}, {4_ms, 0}};
  auto filter = TimestampRangeSet::Empty();
  filter.Sum(TimestampRangeSet::Range(1_ms, 3_ms));
  TimestampRangeFilteredMapView<decltype(unfiltered)> filtered(unfiltered,
                                                               filter);
  EXPECT_EQ(std::vector({2_ms, 1_ms}), TSKeys(filtered));
}

TEST(TimestampRangeFilteredMapView, InfiniteRange) {
  std::map<std::chrono::milliseconds, int, std::greater<>> unfiltered{
      {0_ms, 0}, {1_ms, 0}, {2_ms, 0}, {3_ms, 0}, {4_ms, 0}};
  auto filter = TimestampRangeSet::Empty();
  filter.Sum(TimestampRangeSet::Range(1_ms, 0_ms));
  TimestampRangeFilteredMapView<decltype(unfiltered)> filtered(unfiltered,
                                                               filter);
  EXPECT_EQ(std::vector({4_ms, 3_ms, 2_ms, 1_ms}), TSKeys(filtered));
}

TEST(TimestampRangeFilteredMapView, MultipleFilters) {
  std::chrono::milliseconds max_millis(std::numeric_limits<int64_t>::max());
  std::map<std::chrono::milliseconds, int, std::greater<>> unfiltered{
      {0_ms, 0},  {1_ms, 0},  {2_ms, 0},  {3_ms, 0},
      {4_ms, 0},  {5_ms, 0},  {6_ms, 0},  {7_ms, 0},
      {8_ms, 0},  {9_ms, 0},  {10_ms, 0}, {11_ms, 0},
      {12_ms, 0}, {13_ms, 0}, {14_ms, 0}, {max_millis, 0},
  };
  auto filter = TimestampRangeSet::Empty();
  filter.Sum(TimestampRangeSet::Range(1_ms, 3_ms));
  filter.Sum(TimestampRangeSet::Range(3_ms, 5_ms));
  filter.Sum(TimestampRangeSet::Range(6_ms, 8_ms));
  filter.Sum(TimestampRangeSet::Range(10_ms, 12_ms));
  filter.Sum(TimestampRangeSet::Range(13_ms, 0_ms));
  TimestampRangeFilteredMapView<decltype(unfiltered)> filtered(unfiltered,
                                                               filter);
  EXPECT_EQ(std::vector({max_millis, 14_ms, 13_ms, 11_ms, 10_ms, 7_ms, 6_ms,
                         4_ms, 3_ms, 2_ms, 1_ms}),
            TSKeys(filtered));
}

TEST(RegexFiteredMapView, NoFilter) {
  std::vector<std::shared_ptr<re2::RE2 const>> patterns;
  std::map<std::string, int> unfiltered{{"zero", 0}, {"one", 1}, {"two", 2}};
  auto filter = StringRangeSet::All();

  RegexFiteredMapView<decltype(unfiltered)> filtered(unfiltered, patterns);
  EXPECT_EQ(Vec({"zero", "one", "two"}), Keys(filtered));
}

TEST(RegexFiteredMapView, EmptyFilter) {
  auto pattern = std::make_shared<re2::RE2 const>("this_will_not_be_matched");
  ASSERT_TRUE(pattern->ok());
  std::vector<std::shared_ptr<re2::RE2 const>> patterns({std::move(pattern)});

  std::map<std::string, int> unfiltered{{"zero", 0}, {"one", 1}, {"two", 2}};
  RegexFiteredMapView<decltype(unfiltered)> filtered(unfiltered, patterns);
  EXPECT_EQ(Vec({}), Keys(filtered));
}

TEST(RegexFiteredMapView, OneFilter) {
  auto pattern = std::make_shared<re2::RE2 const>("^[a-z_]*$");
  ASSERT_TRUE(pattern->ok());
  std::vector<std::shared_ptr<re2::RE2 const>> patterns({std::move(pattern)});

  std::map<std::string, int> unfiltered{
      {"NO_MATCH", 0}, {"match", 1}, {"another_match", 2}};
  RegexFiteredMapView<decltype(unfiltered)> filtered(unfiltered, patterns);
  EXPECT_EQ(Vec({"match", "another_match"}), Keys(filtered));
}

TEST(RegexFiteredMapView, MultipleFilters) {
  auto has_a = std::make_shared<re2::RE2 const>("a");
  ASSERT_TRUE(has_a->ok());
  auto has_b = std::make_shared<re2::RE2 const>("b");
  ASSERT_TRUE(has_b->ok());
  auto has_c = std::make_shared<re2::RE2 const>("c");
  ASSERT_TRUE(has_c->ok());
  std::vector<std::shared_ptr<re2::RE2 const>> patterns(
      {std::move(has_a), std::move(has_b), std::move(has_c)});

  std::map<std::string, int> unfiltered{
      {"abc", 0}, {"ab", 1}, {"a", 2}, {"QQ b QQ c QQ a QQ", 4}, {"ac", 5}};
  RegexFiteredMapView<decltype(unfiltered)> filtered(unfiltered, patterns);
  EXPECT_EQ(Vec({"abc", "QQ b QQ c QQ a QQ"}), Keys(filtered));
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

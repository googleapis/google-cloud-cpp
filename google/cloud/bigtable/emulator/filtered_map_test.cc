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

TEST(RangeFilteredMapView, NoFilter) {
  std::map<std::string, int> unfiltered{{"zero", 0}, {"one", 1}, {"two", 2}};
  auto filter = StringRangeSet::All();
  RangeFilteredMapView<decltype(unfiltered), StringRangeSet> filtered(
      unfiltered, filter);
  EXPECT_EQ(Vec({"zero", "one", "two"}), Keys(filtered));
}

TEST(RangeFilteredMapView, EmptyFilter) {
  std::map<std::string, int> unfiltered{{"zero", 0}, {"one", 1}, {"two", 2}};
  auto filter = StringRangeSet::Empty();
  RangeFilteredMapView<decltype(unfiltered), StringRangeSet> filtered(
      unfiltered, filter);
  EXPECT_EQ(Vec({}), Keys(filtered));
}

TEST(RangeFilteredMapView, OneOpen) {
  std::map<std::string, int> unfiltered{{"AA", 0},   {"AAA", 0}, {"AAAa", 0},
                                        {"AAAb", 0}, {"AAB", 0}, {"AAC", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Sum(StringRangeSet::Range("AAA", kOpen, "AAB", kOpen));
  RangeFilteredMapView<decltype(unfiltered), StringRangeSet> filtered(
      unfiltered, filter);
  EXPECT_EQ(Vec({"AAAa", "AAAb"}), Keys(filtered));
}

TEST(RangeFilteredMapView, OneClosed) {
  std::map<std::string, int> unfiltered{{"AA", 0},   {"AAA", 0}, {"AAAa", 0},
                                        {"AAAb", 0}, {"AAB", 0}, {"AAC", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Sum(StringRangeSet::Range("AAA", kClosed, "AAB", kClosed));
  RangeFilteredMapView<decltype(unfiltered), StringRangeSet> filtered(
      unfiltered, filter);
  EXPECT_EQ(Vec({"AAA", "AAAa", "AAAb", "AAB"}), Keys(filtered));
}

TEST(RangeFilteredMapView, NoEntriesAfterClosedFilter) {
  std::map<std::string, int> unfiltered{
      {"AA", 0}, {"AAA", 0}, {"AAAa", 0}, {"AAAb", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Sum(StringRangeSet::Range("AAA", kClosed, "AAB", kClosed));
  RangeFilteredMapView<decltype(unfiltered), StringRangeSet> filtered(
      unfiltered, filter);
  EXPECT_EQ(Vec({"AAA", "AAAa", "AAAb"}), Keys(filtered));
}

TEST(RangeFilteredMapView, NoEntriesAfterOpenFilter) {
  std::map<std::string, int> unfiltered{
      {"AA", 0}, {"AAA", 0}, {"AAAa", 0}, {"AAAb", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Sum(StringRangeSet::Range("AAA", kOpen, "AAB", kOpen));
  RangeFilteredMapView<decltype(unfiltered), StringRangeSet> filtered(
      unfiltered, filter);
  EXPECT_EQ(Vec({"AAAa", "AAAb"}), Keys(filtered));
}

TEST(RangeFilteredMapView, NoEntriesBeforeClosedFilter) {
  std::map<std::string, int> unfiltered{
      {"AAA", 0}, {"AAAa", 0}, {"AAAb", 0}, {"AAB", 0}, {"AAC", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Sum(StringRangeSet::Range("AAA", kClosed, "AAB", kClosed));
  RangeFilteredMapView<decltype(unfiltered), StringRangeSet> filtered(
      unfiltered, filter);
  EXPECT_EQ(Vec({"AAA", "AAAa", "AAAb", "AAB"}), Keys(filtered));
}

TEST(RangeFilteredMapView, NoEntriesBeforeOpenFilter) {
  std::map<std::string, int> unfiltered{
      {"AAAa", 0}, {"AAAb", 0}, {"AAB", 0}, {"AAC", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Sum(StringRangeSet::Range("AAA", kOpen, "AAB", kOpen));
  RangeFilteredMapView<decltype(unfiltered), StringRangeSet> filtered(
      unfiltered, filter);
  EXPECT_EQ(Vec({"AAAa", "AAAb"}), Keys(filtered));
}

TEST(RangeFilteredMapView, MultipleFilters) {
  std::map<std::string, int> unfiltered{
      {"AA", 0},   {"AAA", 0}, {"AAAa", 0}, {"AAAb", 0}, {"AAB", 0},
      {"AAC", 0},  {"BB", 0},  {"BBB", 0},  {"BBBb", 0}, {"CCCa", 0},
      {"CCCb", 0}, {"CCD", 0}, {"CCE", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Sum(StringRangeSet::Range("AAA", kOpen, "AAB", kClosed));
  filter.Sum(StringRangeSet::Range("BBB", kClosed, "BBC", kOpen));
  filter.Sum(StringRangeSet::Range("CCC", kClosed, "CCD", kOpen));
  RangeFilteredMapView<decltype(unfiltered), StringRangeSet> filtered(
      unfiltered, filter);

  EXPECT_EQ(Vec({"AAAa", "AAAb", "AAB", "BBB", "BBBb", "CCCa", "CCCb"}),
            Keys(filtered));
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

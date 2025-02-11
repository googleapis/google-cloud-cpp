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
#include "google/cloud/testing_util/is_proto_equal.h"
#include "google/cloud/testing_util/status_matchers.h"
#include "google/cloud/testing_util/chrono_literals.h"
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

std::vector<std::string> Vec(std::initializer_list<char const *> const &v) {
  std::vector<std::string> res;
  std::transform(v.begin(), v.end(), std::back_inserter(res),
                 [](char const* s) { return std::string(s); });
  std::sort(res.begin(), res.end());
  return res;
}

TEST(FilteredMap, NoFilter) {
  std::map<std::string, int> unfiltered{{"zero", 0}, {"one", 1}, {"two", 2}};
  auto filter = StringRangeSet::All();
  FilteredMapView<decltype(unfiltered), StringRangeSet> filtered(unfiltered,
                                                                 filter);
  EXPECT_EQ(Vec({"zero", "one", "two"}), Keys(filtered));
}

TEST(FilteredMap, EmptyFilter) {
  std::map<std::string, int> unfiltered{{"zero", 0}, {"one", 1}, {"two", 2}};
  auto filter = StringRangeSet::Empty();
  FilteredMapView<decltype(unfiltered), StringRangeSet> filtered(unfiltered,
                                                                 filter);
  EXPECT_EQ(Vec({}), Keys(filtered));
}

TEST(FilteredMap, OneOpen) {
  std::map<std::string, int> unfiltered{{"AA", 0},   {"AAA", 0}, {"AAAa", 0},
                                        {"AAAb", 0}, {"AAB", 0}, {"AAC", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Insert(StringRangeSet::Range("AAA", kOpen, "AAB", kOpen));
  FilteredMapView<decltype(unfiltered), StringRangeSet> filtered(unfiltered,
                                                                 filter);
  EXPECT_EQ(Vec({"AAAa", "AAAb"}), Keys(filtered));
}

TEST(FilteredMap, OneClosed) {
  std::map<std::string, int> unfiltered{{"AA", 0},   {"AAA", 0}, {"AAAa", 0},
                                        {"AAAb", 0}, {"AAB", 0}, {"AAC", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Insert(StringRangeSet::Range("AAA", kClosed, "AAB", kClosed));
  FilteredMapView<decltype(unfiltered), StringRangeSet> filtered(unfiltered,
                                                                 filter);
  EXPECT_EQ(Vec({"AAA", "AAAa", "AAAb", "AAB"}), Keys(filtered));
}

TEST(FilteredMap, NoEntriesAfterClosedFilter) {
  std::map<std::string, int> unfiltered{{"AA", 0},   {"AAA", 0}, {"AAAa", 0},
                                        {"AAAb", 0}}; 
  auto filter = StringRangeSet::Empty();
  filter.Insert(StringRangeSet::Range("AAA", kClosed, "AAB", kClosed));
  FilteredMapView<decltype(unfiltered), StringRangeSet> filtered(unfiltered,
                                                                 filter);
  EXPECT_EQ(Vec({"AAA", "AAAa", "AAAb"}), Keys(filtered));
}

TEST(FilteredMap, NoEntriesAfterOpenFilter) {
  std::map<std::string, int> unfiltered{{"AA", 0},   {"AAA", 0}, {"AAAa", 0},
                                        {"AAAb", 0}}; 
  auto filter = StringRangeSet::Empty();
  filter.Insert(StringRangeSet::Range("AAA", kOpen, "AAB", kOpen));
  FilteredMapView<decltype(unfiltered), StringRangeSet> filtered(unfiltered,
                                                                 filter);
  EXPECT_EQ(Vec({"AAAa", "AAAb"}), Keys(filtered));
}

TEST(FilteredMap, NoEntriesBeforeClosedFilter) {
  std::map<std::string, int> unfiltered{{"AAA", 0}, {"AAAa", 0},
                                        {"AAAb", 0}, {"AAB", 0}, {"AAC", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Insert(StringRangeSet::Range("AAA", kClosed, "AAB", kClosed));
  FilteredMapView<decltype(unfiltered), StringRangeSet> filtered(unfiltered,
                                                                 filter);
  EXPECT_EQ(Vec({"AAA", "AAAa", "AAAb", "AAB"}), Keys(filtered));
}

TEST(FilteredMap, NoEntriesBeforeOpenFilter) {
  std::map<std::string, int> unfiltered{{"AAAa", 0},
                                        {"AAAb", 0}, {"AAB", 0}, {"AAC", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Insert(StringRangeSet::Range("AAA", kOpen, "AAB", kOpen));
  FilteredMapView<decltype(unfiltered), StringRangeSet> filtered(unfiltered,
                                                                 filter);
  EXPECT_EQ(Vec({"AAAa", "AAAb"}), Keys(filtered));
}

TEST(FilteredMap, MultipleFilters) {
  std::map<std::string, int> unfiltered{
      {"AA", 0},   {"AAA", 0},  {"AAAa", 0}, {"AAAb", 0}, {"AAB", 0},
      {"AAC", 0},  {"BB", 0},   {"BBB", 0},  {"BBBa", 0}, {"BBBb", 0},
      {"CCCa", 0}, {"CCCb", 0}, {"CCD", 0},  {"CCE", 0}};
  auto filter = StringRangeSet::Empty();
  filter.Insert(StringRangeSet::Range("AAA", kOpen, "AAB", kClosed));
  filter.Insert(StringRangeSet::Range("BBB", kClosed, "BBC", kOpen));
  filter.Insert(StringRangeSet::Range("CCC", kClosed, "CCD", kOpen));
  FilteredMapView<decltype(unfiltered), StringRangeSet> filtered(unfiltered,
                                                                 filter);

  EXPECT_EQ(Vec({"AAAa", "AAAb", "AAB", "BBB", "BBBa", "BBBb", "CCCa", "CCCb"}),
            Keys(filtered));
}

}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

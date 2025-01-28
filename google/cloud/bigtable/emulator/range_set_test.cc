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

#include "google/cloud/bigtable/emulator/range_set.h"
#include "google/cloud/bigtable/row_range.h"
#include "google/cloud/testing_util/is_proto_equal.h"
#include <google/protobuf/text_format.h>
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
namespace emulator {
namespace {

namespace btproto = ::google::bigtable::v2;
using ::google::cloud::testing_util::IsProtoEqual;

TEST(StringRangeSet, SingleRange) {
  StringRangeSet srs;
  srs.Insert(StringRangeSet::Range("a", false, "b", false));
  ASSERT_EQ(1, srs.disjoint_ranges().size());
  ASSERT_EQ(StringRangeSet::Range("a", false, "b", false),
            *srs.disjoint_ranges().begin());
}

TEST(StartLess, Order) {
//  using StartLess = internal::RowRangeHelpers::StartLess;
//
//  ASSERT_FALSE(StartLess()(RowRange::Closed("a", "").as_proto(),
//                           RowRange::Closed("a", "").as_proto()));
//  ASSERT_TRUE(StartLess()(RowRange::Closed("a", "").as_proto(),
//                          RowRange::Open("a", "").as_proto()));
//  ASSERT_FALSE(StartLess()(RowRange::Open("a", "").as_proto(),
//                          RowRange::Closed("a", "").as_proto()));
//  ASSERT_TRUE(StartLess()(RowRange::Closed("a", "").as_proto(),
//                          RowRange::Closed("b", "").as_proto()));
//  ASSERT_TRUE(StartLess()(RowRange::InfiniteRange().as_proto(),
//                          RowRange::Closed("a", "").as_proto()));
//  ASSERT_TRUE(StartLess()(RowRange::InfiniteRange().as_proto(),
//                          RowRange::Open("a", "").as_proto()));
//  ASSERT_FALSE(StartLess()(RowRange::InfiniteRange().as_proto(),
//                           RowRange::InfiniteRange().as_proto()));
}

}  // anonymous namespace
}  // namespace emulator
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

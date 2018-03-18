//   Copyright 2017 Google Inc.
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

#include "bigtable/client/column_family.h"
#include "bigtable/client/testing/chrono_literals.h"
#include <gmock/gmock.h>

using namespace bigtable::chrono_literals;

TEST(GcRule, MaxNumVersions) {
  auto proto = bigtable::GcRule::MaxNumVersions(3).as_proto();
  EXPECT_EQ(3, proto.max_num_versions());
}

TEST(GcRule, MaxAgeHours) {
  auto proto = bigtable::GcRule::MaxAge(1_h).as_proto();
  EXPECT_EQ(3600, proto.max_age().seconds());
  EXPECT_EQ(0, proto.max_age().nanos());
}

TEST(GcRule, MaxAgeMinutes) {
  auto proto = bigtable::GcRule::MaxAge(2_min).as_proto();
  EXPECT_EQ(120, proto.max_age().seconds());
  EXPECT_EQ(0, proto.max_age().nanos());
}

TEST(GcRule, MaxAgeSeconds) {
  auto proto = bigtable::GcRule::MaxAge(3_s).as_proto();
  EXPECT_EQ(3, proto.max_age().seconds());
  EXPECT_EQ(0, proto.max_age().nanos());
}

TEST(GcRule, MaxAgeMicroseconds) {
  auto proto = bigtable::GcRule::MaxAge(5_us).as_proto();
  EXPECT_EQ(0, proto.max_age().seconds());
  EXPECT_EQ(5000, proto.max_age().nanos());
}

TEST(GcRule, MaxAgeNanoseconds) {
  auto proto = bigtable::GcRule::MaxAge(6_ns).as_proto();
  EXPECT_EQ(0, proto.max_age().seconds());
  EXPECT_EQ(6, proto.max_age().nanos());
}

TEST(GcRule, MaxAgeMixed) {
  auto proto = bigtable::GcRule::MaxAge(1_min + 2_s + 7_ns).as_proto();
  EXPECT_EQ(62, proto.max_age().seconds());
  EXPECT_EQ(7, proto.max_age().nanos());
}

TEST(GcRule, IntersectionSingle) {
  using GC = bigtable::GcRule;
  auto proto = GC::Intersection(GC::MaxNumVersions(42)).as_proto();
  EXPECT_TRUE(proto.has_intersection());
  EXPECT_EQ(1, proto.intersection().rules_size());
  EXPECT_EQ(42, proto.intersection().rules(0).max_num_versions());
}

TEST(GcRule, IntersectionMultiple) {
  using GC = bigtable::GcRule;
  auto proto = GC::Intersection(GC::MaxNumVersions(42), GC::MaxAge(2_s + 3_us))
                   .as_proto();
  EXPECT_TRUE(proto.has_intersection());
  EXPECT_EQ(2, proto.intersection().rules_size());
  EXPECT_EQ(42, proto.intersection().rules(0).max_num_versions());
  EXPECT_EQ(2, proto.intersection().rules(1).max_age().seconds());
  EXPECT_EQ(3000, proto.intersection().rules(1).max_age().nanos());
}

TEST(GcRule, IntersectionNone) {
  using GC = bigtable::GcRule;
  auto proto = GC::Intersection().as_proto();
  EXPECT_TRUE(proto.has_intersection());
  EXPECT_EQ(0, proto.intersection().rules_size());
}

TEST(GcRule, UnionSingle) {
  using GC = bigtable::GcRule;
  auto proto = GC::Union(GC::MaxNumVersions(42)).as_proto();
  EXPECT_TRUE(proto.has_union_());
  EXPECT_EQ(1, proto.union_().rules_size());
  EXPECT_EQ(42, proto.union_().rules(0).max_num_versions());
}

TEST(GcRule, UnionMultiple) {
  using GC = bigtable::GcRule;
  auto proto =
      GC::Union(GC::MaxNumVersions(42), GC::MaxAge(2_s + 3_us)).as_proto();
  EXPECT_TRUE(proto.has_union_());
  EXPECT_EQ(2, proto.union_().rules_size());
  EXPECT_EQ(42, proto.union_().rules(0).max_num_versions());
  EXPECT_EQ(2, proto.union_().rules(1).max_age().seconds());
  EXPECT_EQ(3000, proto.union_().rules(1).max_age().nanos());
}

TEST(GcRule, UnionNone) {
  using GC = bigtable::GcRule;
  auto proto = GC::Union().as_proto();
  EXPECT_TRUE(proto.has_union_());
  EXPECT_EQ(0, proto.union_().rules_size());
}

TEST(ColumnFamilyModification, Create) {
  using M = bigtable::ColumnFamilyModification;
  using GC = bigtable::GcRule;
  auto proto = M::Create("foo", GC::MaxNumVersions(2)).as_proto();
  ASSERT_TRUE(proto.has_create());
  EXPECT_EQ("foo", proto.id());
  EXPECT_EQ(2, proto.create().gc_rule().max_num_versions());
}

TEST(ColumnFamilyModification, Update) {
  using M = bigtable::ColumnFamilyModification;
  using GC = bigtable::GcRule;
  auto proto = M::Update("foo", GC::MaxNumVersions(2)).as_proto();
  ASSERT_TRUE(proto.has_update());
  EXPECT_EQ("foo", proto.id());
  EXPECT_EQ(2, proto.update().gc_rule().max_num_versions());
}

TEST(ColumnFamilyModification, Drop) {
  using M = bigtable::ColumnFamilyModification;
  auto proto = M::Drop("foo").as_proto();
  EXPECT_TRUE(proto.drop());
  EXPECT_EQ("foo", proto.id());
}

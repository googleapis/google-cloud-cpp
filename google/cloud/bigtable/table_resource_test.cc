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

#include "google/cloud/bigtable/table_resource.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <sstream>
#include <string>

namespace google {
namespace cloud {
namespace bigtable {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::google::cloud::testing_util::StatusIs;

TEST(TableResource, Basics) {
  InstanceResource in(Project("p1"), "i1");
  TableResource tr(in, "t1");
  EXPECT_EQ("t1", tr.table_id());
  EXPECT_EQ(in, tr.instance());
  EXPECT_EQ("projects/p1/instances/i1/tables/t1", tr.FullName());

  auto copy = tr;
  EXPECT_EQ(copy, tr);
  EXPECT_EQ("t1", copy.table_id());
  EXPECT_EQ(in, copy.instance());
  EXPECT_EQ("projects/p1/instances/i1/tables/t1", copy.FullName());

  auto moved = std::move(copy);
  EXPECT_EQ(moved, tr);
  EXPECT_EQ("t1", moved.table_id());
  EXPECT_EQ(in, moved.instance());
  EXPECT_EQ("projects/p1/instances/i1/tables/t1", moved.FullName());

  InstanceResource in2(Project("p2"), "i2");
  TableResource tr2(in2, "t2");
  EXPECT_NE(tr2, tr);
  EXPECT_EQ("t2", tr2.table_id());
  EXPECT_EQ(in2, tr2.instance());
  EXPECT_EQ("projects/p2/instances/i2/tables/t2", tr2.FullName());

  TableResource tr_string_ctor("p1", "i1", "t1");
  EXPECT_EQ(tr_string_ctor, tr);
  EXPECT_EQ("t1", tr_string_ctor.table_id());
  EXPECT_EQ(in, tr_string_ctor.instance());
  EXPECT_EQ("projects/p1/instances/i1/tables/t1", tr_string_ctor.FullName());
}

TEST(TableResource, OutputStream) {
  InstanceResource in(Project("p1"), "i1");
  TableResource tr(in, "t1");
  std::ostringstream os;
  os << tr;
  EXPECT_EQ("projects/p1/instances/i1/tables/t1", os.str());
}

TEST(TableResource, MakeTableResource) {
  auto tr = TableResource(InstanceResource(Project("p1"), "i1"), "t1");
  EXPECT_EQ(tr, MakeTableResource(tr.FullName()).value());

  for (std::string invalid : {
           "",
           "projects/",
           "projects/p1",
           "projects/p1/instances/",
           "projects/p1/instances/i1",
           "projects/p1/instances/i1/tables",
           "projects/p1/instances/i1/tables/",
           "/projects/p1/instances/i1/tables/t1",
           "projects/p1/instances/i1/tables/t1/",
           "projects/p1/instances/i1/tables/t1/etc",
       }) {
    auto tr = MakeTableResource(invalid);
    EXPECT_THAT(tr, StatusIs(StatusCode::kInvalidArgument,
                             "Improperly formatted TableResource: " + invalid));
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

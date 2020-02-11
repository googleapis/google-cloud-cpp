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

#include "google/cloud/pubsub/topic.h"
#include <gmock/gmock.h>
#include <sstream>

namespace google {
namespace cloud {
namespace pubsub {
inline namespace GOOGLE_CLOUD_CPP_PUBSUB_NS {
namespace {

TEST(Topic, Basics) {
  Topic in("p1", "t1");
  EXPECT_EQ("p1", in.project_id());
  EXPECT_EQ("t1", in.topic_id());
  EXPECT_EQ("projects/p1/topics/t1", in.FullName());

  auto copy = in;
  EXPECT_EQ(copy, in);
  EXPECT_EQ("p1", copy.project_id());
  EXPECT_EQ("t1", copy.topic_id());
  EXPECT_EQ("projects/p1/topics/t1", copy.FullName());

  auto moved = std::move(copy);
  EXPECT_EQ(moved, in);
  EXPECT_EQ("p1", moved.project_id());
  EXPECT_EQ("t1", moved.topic_id());
  EXPECT_EQ("projects/p1/topics/t1", moved.FullName());

  Topic in2("p2", "t2");
  EXPECT_NE(in2, in);
  EXPECT_EQ("p2", in2.project_id());
  EXPECT_EQ("t2", in2.topic_id());
  EXPECT_EQ("projects/p2/topics/t2", in2.FullName());
}

TEST(Topic, OutputStream) {
  Topic in("p1", "t1");
  std::ostringstream os;
  os << in;
  EXPECT_EQ("projects/p1/topics/t1", os.str());
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_PUBSUB_NS
}  // namespace pubsub
}  // namespace cloud
}  // namespace google

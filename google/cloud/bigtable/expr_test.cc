// Copyright 2019 Google Inc.
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

#include "google/cloud/bigtable/expr.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace bigtable {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

TEST(ExprTest, Trivial) {
  auto expr =
      Expression("request.host == \"hr.example.com\"", "title", "descr", "loc");
  EXPECT_EQ("request.host == \"hr.example.com\"", expr.expression());
  EXPECT_EQ("title", expr.title());
  EXPECT_EQ("descr", expr.description());
  EXPECT_EQ("loc", expr.location());
}

TEST(ExprTest, Printing) {
  auto expr = Expression("request.host == \"hr.example.com\"");
  {
    std::stringstream stream;
    stream << expr;
    EXPECT_EQ("(request.host == \"hr.example.com\")", stream.str());
  }
  expr.set_title("title");
  {
    std::stringstream stream;
    stream << expr;
    EXPECT_EQ("(request.host == \"hr.example.com\", title=\"title\")",
              stream.str());
  }
  expr.set_description("descr");
  {
    std::stringstream stream;
    stream << expr;
    EXPECT_EQ(
        "(request.host == \"hr.example.com\", title=\"title\", "
        "description=\"descr\")",
        stream.str());
  }
  expr.set_location("loc");
  {
    std::stringstream stream;
    stream << expr;
    EXPECT_EQ(
        "(request.host == \"hr.example.com\", title=\"title\", "
        "description=\"descr\", location=\"loc\")",
        stream.str());
  }
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace bigtable
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/internal/rest_context.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::Eq;
using ::testing::Not;

class RestContextTest : public ::testing::Test {
 protected:
  void SetUp() override {
    headers_["header1"] = {"value1"};
    headers_["header2"] = {"value2a", "value2b"};
  }

  RestContext::HttpHeaders headers_;
};

TEST_F(RestContextTest, RvalueBuilder) {
  auto context = RestContext()
                     .AddHeader("header1", "value1")
                     .AddHeader(std::make_pair("header2", "value2a"))
                     .AddHeader("header2", "value2b");
  EXPECT_THAT(
      context.headers(),
      Contains(std::make_pair("header1", std::vector<std::string>{"value1"})));
  EXPECT_THAT(context.headers(),
              Contains(std::make_pair(
                  "header2", std::vector<std::string>{"value2a", "value2b"})));
}

TEST_F(RestContextTest, GetHeaderNotFound) {
  RestContext context(headers_);
  auto result = context.GetHeader("NotFound");
  EXPECT_TRUE(result.empty());
  result = context.GetHeader("notfound");
  EXPECT_TRUE(result.empty());
}

TEST_F(RestContextTest, GetHeaderFound) {
  RestContext context(headers_);
  auto result = context.GetHeader("Header1");
  EXPECT_THAT(result.size(), Eq(1));
  EXPECT_THAT(result, Contains("value1"));
  result = context.GetHeader("header1");
  EXPECT_THAT(result.size(), Eq(1));
  EXPECT_THAT(result, Contains("value1"));
}

TEST_F(RestContextTest, Equality) {
  RestContext lhs(headers_);
  EXPECT_THAT(lhs, Eq(lhs));
  RestContext rhs;
  EXPECT_THAT(lhs, Not(Eq(rhs)));
  rhs.AddHeader("header1", "value1");
  rhs.AddHeader({"header2", "value2a"});
  EXPECT_THAT(lhs, Not(Eq(rhs)));
  rhs.AddHeader("header2", "value2b");
  EXPECT_THAT(lhs, Eq(rhs));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

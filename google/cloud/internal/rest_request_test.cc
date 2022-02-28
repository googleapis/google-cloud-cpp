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

#include "google/cloud/internal/rest_request.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace rest_internal {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::Eq;

class RestRequestTest : public ::testing::Test {
 protected:
  void SetUp() override {
    headers_["header1"] = {"value1"};
    headers_["header2"] = {"value2a", "value2b"};
    parameters_.emplace_back("param1", "value1");
  }

  RestRequest::HttpHeaders headers_;
  RestRequest::HttpParameters parameters_;
};

TEST_F(RestRequestTest, ConstructorPath) {
  RestRequest request("foo/bar");
  EXPECT_THAT(request.path(), Eq("foo/bar"));
  EXPECT_TRUE(request.headers().empty());
  EXPECT_TRUE(request.parameters().empty());
}

TEST_F(RestRequestTest, ConstructorPathHeaders) {
  RestRequest request("foo/bar", headers_);
  EXPECT_THAT(request.path(), Eq("foo/bar"));
  EXPECT_TRUE(request.parameters().empty());
  EXPECT_THAT(
      request.headers(),
      Contains(std::make_pair("header1", std::vector<std::string>{"value1"})));
  EXPECT_THAT(request.headers(),
              Contains(std::make_pair(
                  "header2", std::vector<std::string>{"value2a", "value2b"})));
}

TEST_F(RestRequestTest, ConstructorPathParameters) {
  RestRequest request("foo/bar", parameters_);
  EXPECT_THAT(request.path(), Eq("foo/bar"));
  EXPECT_TRUE(request.headers().empty());
  EXPECT_THAT(request.parameters(),
              Contains(std::make_pair("param1", "value1")));
}

TEST_F(RestRequestTest, ConstructorPathHeadersParameters) {
  RestRequest request("foo/bar", headers_, parameters_);
  EXPECT_THAT(request.path(), Eq("foo/bar"));
  EXPECT_THAT(
      request.headers(),
      Contains(std::make_pair("header1", std::vector<std::string>{"value1"})));
  EXPECT_THAT(request.headers(),
              Contains(std::make_pair(
                  "header2", std::vector<std::string>{"value2a", "value2b"})));
  EXPECT_THAT(request.parameters(),
              Contains(std::make_pair("param1", "value1")));
}

TEST_F(RestRequestTest, RvalueBuilder) {
  auto request = RestRequest()
                     .SetPath("foo/bar")
                     .AddHeader("header1", "value1")
                     .AddHeader(std::make_pair("header2", "value2a"))
                     .AddHeader("header2", "value2b")
                     .AddQueryParameter("param1", "value1")
                     .AddQueryParameter(std::make_pair("param2", "value2"));
  EXPECT_THAT(request.path(), Eq("foo/bar"));
  EXPECT_THAT(
      request.headers(),
      Contains(std::make_pair("header1", std::vector<std::string>{"value1"})));
  EXPECT_THAT(request.headers(),
              Contains(std::make_pair(
                  "header2", std::vector<std::string>{"value2a", "value2b"})));
  ASSERT_THAT(request.parameters().size(), Eq(2));
  EXPECT_THAT(request.parameters()[0],
              Eq(std::make_pair(std::string("param1"), std::string("value1"))));
  EXPECT_THAT(request.parameters()[1],
              Eq(std::make_pair(std::string("param2"), std::string("value2"))));
}

TEST_F(RestRequestTest, AppendPath) {
  auto request = RestRequest().AppendPath("able");
  EXPECT_THAT(request.path(), Eq("able"));
  request.AppendPath("baker");
  EXPECT_THAT(request.path(), Eq("able/baker"));
  request.AppendPath("/charlie");
  EXPECT_THAT(request.path(), Eq("able/baker/charlie"));
  request.AppendPath("delta/").AppendPath("/echo");
  EXPECT_THAT(request.path(), Eq("able/baker/charlie/delta/echo"));
}

TEST_F(RestRequestTest, GetHeaderNotFound) {
  RestRequest request("foo/bar", headers_);
  auto result = request.GetHeader("NotFound");
  EXPECT_TRUE(result.empty());
  result = request.GetHeader("notfound");
  EXPECT_TRUE(result.empty());
}

TEST_F(RestRequestTest, GetHeaderFound) {
  RestRequest request("foo/bar", headers_);
  auto result = request.GetHeader("Header1");
  EXPECT_THAT(result.size(), Eq(1));
  EXPECT_THAT(result, Contains("value1"));
  result = request.GetHeader("header1");
  EXPECT_THAT(result.size(), Eq(1));
  EXPECT_THAT(result, Contains("value1"));
}

TEST_F(RestRequestTest, GetQueryParameterNotFound) {
  RestRequest request("foo/bar", parameters_);
  auto result = request.GetQueryParameter("NotFound");
  EXPECT_TRUE(result.empty());
}

TEST_F(RestRequestTest, GetQueryParameterFoundOnce) {
  RestRequest request("foo/bar", parameters_);
  auto result = request.GetQueryParameter("Param1");
  EXPECT_TRUE(result.empty());
  result = request.GetQueryParameter("param1");
  ASSERT_THAT(result.size(), Eq(1));
  EXPECT_THAT(result[0], Eq("value1"));
}

TEST_F(RestRequestTest, GetQueryParameterFoundMoreThanOnce) {
  parameters_.emplace_back("param1", "value1b");
  RestRequest request("foo/bar", parameters_);
  auto result = request.GetQueryParameter("Param1");
  EXPECT_TRUE(result.empty());
  result = request.GetQueryParameter("param1");
  ASSERT_THAT(result.size(), Eq(2));
  EXPECT_THAT(result[0], Eq("value1"));
  EXPECT_THAT(result[1], Eq("value1b"));
}

TEST_F(RestRequestTest, Equality) {
  RestRequest lhs("foo/bar", headers_, parameters_);
  EXPECT_TRUE(lhs == lhs);
  RestRequest rhs;
  EXPECT_TRUE(lhs != rhs);
  rhs.SetPath("foo/bar");
  EXPECT_TRUE(lhs != rhs);
  rhs.AddHeader("header1", "value1");
  rhs.AddHeader({"header2", "value2a"});
  EXPECT_TRUE(lhs != rhs);
  rhs.AddHeader("header2", "value2b");
  EXPECT_TRUE(lhs != rhs);
  rhs.AddQueryParameter({"param1", "value1"});
  EXPECT_TRUE(lhs == rhs);
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace rest_internal
}  // namespace cloud
}  // namespace google

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

#include "google/cloud/internal/routing_matcher.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace internal {
namespace {

using ::testing::UnorderedElementsAre;

// Simulate a protobuf message with two string fields: `foo` and `bar`.
struct TestRequest {
  std::string const& foo() const { return foo_; };
  std::string const& bar() const { return bar_; };

  std::string foo_;
  std::string bar_;
};

TEST(RoutingMatcher, NoAppendIfNoMatch) {
  auto matcher = RoutingMatcher<TestRequest>{
      "routing_id=",
      {
          {[](TestRequest const& request) -> std::string const& {
             return request.foo();
           },
           std::regex{"baz/([^/]+)"}},
      }};

  std::vector<std::string> params = {"previous"};
  auto request = TestRequest{"foo/foo", "bar/bar"};
  matcher.AppendParam(request, params);
  EXPECT_THAT(params, UnorderedElementsAre("previous"));
}

TEST(RoutingMatcher, MatchesAll) {
  auto matcher = RoutingMatcher<TestRequest>{
      "routing_id=",
      {
          {[](TestRequest const& request) -> std::string const& {
             return request.foo();
           },
           absl::nullopt},
      }};

  std::vector<std::string> params = {"previous"};
  auto request = TestRequest{"foo/foo", "bar/bar"};
  matcher.AppendParam(request, params);
  EXPECT_THAT(params, UnorderedElementsAre("previous", "routing_id=foo/foo"));
}

TEST(RoutingMatcher, EmptyFieldIsSkipped) {
  auto matcher = RoutingMatcher<TestRequest>{
      "routing_id=",
      {
          {[](TestRequest const& request) -> std::string const& {
             return request.foo();
           },
           absl::nullopt},
          {[](TestRequest const& request) -> std::string const& {
             return request.bar();
           },
           std::regex{"bar/([^/]+)"}},
      }};

  std::vector<std::string> params = {"previous"};
  auto request = TestRequest{"", "bar/bar"};
  matcher.AppendParam(request, params);
  EXPECT_THAT(params, UnorderedElementsAre("previous", "routing_id=bar"));
}

TEST(RoutingMatcher, FirstNonEmptyMatchIsUsed) {
  auto matcher = RoutingMatcher<TestRequest>{
      "routing_id=",
      {
          {[](TestRequest const& request) -> std::string const& {
             return request.foo();
           },
           std::regex{"foo/([^/]+)"}},
          {[](TestRequest const& request) -> std::string const& {
             return request.bar();
           },
           std::regex{"bar/([^/]+)"}},
      }};

  std::vector<std::string> params = {"previous"};
  auto request = TestRequest{"foo/foo", "bar/bar"};
  matcher.AppendParam(request, params);
  EXPECT_THAT(params, UnorderedElementsAre("previous", "routing_id=foo"));
}

}  // namespace
}  // namespace internal
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google

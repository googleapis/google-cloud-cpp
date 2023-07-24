// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "generator/internal/http_annotation_parser.h"
#include "google/cloud/testing_util/status_matchers.h"
#include <gmock/gmock.h>
#include <algorithm>
#include <sstream>

namespace google {
namespace cloud {
namespace generator_internal {
namespace {

using ::google::cloud::testing_util::StatusIs;
using ::testing::HasSubstr;

std::shared_ptr<PathTemplate::Segment> MakeMatchSegment() {
  return std::make_shared<PathTemplate::Segment>(
      PathTemplate::Segment{PathTemplate::Match{}});
}

std::shared_ptr<PathTemplate::Segment> MakeMatchRecursiveSegment() {
  return std::make_shared<PathTemplate::Segment>(
      PathTemplate::Segment{PathTemplate::MatchRecursive{}});
}

std::shared_ptr<PathTemplate::Segment> MakeSegment(absl::string_view v) {
  return std::make_shared<PathTemplate::Segment>(
      PathTemplate::Segment{std::string(v)});
}

std::shared_ptr<PathTemplate::Segment> MakeSegment(PathTemplate::Variable v) {
  return std::make_shared<PathTemplate::Segment>(
      PathTemplate::Segment{std::move(v)});
}

bool SameValues(PathTemplate::Segment const& a, PathTemplate::Segment const& b);

bool SameValues(PathTemplate::Segments const& a,
                PathTemplate::Segments const& b) {
  return std::equal(
      a.begin(), a.end(), b.begin(), b.end(),
      [](auto const& lhs, auto const& rhs) { return SameValues(*lhs, *rhs); });
}

bool SameValues(PathTemplate::Variable const& a,
                PathTemplate::Variable const& b) {
  return a.field_path == b.field_path && SameValues(a.segments, b.segments);
}

bool SameValues(PathTemplate::Segment const& a,
                PathTemplate::Segment const& b) {
  struct Visitor {
    PathTemplate::Segment const& a;
    bool operator()(PathTemplate::Match const&) {
      return absl::holds_alternative<PathTemplate::Match>(a.value);
    }
    bool operator()(PathTemplate::MatchRecursive const&) {
      return absl::holds_alternative<PathTemplate::MatchRecursive>(a.value);
    }
    bool operator()(std::string const& s) {
      return absl::holds_alternative<std::string>(a.value) &&
             absl::get<std::string>(a.value) == s;
    }
    bool operator()(PathTemplate::Variable const& v) {
      return absl::holds_alternative<PathTemplate::Variable>(a.value) &&
             SameValues(absl::get<PathTemplate::Variable>(a.value), v);
    }
  };
  return absl::visit(Visitor{a}, b.value);
}

bool SameValues(PathTemplate const& a, PathTemplate const& b) {
  return SameValues(a.segments, b.segments) && a.verb == b.verb;
}

TEST(ParseHttpTemplate, SingleVariableExplicit) {
  auto parsed =
      ParsePathTemplate("/v1/{name=projects/*/instances/*/backups/*}");
  ASSERT_STATUS_OK(parsed);
  auto const expected = PathTemplate{
      /*segments=*/{
          MakeSegment("v1"),
          MakeSegment(PathTemplate::Variable{/*field_path=*/"name",
                                             /*segments=*/
                                             {
                                                 MakeSegment("projects"),
                                                 MakeMatchSegment(),
                                                 MakeSegment("instances"),
                                                 MakeMatchSegment(),
                                                 MakeSegment("backups"),
                                                 MakeMatchSegment(),
                                             }}),
      },
      /*verb=*/{}};
  EXPECT_TRUE(SameValues(expected, *parsed))
      << ", expected=" << expected << ", parsed=" << *parsed;
}

TEST(ParseHttpTemplate, NestedFieldPath) {
  auto parsed = ParsePathTemplate("/v1/{instance.name=projects/*/instances/*}");
  ASSERT_STATUS_OK(parsed);
  auto const expected = PathTemplate{
      /*segments=*/{
          MakeSegment("v1"),
          MakeSegment(PathTemplate::Variable{/*field_path=*/"instance.name",
                                             /*segments=*/
                                             {
                                                 MakeSegment("projects"),
                                                 MakeMatchSegment(),
                                                 MakeSegment("instances"),
                                                 MakeMatchSegment(),
                                             }}),
      },
      /*verb=*/{}};
  EXPECT_TRUE(SameValues(expected, *parsed))
      << ", expected=" << expected << ", parsed=" << *parsed;
}

TEST(ParseHttpTemplate, TwoVariableExplicit) {
  auto parsed = ParsePathTemplate(
      "/v1/projects/{project=project}/instances/{instance=instance}");
  ASSERT_STATUS_OK(parsed);
  auto const expected = PathTemplate{
      /*segments=*/{
          MakeSegment("v1"),
          MakeSegment("projects"),
          MakeSegment(PathTemplate::Variable{/*field_path=*/"project",
                                             /*segments=*/
                                             {
                                                 MakeSegment("project"),
                                             }}),
          MakeSegment("instances"),
          MakeSegment(PathTemplate::Variable{/*field_path=*/"instance",
                                             /*segments=*/
                                             {
                                                 MakeSegment("instance"),
                                             }}),
      },
      /*verb=*/{}};
  EXPECT_TRUE(SameValues(expected, *parsed))
      << ", expected=" << expected << ", parsed=" << *parsed;
}

TEST(ParseHttpTemplate, MatcherOutsideVariable) {
  // This is allowed by the grammar, and used in
  // cloud/gkeconnect/v1beta1/gateway.proto
  auto parsed = ParsePathTemplate("/v1/a/*/b/**");
  ASSERT_STATUS_OK(parsed);
  auto const expected = PathTemplate{/*segments=*/{
                                         MakeSegment("v1"),
                                         MakeSegment("a"),
                                         MakeMatchSegment(),
                                         MakeSegment("b"),
                                         MakeMatchRecursiveSegment(),
                                     },
                                     /*verb=*/{}};
  EXPECT_TRUE(SameValues(expected, *parsed))
      << ", expected=" << expected << ", parsed=" << *parsed;
}

TEST(ParseHttpTemplate, Complex) {
  auto parsed = ParsePathTemplate(
      "/v1/{parent=projects/*/databases/*/documents/*/**}/{collection_id}");
  ASSERT_STATUS_OK(parsed);
  auto const expected = PathTemplate{
      /*segments=*/{
          MakeSegment("v1"),
          MakeSegment(PathTemplate::Variable{/*field_path=*/"parent",
                                             /*segments=*/
                                             {
                                                 MakeSegment("projects"),
                                                 MakeMatchSegment(),
                                                 MakeSegment("databases"),
                                                 MakeMatchSegment(),
                                                 MakeSegment("documents"),
                                                 MakeMatchSegment(),
                                                 MakeMatchRecursiveSegment(),
                                             }}),
          MakeSegment(PathTemplate::Variable{/*field_path=*/"collection_id",
                                             /*segments=*/{}}),
      },
      /*verb=*/{}};
  EXPECT_TRUE(SameValues(expected, *parsed))
      << ", expected=" << expected << ", parsed=" << *parsed;
}

TEST(ParseHttpTemplate, WithVerb) {
  auto parsed = ParsePathTemplate("/v1/{project}:put");
  ASSERT_STATUS_OK(parsed);
  auto const expected = PathTemplate{
      /*segments=*/{
          MakeSegment("v1"),
          MakeSegment(PathTemplate::Variable{/*field_path=*/"project",
                                             /*segments=*/{}}),
      },
      /*verb=*/"put"};
  EXPECT_TRUE(SameValues(expected, *parsed))
      << ", expected=" << expected << ", parsed=" << *parsed;
}

TEST(ParseHttpTemplate, Errors) {
  struct {
    std::string input;
    std::string expected;
  } const cases[] = {
      {"v1/projects/{project}", " offset 0\n"},
      {"/", " offset 1\n"},
      {"/:put", " offset 1\n"},
      {"/v1:bad|verb", " offset 7\n"},
      {"/v1:put ", " offset 7\n"},
      {"/v1//", " offset 4\n"},
      {"/v1/|**/", " offset 4\n"},
      {"/v1/a/{p|}", " offset 8\n"},
      {"/v1/a/{p=", " offset 9\n"},
      {"/v1/a/{p=}", " offset 9\n"},
      {"/v1/a/{=abc}", " offset 7\n"},
      {"/v1/a/{p=**|}", " offset 11\n"},
      {"/v1/a/{p", " offset 8\n"},
      {"/v1/a/{p} ", " offset 9\n"},
      {"/v1/a/{p=b/{c}}", " offset 11\n"},
      {"/v1/a/{", " offset 7\n"},
  };
  for (auto const& test : cases) {
    SCOPED_TRACE("Testing with input=" + test.input);
    auto parsed = ParsePathTemplate(test.input);
    EXPECT_THAT(parsed, StatusIs(StatusCode::kInvalidArgument,
                                 HasSubstr(test.expected)));
  }
}

TEST(ParsetHttpTemplate, OStream) {
  auto parsed = ParsePathTemplate("/v1/a/{b=c}/d/{e=f/**/*}:put");
  ASSERT_STATUS_OK(parsed);
  std::ostringstream os;
  os << *parsed;
  EXPECT_EQ(os.str(),
            "{segments=[ {v1} / {a} / {field_path=b, segments=[ {c} ]} / {d} /"
            " {field_path=e, segments=[ {f} / {**} / {*} ]} ], verb=put}");
}

}  // namespace
}  // namespace generator_internal
}  // namespace cloud
}  // namespace google

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

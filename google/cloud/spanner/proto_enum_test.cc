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

#include "google/cloud/spanner/proto_enum.h"
#include "google/cloud/spanner/testing/singer.pb.h"
#include <gmock/gmock.h>
#include <sstream>
#include <string>
#include <vector>

namespace google {
namespace cloud {
namespace spanner {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

TEST(ProtoEnum, TypeName) {
  EXPECT_EQ(ProtoEnum<testing::Genre>::TypeName(),
            "google.cloud.spanner.testing.Genre");
}

TEST(ProtoEnum, DefaultValue) {
  ProtoEnum<testing::Genre> genre;
  EXPECT_EQ(genre, testing::POP);
}

TEST(ProtoEnum, ValueSemantics) {
  auto genre = ProtoEnum<testing::Genre>(testing::FOLK);

  auto copy = genre;
  EXPECT_EQ(copy, genre);

  copy = genre;
  EXPECT_EQ(copy, genre);

  auto move = std::move(genre);
  EXPECT_EQ(move, copy);

  genre = copy;
  move = std::move(genre);
  EXPECT_EQ(move, copy);
}

TEST(ProtoEnum, RoundTrip) {
  for (auto genre :
       {testing::POP, testing::JAZZ, testing::FOLK, testing::ROCK}) {
    EXPECT_EQ(ProtoEnum<testing::Genre>(genre), genre);
  }
}

TEST(ProtoEnum, Conversions) {
  testing::Genre g1(testing::Genre::POP);
  EXPECT_EQ(g1, testing::Genre::POP);
  ProtoEnum<testing::Genre> p1(testing::Genre::POP);
  EXPECT_EQ(p1, testing::Genre::POP);

  g1 = testing::Genre::JAZZ;
  EXPECT_EQ(g1, testing::Genre::JAZZ);
  p1 = testing::Genre::JAZZ;
  EXPECT_EQ(p1, testing::Genre::JAZZ);

  g1 = testing::Genre::FOLK;
  EXPECT_EQ(g1, testing::Genre::FOLK);
  p1 = g1;
  EXPECT_EQ(p1, testing::Genre::FOLK);

  g1 = testing::Genre::ROCK;
  EXPECT_EQ(g1, testing::Genre::ROCK);
  ProtoEnum<testing::Genre> p2(g1);
  EXPECT_EQ(p2, testing::Genre::ROCK);
}

TEST(ProtoEnum, OutputStream) {
  struct TestCase {
    testing::Genre genre;
    std::string expected;
  };

  std::vector<TestCase> test_cases = {
      {testing::POP, "google.cloud.spanner.testing.POP"},
      {testing::JAZZ, "google.cloud.spanner.testing.JAZZ"},
      {testing::FOLK, "google.cloud.spanner.testing.FOLK"},
      {testing::ROCK, "google.cloud.spanner.testing.ROCK"},
      {static_cast<testing::Genre>(42),
       "google.cloud.spanner.testing.Genre.{42}"},
  };

  for (auto const& tc : test_cases) {
    std::ostringstream ss;
    ss << ProtoEnum<testing::Genre>(tc.genre);
    EXPECT_EQ(ss.str(), tc.expected);
  }
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace spanner
}  // namespace cloud
}  // namespace google

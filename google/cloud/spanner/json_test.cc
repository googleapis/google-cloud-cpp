// Copyright 2021 Google LLC
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

#include "google/cloud/spanner/json.h"
#include <gtest/gtest.h>
#include <sstream>
#include <string>

namespace google {
namespace cloud {
namespace spanner {
inline namespace SPANNER_CLIENT_NS {
namespace {

TEST(Json, DefaultCtor) { EXPECT_EQ("null", std::string(Json())); }

TEST(Json, RegularSemantics) {
  Json j("true");

  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  Json const copy1(j);
  EXPECT_EQ(copy1, j);

  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  Json const copy2 = j;
  EXPECT_EQ(copy2, j);

  Json assign;
  assign = j;
  EXPECT_EQ(assign, j);
}

TEST(Json, RelationalOperators) {
  EXPECT_EQ(Json("42"), Json("42"));
  EXPECT_NE(Json("true"), Json(R"("Hello world!")"));

  // We do not even trim whitespace surrounding the JSON string.
  EXPECT_NE(Json(" true "), Json("true"));
}

TEST(Json, RoundTrip) {
  for (auto const& j : {"null", R"("Hello world!")", "42", "true"}) {
    EXPECT_EQ(std::string(Json(j)), j);
  }
}

TEST(Json, OutputStreaming) {
  auto stream = [](Json const& j) {
    std::ostringstream ss;
    ss << j;
    return std::move(ss).str();
  };

  for (auto const& j :
       {Json(), Json(R"("Hello world!")"), Json("42"), Json("true")}) {
    EXPECT_EQ(stream(j), std::string(j));
  }
}

}  // namespace
}  // namespace SPANNER_CLIENT_NS
}  // namespace spanner
}  // namespace cloud
}  // namespace google

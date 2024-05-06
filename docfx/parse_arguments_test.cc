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

#include "docfx/parse_arguments.h"
#include <gmock/gmock.h>
#include <stdexcept>

namespace docfx {
namespace {

using ::testing::StartsWith;

auto constexpr kExpectedUsageCmd = "Usage: cmd <infile> <library> <version>";

TEST(ParseArguments, Basic) {
  auto const actual = ParseArguments({"cmd", "input-file", "library", "4.2"});
  EXPECT_EQ(actual.input_filename, "input-file");
  EXPECT_EQ(actual.library, "library");
  EXPECT_EQ(actual.version, "4.2");
}

TEST(ParseArguments, Help) {
  auto const arguments = std::vector<std::string>({"cmd", "--help"});
  EXPECT_EXIT(
      ParseArguments(arguments), [](int status) { return status == 0; },
      // The usage message goes to stdout, the error log should be empty:
      "");
}

TEST(ParseArguments, NoCommand) {
  EXPECT_THROW(
      try { ParseArguments({}); } catch (std::exception const& ex) {
        EXPECT_THAT(ex.what(),
                    StartsWith("Usage: program-name-missing <infile> "));
        throw;
      },
      std::runtime_error);
}

TEST(ParseArguments, NoArguments) {
  EXPECT_THROW(
      try { ParseArguments({"cmd"}); } catch (std::exception const& ex) {
        EXPECT_THAT(ex.what(), StartsWith(kExpectedUsageCmd));
        throw;
      },
      std::runtime_error);
}

TEST(ParseArguments, TooFewArguments) {
  EXPECT_THROW(
      try { ParseArguments({"cmd", "1"}); } catch (std::exception const& ex) {
        EXPECT_THAT(ex.what(), StartsWith(kExpectedUsageCmd));
        throw;
      },
      std::runtime_error);
}

TEST(ParseArguments, TooManyArguments) {
  EXPECT_THROW(
      try {
        ParseArguments({"cmd", "1", "2", "3", "4"});
      } catch (std::exception const& ex) {
        EXPECT_THAT(ex.what(), StartsWith(kExpectedUsageCmd));
        throw;
      },
      std::runtime_error);
}

}  // namespace
}  // namespace docfx

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

#include "google/cloud/testing_util/command_line_parsing.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace testing_util {
namespace {

using ::testing::HasSubstr;

TEST(CommandLineParsing, UsageSimple) {
  std::vector<OptionDescriptor> desc{
      {"--option1", "help-for-option1", [](std::string const&) {}},
      {"--option2", "help-for-option2", [](std::string const&) {}},
  };
  auto usage = BuildUsage(desc, "command-name");
  EXPECT_THAT(usage, HasSubstr("command-name"));
  EXPECT_THAT(usage, HasSubstr("--option1"));
  EXPECT_THAT(usage, HasSubstr("--option2"));
  EXPECT_THAT(usage, HasSubstr("help-for-option1"));
  EXPECT_THAT(usage, HasSubstr("help-for-option2"));
}

TEST(CommandLineParsing, Empty) {
  OptionDescriptor d{"--unused", "should not be called",
                     [](std::string const& val) { FAIL() << "value=" << val; }};
  auto unparsed = OptionsParse({d}, {});
  EXPECT_TRUE(unparsed.empty());
}

TEST(CommandLineParsing, Simple) {
  std::string option1_val = "not-set";
  std::string option2_val = "not-set";

  std::vector<OptionDescriptor> desc{
      {"--option1", "help-for-option1",
       [&](std::string const& v) { option1_val = v; }},
      {"--option2", "help-for-option2",
       [&](std::string const& v) { option2_val = v; }},
  };

  auto unparsed =
      OptionsParse(desc, {"command-name", "skip1", "--option2=value2", "skip2",
                          "skip3", "--option1=value1", "skip4", "skip5"});

  EXPECT_THAT(unparsed, ::testing::ElementsAre("command-name", "skip1", "skip2",
                                               "skip3", "skip4", "skip5"));
  EXPECT_EQ(option1_val, "value1");
  EXPECT_EQ(option2_val, "value2");
}

TEST(CommandLineParsing, PrefixArgument) {
  std::string option1_with_suffix_val = "not-set";
  std::string option1_val = "not-set";

  std::vector<OptionDescriptor> desc{
      {"--option1-with-suffix", "help-for-option1-with-suffix",
       [&](std::string const& v) { option1_with_suffix_val = v; }},
      {"--option1", "help-for-option1",
       [&](std::string const& v) { option1_val = v; }},
  };

  auto unparsed =
      OptionsParse(desc, {"command-name", "--option1-with-suffix=suffix1",
                          "skip1", "skip2", "--option1=value1"});

  EXPECT_THAT(unparsed,
              ::testing::ElementsAre("command-name", "skip1", "skip2"));
  EXPECT_EQ(option1_with_suffix_val, "suffix1");
  EXPECT_EQ(option1_val, "value1");
}

TEST(CommandLineParsing, FormatSize) {
  EXPECT_EQ("1023.0B", FormatSize(1023));
  EXPECT_EQ("1.0KiB", FormatSize(kKiB));
  EXPECT_EQ("1.1KiB", FormatSize(kKiB + 100));
  EXPECT_EQ("1.0MiB", FormatSize(kMiB));
  EXPECT_EQ("1.0GiB", FormatSize(kGiB));
  EXPECT_EQ("1.1GiB", FormatSize(kGiB + 128 * kMiB));
  EXPECT_EQ("1.0TiB", FormatSize(kTiB));
  EXPECT_EQ("2.0TiB", FormatSize(2 * kTiB));
}

}  // namespace
}  // namespace testing_util
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

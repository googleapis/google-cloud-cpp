// Copyright 2019 Google LLC
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

#include "google/cloud/storage/benchmarks/benchmark_utils.h"
#include <gmock/gmock.h>

namespace google {
namespace cloud {
namespace storage_benchmarks {
namespace {

using ::testing::HasSubstr;

TEST(StorageBenchmarksParseArgsTest, UsageSimple) {
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

TEST(StorageBenchmarksParseArgsTest, Empty) {
  OptionDescriptor d{"--unused", "should not be called",
                     [](std::string const& val) { FAIL() << "value=" << val; }};
  auto unparsed = OptionsParse({d}, {});
  EXPECT_TRUE(unparsed.empty());
}

TEST(StorageBenchmarksParseArgsTest, Simple) {
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

TEST(StorageBenchmarksParseArgsTest, PrefixArgument) {
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

}  // namespace
}  // namespace storage_benchmarks
}  // namespace cloud
}  // namespace google

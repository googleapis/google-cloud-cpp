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

#include "google/cloud/options.h"
#include "google/cloud/testing_util/scoped_log.h"
#include <gmock/gmock.h>
#include <set>
#include <string>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::UnorderedElementsAre;

struct IntOption {
  using Type = int;
};

struct BoolOption {
  using Type = bool;
};

struct StringOption {
  using Type = std::string;
};

using TestOptionList = OptionList<IntOption, BoolOption, StringOption>;

// This is how customers should set a simple options.
TEST(OptionsUseCase, CustomerSettingSimpleOptions) {
  auto const opts = Options{}.set<IntOption>(123).set<BoolOption>(true);

  EXPECT_TRUE(opts.has<IntOption>());
  EXPECT_TRUE(opts.has<BoolOption>());
}

// This is how customers should append to an option.
TEST(OptionsUseCase, CustomerSettingComplexOption) {
  struct ComplexOption {
    using Type = std::set<std::string>;
  };

  Options opts;

  EXPECT_FALSE(opts.has<ComplexOption>());
  opts.lookup<ComplexOption>().insert("foo");
  EXPECT_TRUE(opts.has<ComplexOption>());
  opts.lookup<ComplexOption>().insert("bar");

  EXPECT_THAT(opts.lookup<ComplexOption>(), UnorderedElementsAre("foo", "bar"));
}

TEST(Options, Has) {
  Options opts;
  EXPECT_FALSE(opts.has<IntOption>());
  opts.set<IntOption>(42);
  EXPECT_TRUE(opts.has<IntOption>());
}

TEST(Options, Set) {
  Options opts;
  opts.set<IntOption>({});
  EXPECT_TRUE(opts.has<IntOption>());
  EXPECT_EQ(0, opts.get<IntOption>());
  opts.set<IntOption>(123);
  EXPECT_EQ(123, opts.get<IntOption>());

  opts = Options{};
  opts.set<BoolOption>({});
  EXPECT_TRUE(opts.has<BoolOption>());
  EXPECT_EQ(false, opts.get<BoolOption>());
  opts.set<BoolOption>(true);
  EXPECT_EQ(true, opts.get<BoolOption>());

  opts = Options{};
  opts.set<StringOption>({});
  EXPECT_TRUE(opts.has<StringOption>());
  EXPECT_EQ("", opts.get<StringOption>());
  opts.set<StringOption>("foo");
  EXPECT_EQ("foo", opts.get<StringOption>());
}

TEST(Options, Get) {
  Options opts;

  int const& i = opts.get<IntOption>();
  EXPECT_EQ(0, i);
  opts.set<IntOption>(42);
  EXPECT_EQ(42, opts.get<IntOption>());

  std::string const& s = opts.get<StringOption>();
  EXPECT_TRUE(s.empty());
  opts.set<StringOption>("test");
  EXPECT_EQ("test", opts.get<StringOption>());
}

TEST(Options, Lookup) {
  Options opts;

  // Lookup with value-initialized default.
  EXPECT_FALSE(opts.has<IntOption>());
  int& x = opts.lookup<IntOption>();
  EXPECT_TRUE(opts.has<IntOption>());
  EXPECT_EQ(0, x);  // Value initialized int.
  x = 42;           // Sets x within the Options
  EXPECT_EQ(42, opts.lookup<IntOption>());

  // Lookup with user-supplied default value.
  opts.unset<IntOption>();
  EXPECT_FALSE(opts.has<IntOption>());
  EXPECT_EQ(42, opts.lookup<IntOption>(42));
  EXPECT_TRUE(opts.has<IntOption>());
}

TEST(Options, Copy) {
  auto a = Options{}.set<IntOption>(42).set<BoolOption>(true).set<StringOption>(
      "foo");

  auto copy = a;  // NOLINT(performance-unnecessary-copy-initialization)
  EXPECT_TRUE(copy.has<IntOption>());
  EXPECT_TRUE(copy.has<BoolOption>());
  EXPECT_TRUE(copy.has<StringOption>());

  EXPECT_EQ(42, copy.get<IntOption>());
  EXPECT_EQ(true, copy.get<BoolOption>());
  EXPECT_EQ("foo", copy.get<StringOption>());
}

TEST(Options, Move) {
  auto a = Options{}.set<IntOption>(42).set<BoolOption>(true).set<StringOption>(
      "foo");

  auto moved = std::move(a);
  EXPECT_TRUE(moved.has<IntOption>());
  EXPECT_TRUE(moved.has<BoolOption>());
  EXPECT_TRUE(moved.has<StringOption>());

  EXPECT_EQ(42, moved.get<IntOption>());
  EXPECT_EQ(true, moved.get<BoolOption>());
  EXPECT_EQ("foo", moved.get<StringOption>());
}

TEST(CheckUnexpectedOptions, Empty) {
  testing_util::ScopedLog log;
  Options opts;
  internal::CheckExpectedOptions<BoolOption>(opts, "caller");
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, OneExpected) {
  testing_util::ScopedLog log;
  Options opts;
  opts.set<BoolOption>({});
  internal::CheckExpectedOptions<BoolOption>(opts, "caller");
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, TwoExpected) {
  testing_util::ScopedLog log;
  Options opts;
  opts.set<BoolOption>({});
  opts.set<IntOption>({});
  internal::CheckExpectedOptions<BoolOption, IntOption>(opts, "caller");
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, FullishLogLine) {
  testing_util::ScopedLog log;
  Options opts;
  opts.set<IntOption>({});
  internal::CheckExpectedOptions<BoolOption>(opts, "caller");
  // This tests exists just to show us what a full log line may look like.
  // The regex hides the nastiness of the actual mangled name.
  EXPECT_THAT(
      log.ExtractLines(),
      Contains(ContainsRegex(
          R"(caller: Unexpected option \(mangled name\): .+IntOption)")));
}

TEST(CheckUnexpectedOptions, OneUnexpected) {
  testing_util::ScopedLog log;
  Options opts;
  opts.set<IntOption>({});
  internal::CheckExpectedOptions<BoolOption>(opts, "caller");
  EXPECT_THAT(log.ExtractLines(),
              Contains(ContainsRegex("caller: Unexpected option.+IntOption")));
}

TEST(CheckUnexpectedOptions, TwoUnexpected) {
  testing_util::ScopedLog log;
  Options opts;
  opts.set<IntOption>({});
  opts.set<StringOption>({});
  internal::CheckExpectedOptions<BoolOption>(opts, "caller");
  EXPECT_THAT(
      log.ExtractLines(),
      AllOf(
          Contains(ContainsRegex("caller: Unexpected option.+IntOption")),
          Contains(ContainsRegex("caller: Unexpected option.+StringOption"))));
}

TEST(CheckUnexpectedOptions, BasicOptionsList) {
  testing_util::ScopedLog log;
  Options opts;
  opts.set<IntOption>({});
  opts.set<StringOption>({});
  internal::CheckExpectedOptions<TestOptionList>(opts, "caller");
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, OptionsListPlusOne) {
  struct FooOption {
    using Type = int;
  };
  testing_util::ScopedLog log;
  Options opts;
  opts.set<IntOption>({});
  opts.set<StringOption>({});
  opts.set<FooOption>({});
  internal::CheckExpectedOptions<FooOption, TestOptionList>(opts, "caller");
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, OptionsListOneUnexpected) {
  struct FooOption {
    using Type = int;
  };
  testing_util::ScopedLog log;
  Options opts;
  opts.set<IntOption>({});
  opts.set<StringOption>({});
  opts.set<FooOption>({});
  internal::CheckExpectedOptions<TestOptionList>(opts, "caller");
  EXPECT_THAT(log.ExtractLines(),
              Contains(ContainsRegex("caller: Unexpected option.+FooOption")));
}

TEST(MergeOptions, Basics) {
  auto a = Options{}.set<StringOption>("from a").set<IntOption>(42);
  auto b = Options{}.set<StringOption>("from b").set<BoolOption>(true);
  a = internal::MergeOptions(std::move(a), std::move(b));
  EXPECT_EQ(a.get<StringOption>(), "from a");  // From a
  EXPECT_EQ(a.get<BoolOption>(), true);        // From b
  EXPECT_EQ(a.get<IntOption>(), 42);           // From a
}

}  // namespace
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

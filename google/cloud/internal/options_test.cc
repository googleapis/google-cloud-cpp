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

#include "google/cloud/internal/options.h"
#include "google/cloud/testing_util/scoped_log.h"
#include <gmock/gmock.h>
#include <set>
#include <string>
#include <tuple>

namespace google {
namespace cloud {
inline namespace GOOGLE_CLOUD_CPP_NS {
namespace internal {
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

using TestOptionsTuple = std::tuple<IntOption, BoolOption, StringOption>;

// This is how customers should set a simple options.
TEST(OptionsUseCase, CustomerSettingSimpleOptions) {
  auto opts = Options{}.set<IntOption>(123).set<BoolOption>(true);

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

// This is how our factory functions should get options.
TEST(OptionsUseCase, FactoriesGettingOptions) {
  auto factory = [](Options const& opts) {
    EXPECT_EQ(123, opts.get_or<IntOption>(123));
    EXPECT_EQ("set-by-customer", opts.get_or<StringOption>({}));
  };

  auto opts = Options{}.set<StringOption>("set-by-customer");
  factory(opts);
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
  EXPECT_EQ(0, opts.get_or<IntOption>(-1));
  opts.set<IntOption>(123);
  EXPECT_EQ(123, opts.get_or<IntOption>(-1));

  opts = Options{};
  opts.set<BoolOption>({});
  EXPECT_TRUE(opts.has<BoolOption>());
  EXPECT_EQ(false, opts.get_or<BoolOption>(true));
  opts.set<BoolOption>(true);
  EXPECT_EQ(true, opts.get_or<BoolOption>(false));

  opts = Options{};
  opts.set<StringOption>({});
  EXPECT_TRUE(opts.has<StringOption>());
  EXPECT_EQ("", opts.get_or<StringOption>("default"));
  opts.set<StringOption>("foo");
  EXPECT_EQ("foo", opts.get_or<StringOption>("default"));
}

TEST(Options, GetOr) {
  Options opts;
  EXPECT_EQ(opts.get_or<IntOption>({}), 0);
  EXPECT_EQ(opts.get_or<IntOption>(42), 42);

  EXPECT_EQ(opts.get_or<BoolOption>({}), false);
  EXPECT_EQ(opts.get_or<BoolOption>(true), true);

  EXPECT_EQ(opts.get_or<StringOption>({}), "");
  EXPECT_EQ(opts.get_or<StringOption>("foo"), "foo");
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

  EXPECT_EQ(42, copy.get_or<IntOption>({}));
  EXPECT_EQ(true, copy.get_or<BoolOption>({}));
  EXPECT_EQ("foo", copy.get_or<StringOption>({}));
}

TEST(Options, Move) {
  auto a = Options{}.set<IntOption>(42).set<BoolOption>(true).set<StringOption>(
      "foo");

  auto moved = std::move(a);
  EXPECT_TRUE(moved.has<IntOption>());
  EXPECT_TRUE(moved.has<BoolOption>());
  EXPECT_TRUE(moved.has<StringOption>());

  EXPECT_EQ(42, moved.get_or<IntOption>({}));
  EXPECT_EQ(true, moved.get_or<BoolOption>({}));
  EXPECT_EQ("foo", moved.get_or<StringOption>({}));
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
  internal::CheckExpectedOptions<TestOptionsTuple>(opts, "caller");
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
  internal::CheckExpectedOptions<FooOption, TestOptionsTuple>(opts, "caller");
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
  internal::CheckExpectedOptions<TestOptionsTuple>(opts, "caller");
  EXPECT_THAT(log.ExtractLines(),
              Contains(ContainsRegex("caller: Unexpected option.+FooOption")));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

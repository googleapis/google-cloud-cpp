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

struct IntOption {
  int value;
};

struct BoolOption {
  bool value;
};

struct StringOption {
  std::string value;
};

struct DefaultedOption {
  int value = 123;
};

using TestOptionsTuple = std::tuple<IntOption, BoolOption, StringOption>;

TEST(Options, Has) {
  Options opts;
  EXPECT_FALSE(opts.has<IntOption>());
  opts.set<IntOption>(42);
  EXPECT_TRUE(opts.has<IntOption>());
}

TEST(Options, Set) {
  Options opts;
  opts.set<IntOption>();
  EXPECT_TRUE(opts.has<IntOption>());
  EXPECT_EQ(0, opts.get_or<IntOption>(-1));
  opts.set<IntOption>(123);
  EXPECT_EQ(123, opts.get_or<IntOption>(-1));
  const IntOption default_int{42};
  opts.set<IntOption>(default_int);
  EXPECT_EQ(42, opts.get_or<IntOption>(-1));

  opts = Options{};
  opts.set<BoolOption>();
  EXPECT_TRUE(opts.has<BoolOption>());
  EXPECT_EQ(false, opts.get_or<BoolOption>(true));
  opts.set<BoolOption>(true);
  EXPECT_EQ(true, opts.get_or<BoolOption>(false));
  const BoolOption default_bool{true};
  opts.set<BoolOption>(default_bool);
  EXPECT_EQ(true, opts.get_or<BoolOption>(false));

  opts = Options{};
  opts.set<StringOption>();
  EXPECT_TRUE(opts.has<StringOption>());
  EXPECT_EQ("", opts.get_or<StringOption>("default"));
  opts.set<StringOption>("foo");
  EXPECT_EQ("foo", opts.get_or<StringOption>("default"));
  const StringOption default_string{"foo"};
  opts.set<StringOption>(default_string);
  EXPECT_EQ("foo", opts.get_or<StringOption>("default"));

  opts = Options{};
  opts.set<DefaultedOption>();
  EXPECT_TRUE(opts.has<DefaultedOption>());
  EXPECT_EQ(123, opts.get_or<DefaultedOption>(-1));
  opts.set<DefaultedOption>(42);
  EXPECT_EQ(42, opts.get_or<DefaultedOption>(-1));
  DefaultedOption default_default;
  default_default.value = 42;
  opts.set<DefaultedOption>(default_default);
  EXPECT_EQ(42, opts.get_or<DefaultedOption>(-1));
}

TEST(Options, GetOr) {
  const IntOption default_int{42};
  const BoolOption default_bool{true};
  const StringOption default_string{"foo"};
  DefaultedOption default_default;
  default_default.value = 42;

  Options opts;
  EXPECT_EQ(opts.get_or<IntOption>(), 0);
  EXPECT_EQ(opts.get_or<IntOption>(42), 42);
  EXPECT_EQ(opts.get_or<IntOption>(default_int), 42);

  EXPECT_EQ(opts.get_or<BoolOption>(), false);
  EXPECT_EQ(opts.get_or<BoolOption>(true), true);
  EXPECT_EQ(opts.get_or<BoolOption>(default_bool), true);

  EXPECT_EQ(opts.get_or<StringOption>(), "");
  EXPECT_EQ(opts.get_or<StringOption>("foo"), "foo");
  EXPECT_EQ(opts.get_or<StringOption>(default_string), "foo");

  EXPECT_EQ(opts.get_or<DefaultedOption>(), 123);
  EXPECT_EQ(opts.get_or<DefaultedOption>(42), 42);
  EXPECT_EQ(opts.get_or<DefaultedOption>(default_default), 42);
}

TEST(Options, Lookup) {
  Options opts;

  // Lookup with value-initialized default.
  EXPECT_FALSE(opts.has<IntOption>());
  int& x = opts.lookup<IntOption>();
  EXPECT_TRUE(opts.has<IntOption>());
  EXPECT_EQ(0, x);  // Value initialized int.
  x = 42;  // Sets x within the Options
  EXPECT_EQ(42, opts.lookup<IntOption>());

  // Lookup with user-supplied default value.
  opts.unset<IntOption>();
  EXPECT_FALSE(opts.has<IntOption>());
  EXPECT_EQ(42, opts.lookup<IntOption>(42));
  EXPECT_TRUE(opts.has<IntOption>());

  // Lookup with user-supplied default IntOption
  const IntOption default_int{42};
  opts.unset<IntOption>();
  EXPECT_FALSE(opts.has<IntOption>());
  EXPECT_EQ(42, opts.lookup<IntOption>(default_int));
  EXPECT_TRUE(opts.has<IntOption>());
}

TEST(Options, Copy) {
  auto a = Options{}
               .set<IntOption>(42)
               .set<BoolOption>(true)
               .set<StringOption>("foo")
               .set<DefaultedOption>();

  auto copy = a;  // NOLINT(performance-unnecessary-copy-initialization)
  EXPECT_TRUE(copy.has<IntOption>());
  EXPECT_TRUE(copy.has<BoolOption>());
  EXPECT_TRUE(copy.has<StringOption>());
  EXPECT_TRUE(copy.has<DefaultedOption>());

  EXPECT_EQ(42, copy.get_or<IntOption>());
  EXPECT_EQ(true, copy.get_or<BoolOption>());
  EXPECT_EQ("foo", copy.get_or<StringOption>());
  EXPECT_EQ(123, copy.get_or<DefaultedOption>());
}

TEST(Options, Move) {
  auto a = Options{}
               .set<IntOption>(42)
               .set<BoolOption>(true)
               .set<StringOption>("foo")
               .set<DefaultedOption>();

  auto moved = std::move(a);
  EXPECT_TRUE(moved.has<IntOption>());
  EXPECT_TRUE(moved.has<BoolOption>());
  EXPECT_TRUE(moved.has<StringOption>());
  EXPECT_TRUE(moved.has<DefaultedOption>());

  EXPECT_EQ(42, moved.get_or<IntOption>());
  EXPECT_EQ(true, moved.get_or<BoolOption>());
  EXPECT_EQ("foo", moved.get_or<StringOption>());
  EXPECT_EQ(123, moved.get_or<DefaultedOption>());
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
  opts.set<BoolOption>();
  internal::CheckExpectedOptions<BoolOption>(opts, "caller");
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, TwoExpected) {
  testing_util::ScopedLog log;
  Options opts;
  opts.set<BoolOption>();
  opts.set<IntOption>();
  internal::CheckExpectedOptions<BoolOption, IntOption>(opts, "caller");
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, FullishLogLine) {
  testing_util::ScopedLog log;
  Options opts;
  opts.set<IntOption>();
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
  opts.set<IntOption>();
  internal::CheckExpectedOptions<BoolOption>(opts, "caller");
  EXPECT_THAT(log.ExtractLines(),
              Contains(ContainsRegex("caller: Unexpected option.+IntOption")));
}

TEST(CheckUnexpectedOptions, TwoUnexpected) {
  testing_util::ScopedLog log;
  Options opts;
  opts.set<IntOption>();
  opts.set<StringOption>();
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
  opts.set<IntOption>();
  opts.set<StringOption>();
  internal::CheckExpectedOptions<TestOptionsTuple>(opts, "caller");
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, OptionsListPlusOne) {
  struct FooOption {
    int value;
  };
  testing_util::ScopedLog log;
  Options opts;
  opts.set<IntOption>();
  opts.set<StringOption>();
  opts.set<FooOption>();
  internal::CheckExpectedOptions<FooOption, TestOptionsTuple>(opts, "caller");
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, OptionsListOneUnexpected) {
  struct FooOption {
    int value;
  };
  testing_util::ScopedLog log;
  Options opts;
  opts.set<IntOption>();
  opts.set<StringOption>();
  opts.set<FooOption>();
  internal::CheckExpectedOptions<TestOptionsTuple>(opts, "caller");
  EXPECT_THAT(log.ExtractLines(),
              Contains(ContainsRegex("caller: Unexpected option.+FooOption")));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

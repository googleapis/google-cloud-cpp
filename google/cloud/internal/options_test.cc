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
#include "google/cloud/testing_util/capture_log_lines_backend.h"
#include <gmock/gmock.h>
#include <string>

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

// Let's test an option struct that has a default value. In this case, the
// struct will no longer support aggregate initialization [1], so we'll need to
// add back a 1-arg and default constructors.
//
// [1]: https://en.cppreference.com/w/cpp/language/aggregate_initialization
struct DefaultedOption {
  explicit DefaultedOption(int v) : value(v) {}
  DefaultedOption() = default;
  int value = 123;
};

TEST(Options, Empty) {
  Options opts{};
  EXPECT_TRUE(opts.empty());
}

TEST(Options, GetWithDefault) {
  Options opts{};

  // Get the specified default value.
  EXPECT_EQ(opts.get_or<IntOption>(42).value, 42);
}

TEST(Options, DefaultedOption) {
  Options opts{};

  // Set doesn't need an argument since a default constructed option is fine.
  opts.set<DefaultedOption>();
  EXPECT_EQ(123, opts.get<DefaultedOption>()->value);

  opts.unset<DefaultedOption>();
  EXPECT_EQ(123, opts.get_or<DefaultedOption>().value);
  EXPECT_EQ(42, opts.get_or<DefaultedOption>(42).value);
}

TEST(Options, MutateOption) {
  Options opts{};
  opts.set<StringOption>("test1");
  EXPECT_EQ("test1", opts.get<StringOption>()->value);

  auto v = opts.get_or<StringOption>("");
  opts.set<StringOption>(v.value + ",test2");

  EXPECT_EQ("test1,test2", opts.get<StringOption>()->value);
}

TEST(Options, Copy) {
  auto a = Options{}.set<IntOption>(42).set<BoolOption>(true).set<StringOption>(
      "foo");

  auto copy = a;  // NOLINT(performance-unnecessary-copy-initialization)
  EXPECT_FALSE(copy.empty());
  EXPECT_TRUE(copy.get<IntOption>().has_value());
  EXPECT_TRUE(copy.get<BoolOption>().has_value());
  EXPECT_TRUE(copy.get<StringOption>().has_value());
  EXPECT_EQ(copy.get<IntOption>()->value, 42);
  EXPECT_EQ(copy.get<BoolOption>()->value, true);
  EXPECT_EQ(copy.get<StringOption>()->value, "foo");
}

TEST(Options, Move) {
  auto a = Options{}.set<IntOption>(42).set<BoolOption>(true).set<StringOption>(
      "foo");

  auto moved = std::move(a);
  EXPECT_FALSE(moved.empty());
  EXPECT_TRUE(moved.get<IntOption>().has_value());
  EXPECT_TRUE(moved.get<BoolOption>().has_value());
  EXPECT_TRUE(moved.get<StringOption>().has_value());
  EXPECT_EQ(moved.get<IntOption>()->value, 42);
  EXPECT_EQ(moved.get<BoolOption>()->value, true);
  EXPECT_EQ(moved.get<StringOption>()->value, "foo");
}

TEST(Options, BasicOperations) {
  Options opts{};
  EXPECT_TRUE(opts.empty());
  EXPECT_FALSE(opts.get<IntOption>().has_value());

  opts.set<IntOption>(42);
  EXPECT_FALSE(opts.empty());
  EXPECT_TRUE(opts.get<IntOption>().has_value());
  EXPECT_EQ(opts.get<IntOption>()->value, 42);

  opts.set<IntOption>(123);
  EXPECT_FALSE(opts.empty());
  EXPECT_TRUE(opts.get<IntOption>().has_value());
  EXPECT_EQ(opts.get<IntOption>()->value, 123);

  opts.set<BoolOption>(true).set<StringOption>("foo");
  EXPECT_FALSE(opts.empty());
  EXPECT_TRUE(opts.get<IntOption>().has_value());
  EXPECT_TRUE(opts.get<BoolOption>().has_value());
  EXPECT_TRUE(opts.get<StringOption>().has_value());
  EXPECT_EQ(opts.get<IntOption>()->value, 123);
  EXPECT_EQ(opts.get<BoolOption>()->value, true);
  EXPECT_EQ(opts.get<StringOption>()->value, "foo");

  opts.unset<IntOption>();
  EXPECT_FALSE(opts.empty());
  EXPECT_FALSE(opts.get<IntOption>().has_value());
  EXPECT_TRUE(opts.get<BoolOption>().has_value());
  EXPECT_TRUE(opts.get<StringOption>().has_value());
  EXPECT_EQ(opts.get<BoolOption>()->value, true);
  EXPECT_EQ(opts.get<StringOption>()->value, "foo");
}

// 
class ScopedLog {
  public:
   ScopedLog()
       : backend_(std::make_shared<testing_util::CaptureLogLinesBackend>()),
         id_(LogSink::Instance().AddBackend(backend_)) {}
   ~ScopedLog() { LogSink::Instance().RemoveBackend(id_); }

   std::vector<std::string> ExtractLines() { return backend_->ClearLogLines(); }

  private:
   std::shared_ptr<testing_util::CaptureLogLinesBackend> backend_;
   long id_;  // NOLINT(google-runtime-int)
};

TEST(CheckUnexpectedOptions, Empty) {
  ScopedLog log;
  Options opts;
  internal::WarnUnexpectedOptions<BoolOption>(opts);
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, OneExpected) {
  ScopedLog log;
  Options opts;
  opts.set<BoolOption>();
  internal::WarnUnexpectedOptions<BoolOption>(opts);
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, TwoExpected) {
  ScopedLog log;
  Options opts;
  opts.set<BoolOption>();
  opts.set<IntOption>();
  internal::WarnUnexpectedOptions<BoolOption, IntOption>(opts);
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CheckUnexpectedOptions, OneUnexpected) {
  ScopedLog log;
  Options opts;
  opts.set<IntOption>();
  internal::WarnUnexpectedOptions<BoolOption>(opts);
  EXPECT_THAT(log.ExtractLines(),
              Contains(ContainsRegex("Unexpected option.+IntOption")));
}

TEST(CheckUnexpectedOptions, TwoUnexpected) {
  ScopedLog log;
  Options opts;
  opts.set<IntOption>();
  opts.set<StringOption>();
  internal::WarnUnexpectedOptions<BoolOption>(opts);
  EXPECT_THAT(
      log.ExtractLines(),
      AllOf(Contains(ContainsRegex("Unexpected option.+IntOption")),
            Contains(ContainsRegex("Unexpected option.+StringOption"))));
}

}  // namespace
}  // namespace internal
}  // namespace GOOGLE_CLOUD_CPP_NS
}  // namespace cloud
}  // namespace google

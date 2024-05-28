// Copyright 2021 Google LLC
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

#include "google/cloud/internal/disable_deprecation_warnings.inc"
#include "google/cloud/common_options.h"
#include "google/cloud/options.h"
#include "google/cloud/testing_util/scoped_log.h"
#include <gmock/gmock.h>
#include <string>
#include <utility>

namespace google {
namespace cloud {
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_BEGIN
namespace {

using ::testing::Contains;
using ::testing::ContainsRegex;
using ::testing::UnorderedElementsAre;

// Tests a generic option by setting it, then getting it.
template <typename T, typename ValueType = typename T::Type>
void TestOption(ValueType const& expected) {
  auto opts = Options{}.template set<T>(expected);
  EXPECT_EQ(expected, opts.template get<T>())
      << "Failed with type: " << typeid(T).name();
}

TEST(CommonOptionList, RegularOptions) {
  TestOption<EndpointOption>("foo.googleapis.com");
  TestOption<UserAgentProductsOption>({"foo", "bar"});
  TestOption<LoggingComponentsOption>({"foo", "bar", "baz"});
}

TEST(CommonOptionList, Expected) {
  testing_util::ScopedLog log;
  Options opts;
  opts.set<EndpointOption>("foo.googleapis.com");
  internal::CheckExpectedOptions<CommonOptionList>(opts, "caller");
  EXPECT_TRUE(log.ExtractLines().empty());
}

TEST(CommonOptionList, Unexpected) {
  struct UnexpectedOption {
    using Type = int;
  };
  testing_util::ScopedLog log;
  Options opts;
  opts.set<UnexpectedOption>({});
  internal::CheckExpectedOptions<CommonOptionList>(opts, "caller");
  EXPECT_THAT(
      log.ExtractLines(),
      Contains(ContainsRegex("caller: Unexpected option.+UnexpectedOption")));
}

TEST(TracingComponentsOption, BackwardsCompat) {
  auto o = Options{}.set<TracingComponentsOption>({"a", "b", "c"});
  EXPECT_THAT(o.get<LoggingComponentsOption>(),
              UnorderedElementsAre("a", "b", "c"));
}

}  // namespace
GOOGLE_CLOUD_CPP_INLINE_NAMESPACE_END
}  // namespace cloud
}  // namespace google
